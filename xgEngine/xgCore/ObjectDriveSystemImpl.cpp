// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "ObjectDriveSystemImpl.h"
#include <cstring>
#include "MaterialResources.h"
#include "ModelResources.h"
#include "ObjectResources.h"
#include "xgHierarchyNode.h"
#include "xgSceneGraph.h"
#include "xgScriptEngine.h"
#include "xgTransforms.h"

namespace xg
{
    namespace
    {
        constexpr uint64_t ObjectDriverLayoutHash = 0x6f64'0001'0018ull;

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

    ObjectDriveSystemImpl::ObjectDriveSystemImpl(ScriptEngine* engine, ComponentDataRegistry* registry, ResourceSystem* resources, TransformSystem* transforms, uint32_t ownerModuleId)
        : _engine(engine)
        , _registry(registry)
        , _resources(resources)
        , _transforms(transforms)
        , _ownerModuleId(ownerModuleId)
    {
        const DataTypeDesc desc = { ObjectDriverDataKey, sizeof(ObjectDriverData), alignof(uint64_t), ObjectDriverLayoutHash, DataTypeFlags::None };
        _type = _registry->RegisterType(ownerModuleId, &desc);
        _listener = { nullptr, &ObjectDriveSystemImpl::OnRemoved, &ObjectDriveSystemImpl::OnMoved, this };
        _registry->AddListener(_type, &_listener);
    }

    ObjectDriveSystemImpl::~ObjectDriveSystemImpl() = default;

    void ObjectDriveSystemImpl::Shutdown()
    {
        Detach();
    }

    void ObjectDriveSystemImpl::Detach()
    {
        if (_registry)
            _registry->RemoveListener(_type, &_listener);
        ResetAll();
        _registry = nullptr;
        _resources = nullptr;
        _transforms = nullptr;
    }

    void ObjectDriveSystemImpl::ReleaseParts(SlotState& state)
    {
        if (_resources)
            for (PartState& part : state.Parts)
            {
                if (part.Model != 0)
                    _resources->Release(_ownerModuleId, part.Model);
                if (part.Material != 0)
                    _resources->Release(_ownerModuleId, part.Material);
            }
        state.Parts.clear();
        state.PartsAcquired = false;
    }

    SceneGraph* ObjectDriveSystemImpl::Graph() const
    {
        return _engine ? _engine->GetSubsystem<SceneGraph>() : nullptr;
    }

    void ObjectDriveSystemImpl::DespawnActors(SlotState& state)
    {
        SceneGraph* graph = Graph();
        for (auto it = state.Actors.rbegin(); it != state.Actors.rend(); ++it)
        {
            _owners.erase(it->Id);
            if (SceneNode* node = graph ? graph->GetNode(it->Id) : nullptr)
                graph->DestroyNode(node);
        }
        for (auto it = _pending.begin(); it != _pending.end();)
        {
            bool mine = false;
            for (const SpawnedActor& actor : state.Actors)
                mine = mine || actor.Id == it->Actor;
            it = mine ? _pending.erase(it) : it + 1;
        }
        state.Actors.clear();
        state.ActorsSpawned = false;
    }

    void ObjectDriveSystemImpl::SpawnActors(SlotState& state, ActorID owner, const ObjectResourceData& object)
    {
        state.ActorsSpawned = true;
        state.Owner = owner;
        SceneGraph* graph = Graph();
        if (!graph || !_transforms)
            return;
        std::vector<ActorID> byNode(object.Nodes.size(), 0);
        for (size_t i = 0; i < object.Nodes.size(); ++i)
        {
            const ObjectNode& node = object.Nodes[i];
            if (node.Expose != ObjectNodeExpose::Actor)
                continue;
            HierarchyNode* created = CreateNode<HierarchyNode>(*graph);
            const size_t length = node.Name.size();
            char* name = static_cast<char*>(std::malloc(length + 1));
            std::memcpy(name, node.Name.c_str(), length + 1);
            created->Name = name;
            created->OwnsName = true;
            const ActorID id = created->Id;
            byNode[i] = id;
            const ActorID parent = node.ActorParent >= 0 ? byNode[static_cast<size_t>(node.ActorParent)] : owner;
            _transforms->Add(id);
            _transforms->SetParent(id, parent, false);
            _transforms->SetLocal(id, &node.ActorPose);
            state.Actors.push_back({ id, static_cast<int32_t>(i) });
            _owners[id] = owner;
            _pending.push_back({ id, node.Components.empty() ? std::string("[]") : node.Components });
        }
    }

    void ObjectDriveSystemImpl::HoldActors(SlotState& state, const ObjectResourceData& object)
    {
        for (const SpawnedActor& actor : state.Actors)
        {
            const TransformData& pose = object.Nodes[static_cast<size_t>(actor.Node)].ActorPose;
            TransformData current = {};
            if (!_transforms->GetLocal(actor.Id, &current))
                continue;
            if (std::memcmp(&current, &pose, sizeof(TransformData)) != 0)
                _transforms->SetLocal(actor.Id, &pose);
        }
    }

