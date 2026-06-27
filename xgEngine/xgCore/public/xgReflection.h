#pragma once

#include <cstdint>
#include "xgTypeInfo.h"
namespace xg
{
    struct RawFieldInfo
    {
        const char* Name;        // Display name
        const char* Key;         // Internal key / same as Name for now
        std::uintptr_t MemberPtr; // Encoded pointer-to-member (&T::Field) as integer
        const char* TypeName;    // Stringified C++ type
    };
}
