#pragma once
#include <cstdint>

namespace xg
{
	XG_ENUM(inherit = byte)
    enum class PayloadMode : uint8_t
    {
        JSON,
        BINARY
    };

    //
    // ScriptMessage
    //
    // Universal message envelope used by ScriptEngine, ScriptHost, and ScriptModules.
    //
    struct ScriptMessage
    {
        const char* TypeName;   // e.g. "EngineConfig", "PlayerState"
        const void* Payload;    // raw bytes (JSON UTF-8 or binary blob)
        int PayloadSize;        // size of Payload in bytes
        uint32_t SourceID = xg::UnassignedID;
    };
}
