#pragma once
#include <vector>
#include <memory>
#include "xgScriptModule.h"
#include "xgCallback.h"
#include <functional>

namespace xg
{
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

    private:
        ScriptModule* _module;
        ThreadDomain  _domain;
        uint16_t      _laneIndex;
        ScriptNode* _parent;
        std::vector<std::unique_ptr<ScriptNode>> _children;

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


        void ForEachModule(const std::function<void(ScriptModule*)>& callback);
        void ForEachModuleRecursive(ScriptNode* node,
            const std::function<void(ScriptModule*)>& callback);

        ScriptModule* FindModuleByIdRecursive(ScriptNode* node, const char* id);

        void Update(float dt);
        void UpdateDedicated(size_t lane, float dt);
        void UpdatePinned(size_t lane, float dt);

        void DebugPrint() const;

    private:
        bool RemoveRecursive(ScriptNode* parent, ScriptModule* module);
        void DebugPrintNode(const ScriptNode* node, int depth) const;

        ScriptNode* _root;
        std::vector<ScriptNode*> _mainNodes;
        std::vector<std::vector<ScriptNode*>> _dedicatedNodes;
        std::vector<std::vector<ScriptNode*>> _pinnedNodes;
    };
}
