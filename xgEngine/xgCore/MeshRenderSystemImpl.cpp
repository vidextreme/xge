// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "MeshRenderSystemImpl.h"
#include "MaterialResources.h"
#include "ModelResources.h"
#include "xgTransforms.h"

namespace xg
{
    namespace
    {
        constexpr uint64_t MeshRendererLayoutHash = 0x6d72'0002'01a8ull;

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
            for (ResourceHandle material : state.Materials)
                if (material != 0)
                    _resources->Release(_ownerModuleId, material);
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
        TrackMaterial(state, 0, data.Material);
        const uint32_t submeshes = (std::min)(data.SubmeshCount, MeshRendererMaxSubmeshes);
        for (uint32_t i = 0; i < MeshRendererMaxSubmeshes; ++i)
            TrackMaterial(state, i + 1, i < submeshes ? data.Submeshes[i].Material : ResourceGUID{});
    }

    void MeshRenderSystemImpl::TrackMaterial(SlotState& state, uint32_t index, const ResourceGUID& guid)
    {
        ResourceHandle& handle = state.Materials[index];
        if (SameGuid(state.MaterialGuids[index], guid) && (handle == 0 || !_resources->IsStale(handle)))
            return;
        if (handle != 0)
            _resources->Release(_ownerModuleId, handle);
        handle = 0;
        state.MaterialGuids[index] = guid;
        if (!IsZero(guid) && _materialType != 0)
        {
            const ResourceID id{ guid, {} };
            handle = _resources->Acquire(_ownerModuleId, _materialType, &id);
        }
    }

    uint64_t MeshRenderSystemImpl::DrawMaterial(const SlotState& state, uint32_t index, const ResourceGUID& guid, ActorID owner)
    {
        if (!_overrides.empty() && !IsZero(guid))
        {
            auto found = _overrides.find(owner);
            if (found != _overrides.end() && SameGuid(found->second.Material, guid))
                return found->second.RendererMaterial;
        }
        if (state.Materials[index] != 0)
            return ResolveMaterial(state.Materials[index]);
        return IsZero(guid) ? xg::ResolveDefaultMaterial() : 0;
    }

    void MeshRenderSystemImpl::SetEditorMaterialOverride(ActorID actor, const ResourceGUID* material, uint64_t rendererMaterial)
    {
        if (!material || IsZero(*material) || rendererMaterial == 0)
            _overrides.erase(actor);
        else
            _overrides[actor] = { *material, rendererMaterial };
    }

    uint64_t MeshRenderSystemImpl::ResolveMaterial(ResourceHandle material)
    {
        if (!_resources)
            return 0;
        const uint64_t resolved = _resources->GetState(material) == ResourceState::Ready ? xg::ResolveMaterial(material) : 0;
        return resolved == 0 && xg::IsMaterialBroken(material) ? xg::ResolveErrorMaterial() : resolved;
    }

    void MeshRenderSystemImpl::SetEditorHidden(const ActorID* actors, uint32_t count)
    {
        _editorHidden.clear();
        for (uint32_t i = 0; actors && i < count; ++i)
            _editorHidden.insert(actors[i]);
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
            const uint32_t submeshes = (std::min)(data.SubmeshCount, MeshRendererMaxSubmeshes);
            const ResourceGUID* items = submeshes != 0 ? GetModelPartItems(model) : nullptr;
            const uint64_t fallback = DrawMaterial(state, 0, data.Material, owners[i]);
            const SceneDrawFlags flags = IsEditorHidden(owners[i]) ? SceneDrawFlags::EditorHidden : SceneDrawFlags::None;
            for (int32_t part = 0; part < model->PartCount; ++part)
            {
                uint64_t material = fallback;
                for (uint32_t s = 0; items && s < submeshes; ++s)
                {
                    const MeshRendererSubmesh& entry = data.Submeshes[s];
                    if (!SameGuid(entry.Submesh, items[part]))
                        continue;
                    if ((static_cast<uint32_t>(entry.Flags) & static_cast<uint32_t>(MeshRendererSubmeshFlags::Hidden)) != 0)
                        material = UINT64_MAX;
                    else
                        material = DrawMaterial(state, s + 1, entry.Material, owners[i]);
                    break;
                }
                if (material == UINT64_MAX)
                    continue;
                SceneDraw draw = {};
                draw.Mesh = model->Parts[part].Mesh;
                draw.Material = material;
                draw.Flags = flags;
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
