// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <vector>
#include <memory>
#include "xgScriptModule.h"
#include "xgCallback.h"
#include <functional>
#include "xgBase.h"
namespace xg
{
    struct Subsystem;
    class ScriptNode
    {
    public:
        explicit ScriptNode(ScriptModule* module);
        ~ScriptNode();

        ScriptModule* GetModule() const;
        ScriptNode* GetParent() const;

        ScriptNode* AddChild(ScriptNode* child);
        void        Update(float dt);

        
        void RegisterSubsystem(TypeID typeID, Subsystem* system);

        bool ShutdownSubsystem(TypeID typeID);
       
        void ForEachSubsystem(const std::function<void(Subsystem*)>& callback);

		Subsystem* GetSubsystem(TypeID typeID)
		{
			auto it = _subsystems.find(typeID);
			if (it != _subsystems.end())
				return static_cast<Subsystem*>(it->second);
			if (_parent)
				return _parent->GetSubsystem(typeID);
			return nullptr;
		}

        template<typename T>
        T* GetSubsystem()
        {
            auto typeID = T::SuperTypeID;
            auto system = GetSubsystem(typeID);
            if (typeID != nullptr)
                return static_cast<T*>(system);

            if (_parent)
                return _parent->GetSubsystem(typeID);

            return nullptr;
        }

        void ShutdownAllSubsystems();
    private:
        ScriptModule* _module;
        ScriptNode* _parent;
        std::vector<std::unique_ptr<ScriptNode>> _children;

        std::unordered_map<TypeID, Subsystem*> _subsystems;

        friend class ScriptTree;
    };

    class ScriptTree
    {
    public:
        ScriptTree();
        ~ScriptTree();

        ScriptNode* AddModule(ScriptModule* module, ScriptModule* parent);

        // Detaches the module's node (and its whole subtree) from the tree and destroys the nodes. Shutting down the
        // subtree's modules and subsystems is the caller's job (Engine::RemoveScriptModule), done before this.
        void RemoveModule(ScriptModule* module);

        // Every module in the subtree under module (module itself excluded), deepest first.
        void CollectDescendants(ScriptModule* module, std::vector<ScriptModule*>& out);

        ScriptNode* FindNode(ScriptModule* module) const;
        ScriptNode* FindNodeForCallback(Callback* cb) const;
        ScriptModule* FindModuleById(const char* id);

        void ForEachNodeRecursive(ScriptNode* node, const std::function<void(ScriptNode*)>& callback);
        void ForEachModule(const std::function<void(ScriptModule*)>& callback);
        void ForEachModuleRecursive(ScriptNode* node,
            const std::function<void(ScriptModule*)>& callback);

        void ForEachSibling(ScriptNode* node,
            const std::function<void(ScriptModule*)>& fn);

        ScriptModule* FindModuleByIdRecursive(ScriptNode* node, const char* id);

        // One recursive pass over the tree, parents before children.
        void Update(float dt);

        void DebugPrint() const;
        void Shutdown();
    private:
        bool RemoveRecursive(ScriptNode* parent, ScriptModule* module);
        void CollectRecursive(ScriptNode* node, std::vector<ScriptModule*>& out);
        void DebugPrintNode(const ScriptNode* node, int depth) const;

        ScriptNode* _root;
    };

}
