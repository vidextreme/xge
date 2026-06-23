#pragma once
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>

namespace xg
{
    class DispatchSystem
    {
    public:
        using DispatchFn = std::function<void()>;

        DispatchSystem(size_t dedicatedCount, size_t pinnedCount);
        ~DispatchSystem();

        void EnqueueMain(DispatchFn fn);
        void EnqueueDedicated(size_t index, DispatchFn fn);
        void EnqueuePinned(size_t index, DispatchFn fn);

        void RunMain();
        void Stop();

    private:
        void DedicatedThreadLoop(size_t index);
        void PinnedThreadLoop(size_t index);

        // MAIN QUEUE
        std::mutex _mainMutex;
        std::queue<DispatchFn> _mainQueue;

        // DEDICATED THREADS
        std::vector<std::thread> _dedicatedThreads;

        // non-movable types must be behind pointers
        std::vector<std::unique_ptr<std::mutex>> _dedicatedMutexes;
        std::vector<std::queue<DispatchFn>>      _dedicatedQueues;
        std::vector<std::unique_ptr<std::condition_variable>> _dedicatedCVs;

        // PINNED THREADS
        std::vector<std::thread> _pinnedThreads;
        std::vector<std::unique_ptr<std::mutex>> _pinnedMutexes;
        std::vector<std::queue<DispatchFn>>      _pinnedQueues;
        std::vector<std::unique_ptr<std::condition_variable>> _pinnedCVs;

        bool _running = true;
    };
}
