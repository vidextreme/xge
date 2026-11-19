// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <condition_variable>
#include <deque>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <vector>
#include "xgJobs.h"

namespace xg
{
    class JobSystemImpl : public JobSystem
    {
        XG_DECLARE_SUBSYSTEM(JobSystemImpl, JobSystem)
    public:
        // workerCount 0 = hardware threads - 1 (at least 1).
        explicit JobSystemImpl(uint32_t workerCount = 0);
        ~JobSystemImpl() override;

        void Shutdown() override;

        JobHandle Schedule(const JobDesc* desc) override;
        bool IsDone(JobHandle handle) const override;
        void Wait(JobHandle handle) override;
        void WaitModule(uint32_t moduleId) override;
        void WaitIdle() override;
        int32_t PumpMain(float budgetSeconds) override;
        uint32_t WorkerCount() const override;

    private:
        enum class State : uint8_t
        {
            Free,
            Waiting,
            Ready,
            Running,
        };

        struct Job
        {
            void (*Run)(void*) = nullptr;
            void* User = nullptr;
            JobQueue Queue = JobQueue::Worker;
            uint32_t Module = 0;
            uint32_t Generation = 1;
            uint32_t Remaining = 0;
            State Status = State::Free;
            std::vector<uint32_t> Dependents;
        };

        template <typename Done>
        void WaitUntil(Done done);
        bool IsDoneLocked(JobHandle handle) const;
        bool TryRunOne(std::unique_lock<std::mutex>& lock, bool allowMain);
        void Execute(std::unique_lock<std::mutex>& lock, uint32_t index);
        void MakeReady(uint32_t index);
        void WorkerLoop();
        bool IsMainThread() const;

        mutable std::mutex _mutex;
        std::condition_variable _workAvailable;
        std::condition_variable _jobDone;
        std::vector<Job> _jobs;
        std::vector<uint32_t> _free;
        std::deque<uint32_t> _workerQueue;
        std::deque<uint32_t> _mainQueue;
        std::unordered_map<uint32_t, uint32_t> _moduleJobs;
        uint32_t _pending = 0;
        std::vector<std::thread> _workers;
        std::thread::id _mainThread;
        bool _stopping = false;
    };
}
