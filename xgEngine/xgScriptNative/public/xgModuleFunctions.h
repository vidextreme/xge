// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once

#include <cstdint>

namespace xg
{
    class ScriptEngine;
}

// Entry points of a script module whose code was loaded by the caller (for example a collectible managed load
// context): the same shapes as a native module's XG_Init / XG_Update / XG_Shutdown / XG_OnMessage. Set Size to
// sizeof(XG_ModuleFunctions); OnMessage may be null.
struct XG_ModuleFunctions
{
    uint32_t Size;
    bool (*Init)(xg::ScriptEngine* engine, uint32_t moduleId);
    void (*Update)(float dt);
    void (*Shutdown)();
    void (*OnMessage)(const char* typeName, void* payload, int payloadSize);
};
