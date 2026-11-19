// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "JobSystemImpl.h"
#include <chrono>
#include "xgLog.h"

namespace xg
{
    namespace
    {
        JobHandle MakeHandle(uint32_t index, uint32_t generation)
        {
            return (static_cast<uint64_t>(generation) << 32) | static_cast<uint64_t>(index + 1);
        }
    }

    JobSystemImpl::JobSystemImpl(uint32_t workerCount)
        : _mainThread(std::this_thread::get_id())
    {
        if (workerCount == 0)
        {
            const uint32_t hardware = std::thread::hardware_concurrency();
            workerCount = hardware > 1 ? hardware - 1 : 1;
        }
        _workers.reserve(workerCount);
        for (uint32_t i = 0; i < workerCount; ++i)
            _workers.emplace_back([this] { WorkerLoop(); });
    }

    JobSystemImpl::~JobSystemImpl()
    {
        Shutdown();
    }

    void JobSystemImpl::Shutdown()
    {
        if (_workers.empty())
            return;
        if (IsMainThread())
            WaitIdle();
        {
            std::lock_guard<std::mutex> lock(_mutex);
            _stopping = true;
        }
        _workAvailable.notify_all();
        for (std::thread& worker : _workers)
            worker.join();
        _workers.clear();
    }

    bool JobSystemImpl::IsMainThread() const
    {
        return std::this_thread::get_id() == _mainThread;
    }

    uint32_t JobSystemImpl::WorkerCount() const
    {
        return static_cast<uint32_t>(_workers.size());
    }

    JobHandle JobSystemImpl::Schedule(const JobDesc* desc)
    {
        if (!desc || !desc->Run || (desc->DependencyCount != 0 && !desc->Dependencies))
            return 0;
        std::unique_lock<std::mutex> lock(_mutex);
        if (_stopping)
        {
            xg::Log(xg::MessageType::Warning, "[Jobs] Schedule after shutdown ignored");
            return 0;
        }
        uint32_t index;
        if (!_free.empty())
        {
            index = _free.back();
            _free.pop_back();
        }
        else
        {
            index = static_cast<uint32_t>(_jobs.size());
            _jobs.emplace_back();
        }
        Job& job = _jobs[index];
        job.Run = desc->Run;
        job.User = desc->User;
        job.Queue = desc->Queue;
        job.Module = desc->Module;
        job.Remaining = 0;
        job.Status = State::Waiting;
        job.Dependents.clear();
        for (uint32_t i = 0; i < desc->DependencyCount; ++i)
        {
            const JobHandle dependency = desc->Dependencies[i];
            if (IsDoneLocked(dependency))
                continue;
            _jobs[static_cast<uint32_t>(dependency) - 1].Dependents.push_back(index);
            ++_jobs[index].Remaining;
        }
        ++_pending;
        ++_moduleJobs[desc->Module];
        const JobHandle handle = MakeHandle(index, _jobs[index].Generation);
        if (_jobs[index].Remaining == 0)
            MakeReady(index);
        lock.unlock();
        _workAvailable.notify_one();
        return handle;
    }

    void JobSystemImpl::MakeReady(uint32_t index)
    {
        Job& job = _jobs[index];
        job.Status = State::Ready;
        if (job.Queue == JobQueue::Main)
        {
            _mainQueue.push_back(index);
            _jobDone.notify_all();
        }
        else
        {
            _workerQueue.push_back(index);
        }
    }

    bool JobSystemImpl::IsDoneLocked(JobHandle handle) const
    {
        const uint32_t slot = static_cast<uint32_t>(handle);
        if (slot == 0 || slot > _jobs.size())
            return true;
        const Job& job = _jobs[slot - 1];
        return job.Status == State::Free || job.Generation != static_cast<uint32_t>(handle >> 32);
    }

    bool JobSystemImpl::IsDone(JobHandle handle) const
    {
        std::lock_guard<std::mutex> lock(_mutex);
        return IsDoneLocked(handle);
    }

    void JobSystemImpl::Execute(std::unique_lock<std::mutex>& lock, uint32_t index)
    {
        Job& job = _jobs[index];
        job.Status = State::Running;
        void (*run)(void*) = job.Run;
        void* user = job.User;
        lock.unlock();
        try
        {
            run(user);
        }
        catch (...)
        {
            xg::Log(xg::MessageType::Error, "[Jobs] a job threw an exception; jobs must not throw");
        }
        lock.lock();

        Job& done = _jobs[index];
        std::vector<uint32_t> dependents;
        dependents.swap(done.Dependents);
        const uint32_t module = done.Module;
        done.Status = State::Free;
        done.Run = nullptr;
        done.User = nullptr;
        if (++done.Generation == 0)
            done.Generation = 1;
        _free.push_back(index);
        for (uint32_t dependent : dependents)
        {
            if (--_jobs[dependent].Remaining == 0)
                MakeReady(dependent);
        }
        --_pending;
        auto found = _moduleJobs.find(module);
        if (found != _moduleJobs.end() && --found->second == 0)
            _moduleJobs.erase(found);
        _jobDone.notify_all();
        if (!dependents.empty())
            _workAvailable.notify_all();
    }

    bool JobSystemImpl::TryRunOne(std::unique_lock<std::mutex>& lock, bool allowMain)
    {
        if (allowMain && !_mainQueue.empty())
        {
            const uint32_t index = _mainQueue.front();
            _mainQueue.pop_front();
            Execute(lock, index);
            return true;
        }
        if (!_workerQueue.empty())
        {
            const uint32_t index = _workerQueue.front();
            _workerQueue.pop_front();
            Execute(lock, index);
            return true;
        }
        return false;
    }

    void JobSystemImpl::WorkerLoop()
    {
        std::unique_lock<std::mutex> lock(_mutex);
        for (;;)
        {
            _workAvailable.wait(lock, [this] { return _stopping || !_workerQueue.empty(); });
            if (_workerQueue.empty())
                return;
            TryRunOne(lock, false);
        }
    }

    template <typename Done>
    void JobSystemImpl::WaitUntil(Done done)
    {
        const bool main = IsMainThread();
        std::unique_lock<std::mutex> lock(_mutex);
        while (!done())
        {
            if (TryRunOne(lock, main))
                continue;
            _jobDone.wait_for(lock, std::chrono::milliseconds(2));
        }
    }

    void JobSystemImpl::Wait(JobHandle handle)
    {
        WaitUntil([&] { return IsDoneLocked(handle); });
    }

    void JobSystemImpl::WaitModule(uint32_t moduleId)
    {
        WaitUntil([&] { return _moduleJobs.find(moduleId) == _moduleJobs.end(); });
    }

    void JobSystemImpl::WaitIdle()
    {
        WaitUntil([&] { return _pending == 0; });
    }

    int32_t JobSystemImpl::PumpMain(float budgetSeconds)
    {
        using Clock = std::chrono::steady_clock;
        const Clock::time_point start = Clock::now();
        int32_t ran = 0;
        std::unique_lock<std::mutex> lock(_mutex);
        while (!_mainQueue.empty())
        {
            if (ran > 0 && budgetSeconds > 0.0f && std::chrono::duration<float>(Clock::now() - start).count() >= budgetSeconds)
                break;
            const uint32_t index = _mainQueue.front();
            _mainQueue.pop_front();
            Execute(lock, index);
            ++ran;
        }
        return ran;
    }
}
