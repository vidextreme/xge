// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "MeshRenderSystemImpl.h"
#include "MaterialResources.h"
#include "xgTransforms.h"

namespace xg
{
    namespace
    {
        constexpr uint64_t MeshRendererLayoutHash = 0x6d72'0001'0028ull;

        bool SameGuid(const ResourceGUID& a, const ResourceGUID& b)
        {
            return a.Hi == b.Hi && a.Lo == b.Lo;
        }

        void Multiply(const float* a, const float* b, float* out)
        {
            for (int row = 0; row < 4; ++row)
                for (int column = 0; column < 4; ++column)
                {
                    float sum = 0.0f;
                    for (int k = 0; k < 4; ++k)
                        sum += a[row * 4 + k] * b[k * 4 + column];
                    out[row * 4 + column] = sum;
                }
        }
    }

    MeshRenderSystemImpl::MeshRenderSystemImpl(ComponentDataRegistry* registry, ResourceSystem* resources, TransformSystem* transforms, uint32_t ownerModuleId)
        : _registry(registry)
        , _resources(resources)
        , _transforms(transforms)
        , _ownerModuleId(ownerModuleId)
    {
        const DataTypeDesc desc = { MeshRendererDataKey, sizeof(MeshRendererData), alignof(uint64_t), MeshRendererLayoutHash, DataTypeFlags::None };
        _type = _registry->RegisterType(ownerModuleId, &desc);
        _listener = { nullptr, &MeshRenderSystemImpl::OnRemoved, &MeshRenderSystemImpl::OnMoved, this };
        _registry->AddListener(_type, &_listener);
    }

    MeshRenderSystemImpl::~MeshRenderSystemImpl() = default;

    void MeshRenderSystemImpl::Shutdown()
    {
        Detach();
    }

    void MeshRenderSystemImpl::Detach()
    {
        if (_registry)
            _registry->RemoveListener(_type, &_listener);
        ResetAll();
        _registry = nullptr;
        _resources = nullptr;
        _transforms = nullptr;
    }

    void MeshRenderSystemImpl::ReleaseState(SlotState& state)
    {
        if (_resources)
        {
            if (state.Model != 0)
                _resources->Release(_ownerModuleId, state.Model);
            if (state.Material != 0)
                _resources->Release(_ownerModuleId, state.Material);
        }
        state = SlotState();
    }

    void MeshRenderSystemImpl::ResetAll()
    {
        for (SlotState& state : _slots)
            ReleaseState(state);
        _slots.clear();
    }

    void MeshRenderSystemImpl::OnRemoved(void* user, ActorID, uint32_t slot)
    {
        auto* self = static_cast<MeshRenderSystemImpl*>(user);
        if (slot < self->_slots.size())
            self->ReleaseState(self->_slots[slot]);
    }

    void MeshRenderSystemImpl::OnMoved(void* user, ActorID, uint32_t from, uint32_t to)
    {
        auto* self = static_cast<MeshRenderSystemImpl*>(user);
        if (from >= self->_slots.size())
            return;
        if (to >= self->_slots.size())
            self->_slots.resize(static_cast<size_t>(to) + 1);
        self->_slots[to] = self->_slots[from];
        self->_slots[from] = SlotState();
    }

    void MeshRenderSystemImpl::Track(SlotState& state, const MeshRendererData& data)
    {
        if (!SameGuid(state.ModelGuid, data.Model) || (state.Model != 0 && _resources->IsStale(state.Model)))
        {
            if (state.Model != 0)
                _resources->Release(_ownerModuleId, state.Model);
            state.Model = 0;
            state.ModelGuid = data.Model;
            if (!IsZero(data.Model) && _modelType != 0)
            {
                const ResourceID id{ data.Model, {} };
                state.Model = _resources->Acquire(_ownerModuleId, _modelType, &id);
            }
        }
        if (!SameGuid(state.MaterialGuid, data.Material) || (state.Material != 0 && _resources->IsStale(state.Material)))
        {
            if (state.Material != 0)
                _resources->Release(_ownerModuleId, state.Material);
            state.Material = 0;
            state.MaterialGuid = data.Material;
            if (!IsZero(data.Material) && _materialType != 0)
            {
                const ResourceID id{ data.Material, {} };
                state.Material = _resources->Acquire(_ownerModuleId, _materialType, &id);
            }
        }
    }

    uint64_t MeshRenderSystemImpl::ResolveMaterial(ResourceHandle material)
    {
        return _resources && _resources->GetState(material) == ResourceState::Ready ? xg::ResolveMaterial(material) : 0;
    }

    void MeshRenderSystemImpl::Update(Renderer* renderer, const std::vector<SceneDraw>& before)
    {
        if (!_registry || !_resources)
            return;
        if (renderer != _renderer)
        {
            for (SlotState& state : _slots)
                state = SlotState();
            _slots.clear();
            _renderer = renderer;
            _modelType = 0;
            _materialType = 0;
        }
        _draws.assign(before.begin(), before.end());
        _drawCount = 0;
        _materialDrawCount = 0;
        if (!renderer)
            return;
        if (_modelType == 0)
            _modelType = _resources->FindType(ModelResourceType);
        if (_materialType == 0)
            _materialType = _resources->FindType(MaterialResourceType);

        const ActorID* owners = nullptr;
        uint32_t count = 0;
        auto* elements = static_cast<const MeshRendererData*>(_registry->GetDense(_type, &owners, &count));
        if (_slots.size() < count)
            _slots.resize(count);
        for (uint32_t i = 0; i < count && elements; ++i)
        {
            const MeshRendererData& data = elements[i];
            SlotState& state = _slots[i];
            Track(state, data);
            if ((static_cast<uint32_t>(data.Flags) & static_cast<uint32_t>(MeshRendererFlags::Hidden)) != 0 || state.Model == 0
                || _resources->GetState(state.Model) != ResourceState::Ready)
                continue;
            const auto* model = static_cast<const ModelResourceInfo*>(_resources->GetData(state.Model));
            if (!model || !model->Parts)
                continue;
            float actorWorld[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
            if (_transforms && _transforms->Has(owners[i]))
                _transforms->GetWorldMatrix(owners[i], actorWorld);
            uint64_t material = 0;
            if (state.Material != 0 && _resources->GetState(state.Material) == ResourceState::Ready)
                material = ResolveMaterial(state.Material);
            for (int32_t part = 0; part < model->PartCount; ++part)
            {
                SceneDraw draw = {};
                draw.Mesh = model->Parts[part].Mesh;
                draw.Material = material;
                Multiply(model->Parts[part].World, actorWorld, draw.World);
                _draws.push_back(draw);
                if (material != 0)
                    ++_materialDrawCount;
            }
        }
        _drawCount = static_cast<int32_t>(_draws.size() - before.size());
        renderer->SetSceneDraws(_draws.data(), static_cast<uint32_t>(_draws.size()));
    }
}
