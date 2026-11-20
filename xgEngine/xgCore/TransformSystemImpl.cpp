// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "TransformSystemImpl.h"
#include "xgScriptEngine.h"
#include "xgSceneGraph.h"
#include "xgHierarchyNode.h"
#include "xgVec3.h"
#include "xgQuat.h"

namespace xg
{
    namespace
    {
        constexpr uint64_t TransformLayoutHash = 0x5847545246303031ull;

        Mat4x4 ToMatrix(const TransformData& t)
        {
            return Mat4x4::Scaling(t.Scale[0], t.Scale[1], t.Scale[2])
                * Mat4x4::RotationQuaternion(Quat(t.Rotation[0], t.Rotation[1], t.Rotation[2], t.Rotation[3]))
                * Mat4x4::Translation(t.Position[0], t.Position[1], t.Position[2]);
        }

        TransformData FromMatrix(const Mat4x4& m)
        {
            Vec3 scale;
            Quat rotation;
            Vec3 translation;
            m.Decompose(scale, rotation, translation);
            return { { translation.x, translation.y, translation.z },
                     { rotation.x, rotation.y, rotation.z, rotation.w },
                     { scale.x, scale.y, scale.z } };
        }

        SceneNodeHierarchy* HierarchyOf(HierarchyNode* node)
        {
            return node ? static_cast<SceneNodeHierarchy*>(node) : nullptr;
        }

        HierarchyNode* ActorNodeOf(SceneNodeHierarchy* h)
        {
            return h && h->Owner ? h->Owner->Get<HierarchyNode>() : nullptr;
        }
    }

    TransformSystemImpl::TransformSystemImpl(ScriptEngine* engine, ComponentDataRegistry* registry, uint32_t ownerModuleId)
        : _engine(engine)
        , _registry(registry)
        , _ownerModuleId(ownerModuleId)
    {
        const DataTypeDesc desc = { TransformDataKey, sizeof(TransformData), alignof(float), TransformLayoutHash, DataTypeFlags::EngineOwned };
        _type = _registry->RegisterType(ownerModuleId, &desc);
        _listener = { &TransformSystemImpl::OnAdded, &TransformSystemImpl::OnRemoved, &TransformSystemImpl::OnMoved, this };
        _registry->AddListener(_type, &_listener);
    }

    TransformSystemImpl::~TransformSystemImpl() = default;

    void TransformSystemImpl::Shutdown()
    {
        _world.clear();
        _dirty.clear();
        _registry = nullptr;
    }

    SceneGraph* TransformSystemImpl::Graph() const
    {
        return _engine ? _engine->GetSubsystem<SceneGraph>() : nullptr;
    }

    HierarchyNode* TransformSystemImpl::NodeOf(ActorID actor) const
    {
        SceneGraph* graph = Graph();
        SceneNode* node = graph && actor != 0 ? graph->GetNode(actor) : nullptr;
        return node ? node->Get<HierarchyNode>() : nullptr;
    }

    int32_t TransformSystemImpl::SlotOf(ActorID actor) const
    {
        return _registry ? _registry->GetSlot(_type, actor) : -1;
    }

    TransformData& TransformSystemImpl::Local(uint32_t slot) const
    {
        const ActorID* owners = nullptr;
        uint32_t count = 0;
        auto* dense = static_cast<TransformData*>(_registry->GetDense(_type, &owners, &count));
        return dense[slot];
    }

    void TransformSystemImpl::OnAdded(void* user, ActorID actor, uint32_t slot)
    {
        auto* self = static_cast<TransformSystemImpl*>(user);
        self->Local(slot) = IdentityTransformData();
        self->_world.resize(static_cast<size_t>(slot) + 1, Mat4x4::Identity());
        self->_dirty.resize(static_cast<size_t>(slot) + 1, 1);
        self->_dirty[slot] = 1;
        self->_anyDirty = true;
        if (HierarchyNode* node = self->NodeOf(actor))
        {
            node->TransformIndex = slot;
            self->MarkDescendantsDirty(HierarchyOf(node));
        }
    }

