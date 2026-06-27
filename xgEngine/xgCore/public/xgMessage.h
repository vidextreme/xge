#pragma once
#include "xgStream.h"

namespace xg
{
    struct ScriptMessage
    {
        uint32_t Type = 0;
        std::unique_ptr<Stream> Payload;
    };
}
