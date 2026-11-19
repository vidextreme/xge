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

        // A paused node skips its module's and its subtree's updates (and their actors'), except for one frame at
        // StepDeltaTime after RequestStep.
        static constexpr float StepDeltaTime = 1.0f / 60.0f;
        bool IsPaused() const { return _paused; }
        void SetPaused(bool paused) { _paused = paused; }
        void RequestStep() { _stepPending = true; }
        // The delta time this node runs with this frame, or a negative value while a pause (its own or an
        // ancestor's) holds it.
        float FrameDelta(float dt) const;

        
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
        bool _paused = false;
        bool _stepPending = false;

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
        // Ends the frame's single steps (after the actor update).
        void ClearSteps();

        void DebugPrint() const;
        void Shutdown();
    private:
        bool RemoveRecursive(ScriptNode* parent, ScriptModule* module);
        void CollectRecursive(ScriptNode* node, std::vector<ScriptModule*>& out);
        void DebugPrintNode(const ScriptNode* node, int depth) const;

        ScriptNode* _root;
    };

}