    void TransformSystemImpl::OnRemoved(void* user, ActorID actor, uint32_t slot)
    {
        auto* self = static_cast<TransformSystemImpl*>(user);
        if (HierarchyNode* node = self->NodeOf(actor))
        {
            node->TransformIndex = NoTransformIndex;
            self->MarkDescendantsDirty(HierarchyOf(node));
        }
        if (static_cast<size_t>(slot) + 1 == self->_world.size())
        {
            self->_world.pop_back();
            self->_dirty.pop_back();
        }
    }

    void TransformSystemImpl::OnMoved(void* user, ActorID actor, uint32_t from, uint32_t to)
    {
        auto* self = static_cast<TransformSystemImpl*>(user);
        self->_world[to] = self->_world[from];
        self->_dirty[to] = self->_dirty[from];
        self->_world.pop_back();
        self->_dirty.pop_back();
        if (HierarchyNode* node = self->NodeOf(actor))
            node->TransformIndex = to;
    }

    void TransformSystemImpl::MarkDescendantsDirty(SceneNodeHierarchy* node)
    {
        if (!node)
            return;
        std::vector<SceneNodeHierarchy*> stack;
        for (SceneNodeHierarchy* child = node->FirstChild; child; child = child->NextSibling)
            stack.push_back(child);
        while (!stack.empty())
        {
            SceneNodeHierarchy* h = stack.back();
            stack.pop_back();
            HierarchyNode* actor = ActorNodeOf(h);
            const uint32_t slot = actor ? actor->TransformIndex : NoTransformIndex;
            if (slot != NoTransformIndex && slot < _dirty.size())
            {
                if (_dirty[slot])
                    continue;
                _dirty[slot] = 1;
                _anyDirty = true;
            }
            for (SceneNodeHierarchy* child = h->FirstChild; child; child = child->NextSibling)
                stack.push_back(child);
        }
    }

    Mat4x4 TransformSystemImpl::ParentWorldOf(SceneNodeHierarchy* node)
    {
        for (SceneNodeHierarchy* a = node ? node->Parent : nullptr; a; a = a->Parent)
        {
            HierarchyNode* actor = ActorNodeOf(a);
            if (actor && actor->TransformIndex != NoTransformIndex && actor->TransformIndex < _world.size())
                return WorldAt(actor->TransformIndex);
        }
        return Mat4x4::Identity();
    }

    const Mat4x4& TransformSystemImpl::WorldAt(uint32_t slot)
    {
        if (_dirty[slot])
        {
            const ActorID* owners = nullptr;
            uint32_t count = 0;
            _registry->GetDense(_type, &owners, &count);
            const Mat4x4 parent = ParentWorldOf(HierarchyOf(NodeOf(owners[slot])));
            _world[slot] = ToMatrix(Local(slot)) * parent;
            _dirty[slot] = 0;
            _registry->MarkChanged(_type, slot);
        }
        return _world[slot];
    }

    void TransformSystemImpl::SetLocalAt(uint32_t slot, const TransformData& value)
    {
        Local(slot) = value;
        const ActorID* owners = nullptr;
        uint32_t count = 0;
        _registry->GetDense(_type, &owners, &count);
        SceneNodeHierarchy* node = HierarchyOf(NodeOf(owners[slot]));
        _world[slot] = ToMatrix(value) * ParentWorldOf(node);
        _dirty[slot] = 0;
        _registry->MarkChanged(_type, slot);
        MarkDescendantsDirty(node);
    }

    void TransformSystemImpl::SetWorldAt(uint32_t slot, const Mat4x4& world)
    {
        const ActorID* owners = nullptr;
        uint32_t count = 0;
        _registry->GetDense(_type, &owners, &count);
        SceneNodeHierarchy* node = HierarchyOf(NodeOf(owners[slot]));
        const Mat4x4 parent = ParentWorldOf(node);
        Local(slot) = FromMatrix(world * parent.Inverse());
        _world[slot] = ToMatrix(Local(slot)) * parent;
        _dirty[slot] = 0;
        _registry->MarkChanged(_type, slot);
        MarkDescendantsDirty(node);
    }

