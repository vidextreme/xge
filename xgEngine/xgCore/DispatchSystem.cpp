#include "pch.h"
#include "DispatchSystem.h"

namespace xg
{
    DispatchSystem::DispatchSystem(size_t dedicatedCount, size_t pinnedCount)
    {
        // Resize containers
        _dedicatedMutexes.resize(dedicatedCount);
        _dedicatedQueues.resize(dedicatedCount);
        _dedicatedCVs.resize(dedicatedCount);

        _pinnedMutexes.resize(pinnedCount);
        _pinnedQueues.resize(pinnedCount);
        _pinnedCVs.resize(pinnedCount);

        // Allocate mutexes and CVs
        for (auto& m : _dedicatedMutexes)
            m = std::make_unique<std::mutex>();
        for (auto& cv : _dedicatedCVs)
            cv = std::make_unique<std::condition_variable>();

        for (auto& m : _pinnedMutexes)
            m = std::make_unique<std::mutex>();
        for (auto& cv : _pinnedCVs)
            cv = std::make_unique<std::condition_variable>();

        // Launch dedicated threads
        _dedicatedThreads.reserve(dedicatedCount);
        for (size_t i = 0; i < dedicatedCount; ++i)
            _dedicatedThreads.emplace_back(&DispatchSystem::DedicatedThreadLoop, this, i);

        // Launch pinned threads
        _pinnedThreads.reserve(pinnedCount);
        for (size_t i = 0; i < pinnedCount; ++i)
            _pinnedThreads.emplace_back(&DispatchSystem::PinnedThreadLoop, this, i);
    }

    DispatchSystem::~DispatchSystem()
    {
        Stop();
    }

    void DispatchSystem::Stop()
    {
        _running = false;

        // Wake all dedicated threads
        for (auto& cv : _dedicatedCVs)
            cv->notify_all();

        // Wake all pinned threads
        for (auto& cv : _pinnedCVs)
            cv->notify_all();

        // Join dedicated
        for (auto& t : _dedicatedThreads)
            if (t.joinable())
                t.join();

        // Join pinned
        for (auto& t : _pinnedThreads)
            if (t.joinable())
                t.join();
    }

    void DispatchSystem::EnqueueMain(DispatchFn fn)
    {
        std::lock_guard lock(_mainMutex);
        _mainQueue.push(std::move(fn));
    }

    void DispatchSystem::RunMain()
    {
        std::queue<DispatchFn> local;

        {
            std::lock_guard lock(_mainMutex);
            std::swap(local, _mainQueue);
        }

        while (!local.empty())
        {
            local.front()();
            local.pop();
        }
    }

    void DispatchSystem::EnqueueDedicated(size_t index, DispatchFn fn)
    {
        {
            std::lock_guard lock(*_dedicatedMutexes[index]);
            _dedicatedQueues[index].push(std::move(fn));
        }
        _dedicatedCVs[index]->notify_one();
    }

    void DispatchSystem::EnqueuePinned(size_t index, DispatchFn fn)
    {
        {
            std::lock_guard lock(*_pinnedMutexes[index]);
            _pinnedQueues[index].push(std::move(fn));
        }
        _pinnedCVs[index]->notify_one();
    }

    void DispatchSystem::DedicatedThreadLoop(size_t index)
    {
        while (_running)
        {
            DispatchFn fn;

            {
                std::unique_lock lock(*_dedicatedMutexes[index]);
                _dedicatedCVs[index]->wait(lock, [&]
                    {
                        return !_running || !_dedicatedQueues[index].empty();
                    });

                if (!_running)
                    return;

                fn = std::move(_dedicatedQueues[index].front());
                _dedicatedQueues[index].pop();
            }

            fn();
        }
    }

    void DispatchSystem::PinnedThreadLoop(size_t index)
    {
        while (_running)
        {
            DispatchFn fn;

            {
                std::unique_lock lock(*_pinnedMutexes[index]);
                _pinnedCVs[index]->wait(lock, [&]
                    {
                        return !_running || !_pinnedQueues[index].empty();
                    });

                if (!_running)
                    return;

                fn = std::move(_pinnedQueues[index].front());
                _pinnedQueues[index].pop();
            }

            fn();
        }
    }
}
