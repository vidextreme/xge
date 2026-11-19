// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "ScriptTree.h"
#include "xgLog.h"
#include "xgMemberCallback.h"
#include <cstring>
#include "xgSubsystem.h"
namespace xg
{
    ScriptNode::ScriptNode(ScriptModule* module)
        : _module(module), _parent(nullptr)
    {}

    ScriptNode::~ScriptNode() = default;

    ScriptModule* ScriptNode::GetModule() const { return _module; }
    ScriptNode* ScriptNode::GetParent() const { return _parent; }

    ScriptNode* ScriptNode::AddChild(ScriptNode* child)
    {
        child->_parent = this;
        _children.emplace_back(child);
        return child;
    }

    void ScriptNode::Update(float dt)
    {
        if (_paused)
        {
            if (!_stepPending)
                return;
            dt = StepDeltaTime;
        }
        if (_module && _module->IsValid())
            _module->Update(dt);

        for (auto& c : _children)
            c->Update(dt);
    }

    float ScriptNode::FrameDelta(float dt) const
    {
        bool stepped = false;
        for (const ScriptNode* node = this; node; node = node->_parent)
        {
            if (!node->_paused)
                continue;
            if (!node->_stepPending)
                return -1.0f;
            stepped = true;
        }
        return stepped ? StepDeltaTime : dt;
    }

    void ScriptNode::RegisterSubsystem(TypeID typeID, Subsystem* system)
    {
        xg::Log(xg::MessageType::Info, "Registering subsystem %llu for module %s",
            typeID, _module ? _module->GetId() : "<null>");
        if (_subsystems.find(typeID) != _subsystems.end())
            xg::Log(xg::MessageType::Warning, "Subsystem %llu replaced on module %s; the previous instance is not shut down",
                typeID, _module ? _module->GetId() : "<null>");
        _subsystems[typeID] = system;
    }

    bool ScriptNode::ShutdownSubsystem(TypeID typeID)
    {
        auto it = _subsystems.find(typeID);
        if (it == _subsystems.end())
            return false;

        Subsystem* system = it->second;
        _subsystems.erase(it);
        system->Shutdown();
        XG_DELETE(system);
        return true;
    }

    void ScriptNode::ForEachSubsystem(const std::function<void(Subsystem*)>& callback)
    {
        for (auto& [id, system] : _subsystems)
        {
            callback(system);
        }
    }

    void ScriptNode::ShutdownAllSubsystems()
    {
        for (auto& pair : _subsystems)
        {
            Subsystem* system = pair.second;
			system->Shutdown();
            XG_DELETE(system);
        }
        _subsystems.clear();
    }

    ScriptTree::ScriptTree() : _root(nullptr) {}
    ScriptTree::~ScriptTree() { delete _root; }

    ScriptNode* ScriptTree::AddModule(ScriptModule* module, ScriptModule* parent)
    {
        if (!module)
            return nullptr;

        if (!parent)
        {
            if (!_root)
            {
                _root = new ScriptNode(module);
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

        return parentNode->AddChild(new ScriptNode(module));
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
        for (size_t i = 0; i < children.size(); ++i)
        {
            if (children[i]->GetModule() == module)
            {
                children.erase(children.begin() + i);
                return true;
            }
            if (RemoveRecursive(children[i].get(), module))
                return true;
        }
        return false;
    }

    void ScriptTree::CollectDescendants(ScriptModule* module, std::vector<ScriptModule*>& out)
    {
        if (ScriptNode* node = FindNode(module))
            for (auto& child : node->_children)
                CollectRecursive(child.get(), out);
    }

    void ScriptTree::CollectRecursive(ScriptNode* node, std::vector<ScriptModule*>& out)
    {
        for (auto& child : node->_children)
            CollectRecursive(child.get(), out);
        if (node->GetModule())
            out.push_back(node->GetModule());
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
        if (_root)
            _root->Update(dt);
    }

    void ScriptTree::ClearSteps()
    {
        ForEachNodeRecursive(_root, [](ScriptNode* node) { node->_stepPending = false; });
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

    void ScriptTree::Shutdown()
    {
        ForEachNodeRecursive(_root, [&](ScriptNode* node) 
            { 
                node->ShutdownAllSubsystems();
            });
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

    void ScriptTree::ForEachNodeRecursive(ScriptNode* node, const std::function<void(ScriptNode*)>& callback)
    {
        if (!node)
            return;

        callback(node);

        for (auto& child : node->_children)
        {
            ForEachNodeRecursive(child.get(), callback);
        }
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
