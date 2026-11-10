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
        ScriptNode(ScriptModule* module, ThreadDomain domain);
        ~ScriptNode();

        ScriptModule* GetModule() const;
        ThreadDomain  GetDomain() const;
        ScriptNode* GetParent() const;

        void SetDomain(ThreadDomain domain);
        void SetLaneIndex(uint16_t index);
        uint16_t GetLaneIndex() const;

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
        ThreadDomain  _domain;
        uint16_t      _laneIndex;
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

        ScriptNode* AddModule(ScriptModule* module,
            ScriptModule* parent,
            ThreadDomain  domain);

        void RemoveModule(ScriptModule* module);

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

        void Update(float dt);
        void UpdateDedicated(size_t lane, float dt);
        void UpdatePinned(size_t lane, float dt);

        void DebugPrint() const;
        void Shutdown();
    private:
        bool RemoveRecursive(ScriptNode* parent, ScriptModule* module);
        void DebugPrintNode(const ScriptNode* node, int depth) const;

        ScriptNode* _root;
        std::vector<ScriptNode*> _mainNodes;
        std::vector<std::vector<ScriptNode*>> _dedicatedNodes;
        std::vector<std::vector<ScriptNode*>> _pinnedNodes;
    };

}
