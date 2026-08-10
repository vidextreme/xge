#include "pch.h"
#include "xgID.h"
#include <chrono>

namespace xg
{
    xgIDGenerator::xgIDGenerator(uint16_t machineID) noexcept
        : _machineID(machineID)
    {}

    uint64_t xgIDGenerator::NowMs() const noexcept
    {
        using namespace std::chrono;
        return duration_cast<milliseconds>(
            steady_clock::now().time_since_epoch()
        ).count();
    }

    xgID xgIDGenerator::Generate() noexcept
    {
        uint64_t now = NowMs();

        if (now == _lastTimestamp)
        {
            _sequence = (_sequence + 1) & 0xFFF; // 12 bits
        }
        else
        {
            _lastTimestamp = now;
            _sequence = 0;
        }

        return
            ((now & 0x1FFFFFFFFFFULL) << 22) |   // 41 bits timestamp
            ((_machineID & 0x3FFULL) << 12) |   // 10 bits machine ID
            (_sequence & 0xFFFULL);              // 12 bits sequence
    }
}