    ActorID ObjectDriveSystemImpl::GetCompositionActor(ActorID driver, const char* name) const
    {
        if (!_registry || !_resources || !name)
            return 0;
        const int32_t slot = _registry->GetSlot(_type, driver);
        if (slot < 0 || static_cast<size_t>(slot) >= _slots.size())
            return 0;
        const SlotState& state = _slots[static_cast<size_t>(slot)];
        const ObjectResourceData* object = ReadyObject(state);
        if (!object)
            return 0;
        for (const SpawnedActor& actor : state.Actors)
            if (object->Nodes[static_cast<size_t>(actor.Node)].Name == name)
                return actor.Id;
        return 0;
    }

    ActorID ObjectDriveSystemImpl::GetCompositionOwner(ActorID actor) const
    {
        auto it = _owners.find(actor);
        return it == _owners.end() ? 0 : it->second;
    }

    int32_t ObjectDriveSystemImpl::TakeSpawned(ActorID* outActor, char* json, int32_t capacity)
    {
        while (!_pending.empty() && GetCompositionOwner(_pending.front().Actor) == 0)
            _pending.pop_front();
        if (_pending.empty())
            return -1;
        const PendingComponents& front = _pending.front();
        const int32_t length = static_cast<int32_t>(front.JSON.size());
        if (outActor)
            *outActor = front.Actor;
        if (json && capacity > length)
        {
            std::memcpy(json, front.JSON.c_str(), static_cast<size_t>(length) + 1);
            _pending.pop_front();
        }
        return length;
    }

    void ObjectDriveSystemImpl::ReleaseState(SlotState& state)
    {
        DespawnActors(state);
        ReleaseParts(state);
        if (_resources && state.Object != 0)
            _resources->Release(_ownerModuleId, state.Object);
        state = SlotState();
    }

    void ObjectDriveSystemImpl::ResetAll()
    {
        for (SlotState& state : _slots)
            ReleaseState(state);
        _slots.clear();
    }

    void ObjectDriveSystemImpl::OnRemoved(void* user, ActorID, uint32_t slot)
    {
        auto* self = static_cast<ObjectDriveSystemImpl*>(user);
        if (slot < self->_slots.size())
            self->ReleaseState(self->_slots[slot]);
    }

    void ObjectDriveSystemImpl::OnMoved(void* user, ActorID, uint32_t from, uint32_t to)
    {
        auto* self = static_cast<ObjectDriveSystemImpl*>(user);
        if (from >= self->_slots.size())
            return;
        if (to >= self->_slots.size())
            self->_slots.resize(static_cast<size_t>(to) + 1);
        self->_slots[to] = std::move(self->_slots[from]);
        self->_slots[from] = SlotState();
    }

    const ObjectResourceData* ObjectDriveSystemImpl::ReadyObject(const SlotState& state) const
    {
        if (state.Object == 0 || _resources->GetState(state.Object) != ResourceState::Ready)
            return nullptr;
        return static_cast<const ObjectResourceData*>(_resources->GetData(state.Object));
    }

    void ObjectDriveSystemImpl::RefreshPart(PartState& part, const ResourceGUID& model, const ResourceGUID& material)
    {
        if ((part.Model == 0 || _resources->IsStale(part.Model)) && !IsZero(model) && _modelType != 0)
        {
            if (part.Model != 0)
                _resources->Release(_ownerModuleId, part.Model);
            const ResourceID id{ model, {} };
            part.Model = _resources->Acquire(_ownerModuleId, _modelType, &id);
        }
        if ((part.Material == 0 || _resources->IsStale(part.Material)) && !IsZero(material) && _materialType != 0)
        {
            if (part.Material != 0)
                _resources->Release(_ownerModuleId, part.Material);
            const ResourceID id{ material, {} };
            part.Material = _resources->Acquire(_ownerModuleId, _materialType, &id);
        }
    }

    void ObjectDriveSystemImpl::Track(SlotState& state, const ObjectDriverData& data)
    {
        if (!SameGuid(state.ObjectGuid, data.Object) || (state.Object != 0 && _resources->IsStale(state.Object)))
        {
            DespawnActors(state);
            ReleaseParts(state);
            if (state.Object != 0)
                _resources->Release(_ownerModuleId, state.Object);
            state.Object = 0;
            state.ObjectGuid = data.Object;
            if (!IsZero(data.Object) && _objectType != 0)
            {
                const ResourceID id{ data.Object, {} };
                state.Object = _resources->Acquire(_ownerModuleId, _objectType, &id);
            }
        }
        const ObjectResourceData* object = ReadyObject(state);
        if (!object)
            return;
        if (!state.PartsAcquired)
        {
            state.Parts.resize(object->Parts.size());
            state.PartsAcquired = true;
        }
        for (size_t i = 0; i < object->Parts.size(); ++i)
            RefreshPart(state.Parts[i], object->Parts[i].Model, object->Parts[i].Material);
    }

