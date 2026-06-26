#pragma once
#include "xgBase.h"
#include "xgEngineDef.generated.h"

namespace xg
{
    XG_SERIALIZABLE()
        struct EngineConfig
    {
        XG_FIELD()
            const char* RendererModule = nullptr;
    };

    XG_ENUM(inherit = byte)
        enum class ThreadDomain : uint8_t
    {
        MainThread = 0,
        WorkerThread,
        DedicatedThread,
        PinnedThread
    };
}