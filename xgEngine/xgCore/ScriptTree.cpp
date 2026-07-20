// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "ScriptTree.h"
#include "xgLog.h"
#include "xgMemberCallback.h"
#include <cstring>
#include "xgSystem.h"
namespace xg
{
    ScriptNode::ScriptNode(ScriptModule* module, ThreadDomain domain)
        : _module(module), _domain(domain), _laneIndex(0), _parent(nullptr)
    {}

    ScriptNode::~ScriptNode() = default;

    ScriptModule* ScriptNode::GetModule() const { return _module; }
    ThreadDomain  ScriptNode::GetDomain() const { return _domain; }
    ScriptNode* ScriptNode::GetParent() const { return _parent; }

    void ScriptNode::SetDomain(ThreadDomain domain) { _domain = domain; }
    void ScriptNode::SetLaneIndex(uint16_t index) { _laneIndex = index; }
    uint16_t ScriptNode::GetLaneIndex() const { return _laneIndex; }

    ScriptNode* ScriptNode::AddChild(ScriptNode* child)
    {
        child->_parent = this;
        _children.emplace_back(child);
        return child;
    }

    void ScriptNode::Update(float dt)
    {
        if (_module && _module->IsValid())
            _module->Update(dt);

        for (auto& c : _children)
            c->Update(dt);
    }

    void ScriptNode::RegisterSystem(TypeID typeID, System* system)
    {
        xg::Log(xg::MessageType::Info, "Registering system %llu for module %s",
            typeID, _module ? _module->GetId() : "<null>");
        _systems[typeID] = system;
    }

    void ScriptNode::ShutdownSystem(TypeID typeID)
    {
        auto it = _systems.find(typeID);
        if (it != _systems.end())
        {
            auto* system = it->second;
            system->Shutdown();
            delete system;
            _systems.erase(it);
        }
    }

    void ScriptNode::ShutdownAllSystems()
    {
        for (auto& pair : _systems)
        {
            System* system = pair.second;
			system->Shutdown();
            XG_DELETE(system);
        }
        _systems.clear();
    }

    ScriptTree::ScriptTree() : _root(nullptr) {}
    ScriptTree::~ScriptTree() { delete _root; }

    ScriptNode* ScriptTree::AddModule(ScriptModule* module,
        ScriptModule* parent,
        ThreadDomain  domain)
    {
        if (!module)
            return nullptr;

        if (!parent)
        {
            if (!_root)
            {
                _root = new ScriptNode(module, domain);
                _mainNodes.push_back(_root);
                return _root;
            }
            parent = _root->GetModule();
        }

        ScriptNode* parentNode = FindNode(parent);
        if (!parentNode)
        {
            xg::Log(xg::MessageType::Error,
                "ScriptTree::AddModule: parent module not found");
            return nullptr;
        }

        ScriptNode* node = parentNode->AddChild(new ScriptNode(module, domain));

        switch (domain)
        {
        case ThreadDomain::MainThread:
            _mainNodes.push_back(node);
            break;

        case ThreadDomain::DedicatedThread:
            if (node->GetLaneIndex() >= _dedicatedNodes.size())
                _dedicatedNodes.resize(node->GetLaneIndex() + 1);
            _dedicatedNodes[node->GetLaneIndex()].push_back(node);
            break;

        case ThreadDomain::PinnedThread:
            if (node->GetLaneIndex() >= _pinnedNodes.size())
                _pinnedNodes.resize(node->GetLaneIndex() + 1);
            _pinnedNodes[node->GetLaneIndex()].push_back(node);
            break;
        }

        return node;
    }

    void ScriptTree::RemoveModule(ScriptModule* module)
    {
        if (!_root || !module)
            return;

        if (_root->GetModule() == module)
        {
            delete _root;
            _root = nullptr;
            return;
        }

        RemoveRecursive(_root, module);
    }

