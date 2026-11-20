// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgJobs.h"
#include "xgScriptEngine.h"

// C ABI over the engine's job system for managed code (xgSharedKit.Managed Jobs.cs).

namespace
{
    xg::JobSystem* JobsOf(xg::ScriptEngine* engine)
    {
        return engine ? engine->GetSubsystem<xg::JobSystem>() : nullptr;
    }
}

// queue: 0 = worker, 1 = main thread. moduleId owns the job: the engine waits for it before unloading the module.
// Returns 0 when there is no job system or the arguments are invalid (the job never runs).
XG_MODULE_EXPORT uint64_t XG_JobSchedule(xg::ScriptEngine* engine, uint32_t moduleId, void (*run)(void*), void* user,
    const uint64_t* dependencies, uint32_t dependencyCount, int32_t queue)
{
    xg::JobSystem* jobs = JobsOf(engine);
    if (!jobs || !run || queue < 0 || queue > 1)
        return 0;
    const xg::JobDesc desc{ run, user, dependencies, dependencyCount, static_cast<xg::JobQueue>(queue), moduleId };
    return jobs->Schedule(&desc);
}

XG_MODULE_EXPORT bool XG_JobIsDone(xg::ScriptEngine* engine, uint64_t handle)
{
    xg::JobSystem* jobs = JobsOf(engine);
    return !jobs || jobs->IsDone(handle);
}

XG_MODULE_EXPORT void XG_JobWait(xg::ScriptEngine* engine, uint64_t handle)
{
    if (xg::JobSystem* jobs = JobsOf(engine))
        jobs->Wait(handle);
}

XG_MODULE_EXPORT uint32_t XG_JobWorkerCount(xg::ScriptEngine* engine)
{
    xg::JobSystem* jobs = JobsOf(engine);
    return jobs ? jobs->WorkerCount() : 0;
}