    bool TransformSystemImpl::Add(ActorID actor)
    {
        if (!_registry || actor == 0 || SlotOf(actor) >= 0)
            return false;
        return _registry->Add(_type, actor) != nullptr;
    }

    bool TransformSystemImpl::Remove(ActorID actor)
    {
        return _registry && _registry->Remove(_type, actor);
    }

    bool TransformSystemImpl::Has(ActorID actor) const
    {
        return SlotOf(actor) >= 0;
    }

    bool TransformSystemImpl::GetLocal(ActorID actor, TransformData* out) const
    {
        const int32_t slot = SlotOf(actor);
        if (slot < 0 || !out)
            return false;
        *out = Local(static_cast<uint32_t>(slot));
        return true;
    }

    bool TransformSystemImpl::SetLocal(ActorID actor, const TransformData* value)
    {
        const int32_t slot = SlotOf(actor);
        if (slot < 0 || !value)
            return false;
        SetLocalAt(static_cast<uint32_t>(slot), *value);
        return true;
    }

    bool TransformSystemImpl::GetWorld(ActorID actor, TransformData* out)
    {
        const int32_t slot = SlotOf(actor);
        if (slot < 0 || !out)
            return false;
        *out = FromMatrix(WorldAt(static_cast<uint32_t>(slot)));
        return true;
    }

    bool TransformSystemImpl::SetWorld(ActorID actor, const TransformData* value)
    {
        const int32_t slot = SlotOf(actor);
        if (slot < 0 || !value)
            return false;
        SetWorldAt(static_cast<uint32_t>(slot), ToMatrix(*value));
        return true;
    }

    bool TransformSystemImpl::GetWorldMatrix(ActorID actor, float* out16)
    {
        const int32_t slot = SlotOf(actor);
        if (slot < 0 || !out16)
            return false;
        const Mat4x4& world = WorldAt(static_cast<uint32_t>(slot));
        for (int r = 0; r < 4; ++r)
            for (int c = 0; c < 4; ++c)
                out16[r * 4 + c] = world.m[r][c];
        return true;
    }

    bool TransformSystemImpl::SetParent(ActorID child, ActorID parent, bool keepWorld)
    {
        SceneGraph* graph = Graph();
        SceneNode* childNode = graph && child != 0 ? graph->GetNode(child) : nullptr;
        SceneNodeHierarchy* childH = childNode ? childNode->Get<SceneNodeHierarchy>() : nullptr;
        if (!childH)
            return false;
        SceneNodeHierarchy* parentH = nullptr;
        if (parent != 0)
        {
            SceneNode* parentNode = graph->GetNode(parent);
            parentH = parentNode ? parentNode->Get<SceneNodeHierarchy>() : nullptr;
            if (!parentH)
                return false;
            for (SceneNodeHierarchy* walk = parentH; walk; walk = walk->Parent)
            {
                if (walk == childH)
                    return false;
            }
        }
        const int32_t slot = SlotOf(child);
        const bool keep = keepWorld && slot >= 0;
        const Mat4x4 world = keep ? WorldAt(static_cast<uint32_t>(slot)) : Mat4x4::Identity();
        graph->SetParent(childNode, parentH ? parentH->Owner : nullptr);
        if (keep)
        {
            SetWorldAt(static_cast<uint32_t>(slot), world);
            return true;
        }
        if (slot >= 0)
        {
            _dirty[static_cast<uint32_t>(slot)] = 1;
            _anyDirty = true;
        }
        MarkDescendantsDirty(childH);
        return true;
    }

