#include "pch.h"
#include "ScriptTree.h"
#include "xgLog.h"
#include <vector>
#include <memory>

namespace xg
{
    // Internal-only structure for children
    struct NodeChildren
    {
        std::vector<std::unique_ptr<ScriptNode>> list;
    };

    // ------------------------------------------------------------
    // ScriptNode
    // ------------------------------------------------------------

    ScriptNode::ScriptNode(ScriptModule* module, ThreadDomain domain)
        : _module(module)
        , _domain(domain)
        , _children(new NodeChildren())
    {
    }

    ScriptNode::~ScriptNode()
    {
        delete static_cast<NodeChildren*>(_children);
        _children = nullptr;
    }

    ScriptModule* ScriptNode::GetModule() const
    {
        return _module;
    }

    ThreadDomain ScriptNode::GetDomain() const
    {
        return _domain;
    }

    ScriptNode* ScriptNode::GetParent() const
    {
        return _parent;
    }

    ScriptNode* ScriptNode::AddChild(ScriptNode* child)
    {
        child->_parent = this;

        auto* children = static_cast<NodeChildren*>(_children);
        children->list.emplace_back(child);

        return child;
    }

    void ScriptNode::Update(float dt)
    {
        if (_module && _module->IsValid())
            _module->Update(dt);

        auto* children = static_cast<NodeChildren*>(_children);
        for (auto& c : children->list)
            c->Update(dt);
    }

    // ------------------------------------------------------------
    // ScriptTree
    // ------------------------------------------------------------

    ScriptTree::ScriptTree()
    {
    }

    ScriptTree::~ScriptTree()
    {
        delete _root;
        _root = nullptr;
    }

    ScriptNode* ScriptTree::AddModule(ScriptModule* module,
        ScriptModule* parent,
        ThreadDomain domain)
    {
        // CASE 1: No parent → create or reuse root
        if (!parent)
        {
            if (!_root)
            {
                _root = new ScriptNode(module, domain);
                return _root;
            }

            // If root exists but parent=null, attach under root
            parent = _root->GetModule();
        }

        // CASE 2: Parent exists → find parent node
        ScriptNode* parentNode = FindNode(parent);
        if (!parentNode)
        {
            xg::Log(xg::MessageType::Error,
                "ScriptTree::AddModule: parent module not found in tree");
            return nullptr;
        }

        // Attach child
        return parentNode->AddChild(new ScriptNode(module, domain));
    }

    void ScriptTree::RemoveModule(ScriptModule* module)
    {
        if (!_root)
            return;

        // Removing root
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
        auto* children = static_cast<NodeChildren*>(parent->_children);

        for (size_t i = 0; i < children->list.size(); ++i)
        {
            ScriptNode* child = children->list[i].get();

            if (child->GetModule() == module)
            {
                children->list.erase(children->list.begin() + i);
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

        std::vector<ScriptNode*> stack;
        stack.push_back(_root);

        while (!stack.empty())
        {
            ScriptNode* n = stack.back();
            stack.pop_back();

            if (n->GetModule() == module)
                return n;

            auto* children = static_cast<NodeChildren*>(n->_children);
            for (auto& c : children->list)
                stack.push_back(c.get());
        }

        return nullptr;
    }

    void ScriptTree::Update(float dt)
    {
        if (_root)
            _root->Update(dt);
    }

    void ScriptTree::DebugPrintNode(const ScriptNode* node, int depth) const
    {
        if (!node)
            return;

        char indent[64];
        int count = depth * 2;
        if (count > 63) count = 63;
        memset(indent, ' ', count);
        indent[count] = '\0';

        const char* id = node->GetModule() ? node->GetModule()->GetId() : "<null>";
        xg::Log(xg::MessageType::Info, "%s- %s", indent, id);

        auto* children = static_cast<NodeChildren*>(node->_children);
        for (auto& c : children->list)
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

}