    bool ScriptTree::RemoveRecursive(ScriptNode* parent, ScriptModule* module)
    {
        auto& children = parent->_children;
        parent->ShutdownAllSystems();
        for (size_t i = 0; i < children.size(); ++i)
        {
            ScriptNode* child = children[i].get();

            if (child->GetModule() == module)
            {
                child->ShutdownAllSystems();

                for (auto& gc : child->_children)
                    gc->ShutdownAllSystems();

                children.erase(children.begin() + i);
                return true;
            }

            if (RemoveRecursive(child, module))
                return true;
        }

        return false;
    }

    ScriptNode* ScriptTree::FindNode(ScriptModule* module) const
    {
        if (!_root)
            return nullptr;

        std::vector<ScriptNode*> stack{ _root };

        while (!stack.empty())
        {
            ScriptNode* n = stack.back();
            stack.pop_back();

            if (n->GetModule() == module)
                return n;

            for (auto& c : n->_children)
                stack.push_back(c.get());
        }

        return nullptr;
    }

    ScriptNode* ScriptTree::FindNodeForCallback(Callback* cb) const
    {
        auto* mc = static_cast<MemberCallbackBase*>(cb);
        return FindNode(mc->GetOwnerModule());
    }


    void ScriptTree::Update(float dt)
    {
        for (ScriptNode* n : _mainNodes)
            n->Update(dt);
    }

    void ScriptTree::UpdateDedicated(size_t lane, float dt)
    {
        if (lane >= _dedicatedNodes.size())
            return;

        for (ScriptNode* n : _dedicatedNodes[lane])
            n->Update(dt);
    }

    void ScriptTree::UpdatePinned(size_t lane, float dt)
    {
        if (lane >= _pinnedNodes.size())
            return;

        for (ScriptNode* n : _pinnedNodes[lane])
            n->Update(dt);
    }

    void ScriptTree::DebugPrintNode(const ScriptNode* node, int depth) const
    {
        char indent[64];
        int count = depth * 2;
        memset(indent, ' ', count);
        indent[count] = '\0';

        const char* id = node->GetModule()
            ? node->GetModule()->GetId()
            : "<null>";

        xg::Log(xg::MessageType::Info, "%s- %s", indent, id);

        for (auto& c : node->_children)
            DebugPrintNode(c.get(), depth + 1);
    }

    void ScriptTree::DebugPrint() const
    {
        xg::Log(xg::MessageType::Info, "=== ScriptTree ===");

        if (_root)
            DebugPrintNode(_root, 0);
        else
            xg::Log(xg::MessageType::Info, "<empty>");

        xg::Log(xg::MessageType::Info, "==================");
    }

    ScriptModule* ScriptTree::FindModuleById(const char* id)
    {
        if (!_root)
            return nullptr;

        return FindModuleByIdRecursive(_root, id);
    }

    ScriptModule* ScriptTree::FindModuleByIdRecursive(ScriptNode* node, const char* id)
    {
        if (!node)
            return nullptr;

        ScriptModule* m = node->GetModule();
        if (m && strcmp(m->GetId(), id) == 0)
            return m;

        for (auto& c : node->_children)
        {
            ScriptModule* found = FindModuleByIdRecursive(c.get(), id);
            if (found)
                return found;
        }

        return nullptr;
    }

    void ScriptTree::ForEachModule(const std::function<void(ScriptModule*)>& callback)
    {
        ForEachModuleRecursive(_root, callback);
    }

    void ScriptTree::ForEachModuleRecursive(ScriptNode* node,
        const std::function<void(ScriptModule*)>& callback)
    {
        if (!node)
            return;

        if (node->GetModule())
            callback(node->GetModule());

        for (auto& c : node->_children)
            ForEachModuleRecursive(c.get(), callback);
    }

    void ScriptTree::ForEachSibling(ScriptNode* node,
        const std::function<void(ScriptModule*)>& fn)
    {
        if (!node || !node->_parent)
            return;

        ScriptNode* parent = node->_parent;

        for (auto& child : parent->_children)
        {
            if (child.get() != node)
                fn(child->GetModule());
        }
    }

}