    void ObjectDriveSystemImpl::ActorWorld(ActorID actor, float* out) const
    {
        static constexpr float Identity[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
        std::memcpy(out, Identity, sizeof(Identity));
        if (_transforms && _transforms->Has(actor))
            _transforms->GetWorldMatrix(actor, out);
    }

    bool ObjectDriveSystemImpl::GetNodeWorld(ActorID actor, const char* name, float* outWorld)
    {
        if (!_registry || !_resources || !name || !outWorld)
            return false;
        const int32_t slot = _registry->GetSlot(_type, actor);
        if (slot < 0 || static_cast<size_t>(slot) >= _slots.size())
            return false;
        const ObjectResourceData* object = ReadyObject(_slots[static_cast<size_t>(slot)]);
        if (!object)
            return false;
        const uint32_t hash = HashObjectName(name);
        const ObjectNode* found = nullptr;
        for (const ObjectNode& node : object->Nodes)
        {
            if (node.NameHash != hash || node.Name != name)
                continue;
            if (!found || node.Expose == ObjectNodeExpose::Proxy)
                found = &node;
            if (node.Expose == ObjectNodeExpose::Proxy)
                break;
        }
        if (!found)
            return false;
        float actorWorld[16];
        ActorWorld(actor, actorWorld);
        Multiply(found->Baked, actorWorld, outWorld);
        return true;
    }

    bool ObjectDriveSystemImpl::GetModelPartItem(ResourceHandle model, int32_t index, ResourceGUID* outItem) const
    {
        if (!_resources || !outItem || _resources->GetState(model) != ResourceState::Ready)
            return false;
        const auto* info = static_cast<const ModelResourceInfo*>(_resources->GetData(model));
        const ResourceGUID* items = GetModelPartItems(info);
        if (!info || !items || index < 0 || index >= info->PartCount)
            return false;
        *outItem = items[index];
        return true;
    }

    void ObjectDriveSystemImpl::Update(Renderer* renderer, std::vector<SceneDraw>& draws, const MeshRenderSystem* hidden)
    {
        _drawCount = 0;
        _readyCount = 0;
        if (!_registry || !_resources)
            return;
        if (renderer != _renderer)
        {
            ResetAll();
            _renderer = renderer;
            _objectType = 0;
            _modelType = 0;
            _materialType = 0;
        }
        if (!renderer)
            return;
        if (_objectType == 0)
            _objectType = _resources->FindType(ObjectResourceType);
        if (_modelType == 0)
            _modelType = _resources->FindType(ModelResourceType);
        if (_materialType == 0)
            _materialType = _resources->FindType(MaterialResourceType);

        const size_t first = draws.size();
        const ActorID* owners = nullptr;
        uint32_t count = 0;
        auto* elements = static_cast<const ObjectDriverData*>(_registry->GetDense(_type, &owners, &count));
        if (_slots.size() < count)
            _slots.resize(count);
        for (uint32_t i = 0; i < count && elements; ++i)
        {
            const ObjectDriverData& data = elements[i];
            SlotState& state = _slots[i];
            Track(state, data);
            const ObjectResourceData* object = ReadyObject(state);
            if (!object)
                continue;
            ++_readyCount;
            if (state.ActorsSpawned && state.Owner != owners[i])
                DespawnActors(state);
            if (!state.ActorsSpawned)
                SpawnActors(state, owners[i], *object);
            else if (_transforms)
                HoldActors(state, *object);
            if ((static_cast<uint32_t>(data.Flags) & static_cast<uint32_t>(ObjectDriverFlags::Hidden)) != 0)
                continue;
            float actorWorld[16];
            ActorWorld(owners[i], actorWorld);
            const SceneDrawFlags flags = hidden && hidden->IsEditorHidden(owners[i]) ? SceneDrawFlags::EditorHidden : SceneDrawFlags::None;
            for (size_t p = 0; p < object->Parts.size() && p < state.Parts.size(); ++p)
            {
                const ObjectPart& part = object->Parts[p];
                const PartState& handles = state.Parts[p];
                if (handles.Model == 0 || _resources->GetState(handles.Model) != ResourceState::Ready)
                    continue;
                const auto* model = static_cast<const ModelResourceInfo*>(_resources->GetData(handles.Model));
                if (!model || !model->Parts)
                    continue;
                const ResourceGUID* items = GetModelPartItems(model);
                uint64_t material = 0;
                if (handles.Material != 0 && _resources->GetState(handles.Material) == ResourceState::Ready)
                    material = ResolveMaterial(handles.Material);
                float nodeWorld[16];
                Multiply(object->Nodes[static_cast<size_t>(part.Node)].Baked, actorWorld, nodeWorld);
                for (int32_t m = 0; m < model->PartCount; ++m)
                {
                    if (!IsZero(part.Submesh) && (!items || !SameGuid(items[m], part.Submesh)))
                        continue;
                    SceneDraw draw = {};
                    draw.Mesh = model->Parts[m].Mesh;
                    draw.Material = material;
                    draw.Flags = flags;
                    Multiply(model->Parts[m].World, nodeWorld, draw.World);
                    draws.push_back(draw);
                }
            }
        }
        _drawCount = static_cast<int32_t>(draws.size() - first);
    }
}