    void TransformSystemImpl::Update()
    {
        if (!_anyDirty || !_registry)
            return;
        _anyDirty = false;
        SceneGraph* graph = Graph();
        if (!graph)
        {
            for (uint32_t slot = 0; slot < _dirty.size(); ++slot)
                WorldAt(slot);
            return;
        }
        const int32_t rootCount = graph->CopyRootIds(nullptr, 0);
        _roots.resize(static_cast<size_t>(rootCount));
        graph->CopyRootIds(_roots.data(), rootCount);

        struct Entry
        {
            SceneNodeHierarchy* Node;
            const Mat4x4* ParentWorld;
        };
        static const Mat4x4 identity = Mat4x4::Identity();
        std::vector<Entry> stack;
        for (ActorID root : _roots)
        {
            SceneNode* rootNode = graph->GetNode(root);
            SceneNodeHierarchy* h = rootNode ? rootNode->Get<SceneNodeHierarchy>() : nullptr;
            if (h)
                stack.push_back({ h, &identity });
            while (!stack.empty())
            {
                const Entry entry = stack.back();
                stack.pop_back();
                const Mat4x4* world = entry.ParentWorld;
                HierarchyNode* actor = ActorNodeOf(entry.Node);
                const uint32_t slot = actor ? actor->TransformIndex : NoTransformIndex;
                if (slot != NoTransformIndex && slot < _world.size())
                {
                    if (_dirty[slot])
                    {
                        _world[slot] = ToMatrix(Local(slot)) * *entry.ParentWorld;
                        _dirty[slot] = 0;
                        _registry->MarkChanged(_type, slot);
                    }
                    world = &_world[slot];
                }
                for (SceneNodeHierarchy* child = entry.Node->FirstChild; child; child = child->NextSibling)
                    stack.push_back({ child, world });
            }
        }
        for (uint32_t slot = 0; slot < _dirty.size(); ++slot)
        {
            if (_dirty[slot])
                WorldAt(slot);
        }
    }

    int32_t TransformSystemImpl::GetLocals(const ActorID* actors, int32_t count, TransformData* out) const
    {
        int32_t found = 0;
        for (int32_t i = 0; actors && out && i < count; ++i)
        {
            if (GetLocal(actors[i], &out[i]))
                ++found;
            else
                out[i] = IdentityTransformData();
        }
        return found;
    }

    int32_t TransformSystemImpl::SetLocals(const ActorID* actors, int32_t count, const TransformData* values)
    {
        int32_t found = 0;
        for (int32_t i = 0; actors && values && i < count; ++i)
        {
            if (SetLocal(actors[i], &values[i]))
                ++found;
        }
        return found;
    }

    int32_t TransformSystemImpl::GetWorlds(const ActorID* actors, int32_t count, TransformData* out)
    {
        int32_t found = 0;
        for (int32_t i = 0; actors && out && i < count; ++i)
        {
            if (GetWorld(actors[i], &out[i]))
                ++found;
            else
                out[i] = IdentityTransformData();
        }
        return found;
    }

    int32_t TransformSystemImpl::SetWorlds(const ActorID* actors, int32_t count, const TransformData* values)
    {
        int32_t found = 0;
        for (int32_t i = 0; actors && values && i < count; ++i)
        {
            if (SetWorld(actors[i], &values[i]))
                ++found;
        }
        return found;
    }

    uint64_t TransformSystemImpl::GetCurrentVersion() const
    {
        return _registry ? _registry->GetCurrentVersion() : 0;
    }

    int32_t TransformSystemImpl::CollectChanged(uint64_t since, ActorID* out, int32_t capacity) const
    {
        return _registry ? _registry->CollectChanged(_type, since, out, capacity) : 0;
    }

    int32_t TransformSystemImpl::CopyActors(ActorID* out, int32_t capacity) const
    {
        if (!_registry)
            return 0;
        const ActorID* owners = nullptr;
        uint32_t count = 0;
        _registry->GetDense(_type, &owners, &count);
        for (int32_t i = 0; out && i < capacity && i < static_cast<int32_t>(count); ++i)
            out[i] = owners[i];
        return static_cast<int32_t>(count);
    }
}
