#pragma once
#include "xgScriptModule.h"
#include "xgModules.h"

namespace xg
{
    XG_ENUM(inherit = byte)
        enum class ThreadDomain : uint8_t
    {
        MainThread = 0,
        WorkerThread,
        DedicatedThread,
        PinnedThread
    };


    class ScriptNode
    {
    public:
        ScriptNode(ScriptModule* module, ThreadDomain domain);
        ~ScriptNode();

        ScriptModule* GetModule() const;
        ThreadDomain GetDomain() const;
        ScriptNode* GetParent() const;

        ScriptNode* AddChild(ScriptNode* child);
        void Update(float dt);

    private:
        ScriptModule* _module = nullptr;
        ThreadDomain _domain = ThreadDomain::MainThread;

        ScriptNode* _parent = nullptr;
        void* _children = nullptr; // opaque STL container
        friend class ScriptTree;
    };

    class ScriptTree
    {
    public:
        ScriptTree();
        ~ScriptTree();

        ScriptNode* AddModule(ScriptModule* module,
            ThreadDomain domain,
            ScriptModule* parent = nullptr);

        void RemoveModule(ScriptModule* module);
        void Update(float dt);

        void DebugPrint() const;

    private:
        ScriptNode* FindNode(ScriptModule* module) const;
        bool RemoveRecursive(ScriptNode* parent, ScriptModule* module);
        void DebugPrintNode(const ScriptNode* node, int depth) const;

    private:
        ScriptNode* _root = nullptr;
    };
}
