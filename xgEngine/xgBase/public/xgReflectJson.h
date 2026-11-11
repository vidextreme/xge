// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <deque>
#include <string>
#include "xgJson.h"
#include "xgTypeRegistry.h"

namespace xg
{
    // Owns the text behind const char* fields filled by ReadReflected; keep it alive as long as the object
    class ReflectStringStore
    {
    public:
        const char* Store(const char* text);

    private:
        std::deque<std::string> _strings;
    };

    // Fills the fields of a reflected object from the JSON keys that match the schema's field names (exact match first,
    // then ignoring case, so "path" fills a field named Path).
    // Fields without a matching key, or with a value of the wrong JSON type, keep their current value.
    // table names a sub-object to read (returns false, object unchanged, when it is missing or not an object);
    // null reads the whole document.
    bool ReadReflected(const Json& json, const TypeSchema& schema, void* object, ReflectStringStore& strings, const char* table = nullptr);

    // "Field=value Field=value ..." for the primitive and string fields of a reflected object
    std::string DescribeReflected(const TypeSchema& schema, const void* object);
}
