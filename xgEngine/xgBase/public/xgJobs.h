// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <cstdint>
#include "xgBase.h"
#include "xgSubsystem.h"

namespace xg
{
    // Slot index + 1 in the low 32 bits, slot generation in the high 32 bits. 0 is "no job" and is always done.
    using JobHandle = uint64_t;

    enum class JobQueue : int32_t
    {
        // Runs on a worker thread.
        Worker = 0,
        // Runs on the main thread, from JobSystem::PumpMain (every frame) or a main-thread Wait.
        Main = 1,
    };

    // Run must not throw. Module is the module whose code Run executes (0 = engine): the engine waits for a module's
    // jobs before unloading it.
    struct JobDesc
    {
        void (*Run)(void* user);
        void* User;
        const JobHandle* Dependencies;
        uint32_t DependencyCount;
        JobQueue Queue;
        uint32_t Module;
    };

    // The engine's job system, shared by every module: worker threads plus a main-thread queue, with dependencies.
    // A job runs once every job it depends on is done. Schedule, IsDone and Wait may be called from any thread;
    // PumpMain, WaitModule and WaitIdle from the main thread only.
    class JobSystem : public Subsystem
    {
        XG_DECLARE_BASE_SUBSYSTEM(JobSystem)

    public:
        virtual ~JobSystem() = default;

        // Returns 0 when the description is invalid (the job never runs).
        virtual JobHandle Schedule(const JobDesc* desc) = 0;

        // True once the job ran, and for 0 or stale handles.
        virtual bool IsDone(JobHandle handle) const = 0;

        // Blocks until the job is done, running other ready jobs meanwhile (main-thread jobs only on the main thread).
        virtual void Wait(JobHandle handle) = 0;

        // Runs every pending job of the module, including its main-thread jobs.
        virtual void WaitModule(uint32_t moduleId) = 0;

        // Runs every pending job.
        virtual void WaitIdle() = 0;

        // Runs ready main-thread jobs for up to budgetSeconds (at least one when any is ready; <= 0 = all of them).
        // Returns how many ran. The engine calls it once per frame.
        virtual int32_t PumpMain(float budgetSeconds) = 0;

        virtual uint32_t WorkerCount() const = 0;
    };
}
