#pragma once

#include <cstdint>

namespace xg
{
    struct RawFieldInfo
    {
        const char* Name;        // Display name
        const char* Key;         // Internal key / same as Name for now
        std::uintptr_t MemberPtr; // Encoded pointer-to-member (&T::Field) as integer
        const char* TypeName;    // Stringified C++ type
    };

    template<typename T>
    struct TypeInfo
    {
        static constexpr const char* Name = "Unknown";
        static constexpr int FieldCount = 0;
        static constexpr RawFieldInfo Fields[1] = {};
    };
}
