#pragma once

// xgID
// ------------------------------------------------------------
// xgID is the engine’s 64‑bit unique identifier type,
// generated using a Snowflake‑style layout:
//
//   [41 bits] Timestamp (ms)
//   [10 bits] Machine ID
//   [12 bits] Sequence
//
// xgID provides globally unique, time‑sortable, non‑reused
// identifiers for runtime objects such as Actors and Components.
// It is cross‑platform, deterministic, and safe for multi‑threaded
// spawning.

#include <cstdint>


namespace xg
{
    using xgID = uint64_t;

    class xgIDGenerator
    {
    public:
        explicit xgIDGenerator(uint16_t machineID = 1) noexcept;

        xgID Generate() noexcept;

    private:
        uint64_t NowMs() const noexcept;

        uint64_t _lastTimestamp = 0;
        uint16_t _sequence = 0;
        uint16_t _machineID = 1;
    };
}