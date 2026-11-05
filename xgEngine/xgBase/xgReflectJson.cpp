// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgReflectJson.h"
#include <cstring>

namespace xg
{
    const char* ReflectStringStore::Store(const char* text)
    {
        _strings.emplace_back(text ? text : "");
        return _strings.back().c_str();
    }

    bool ReadReflected(const Json& json, const TypeSchema& schema, void* object, ReflectStringStore& strings)
    {
        if (!object)
            return false;

        uint8_t* base = static_cast<uint8_t*>(object);
        for (uint32_t i = 0; i < schema.FieldCount; ++i)
        {
            const FieldSchema& field = schema.Fields[i];
            if (!json.Has(field.Name))
                continue;

            void* fieldPtr = base + field.Offset;
            switch (field.ValueKind)
            {
            case ValueType::Bool:
            {
                bool& value = *static_cast<bool*>(fieldPtr);
                value = json.GetBool(field.Name, value);
                break;
            }
            case ValueType::Int32:
            {
                int32_t& value = *static_cast<int32_t*>(fieldPtr);
                value = json.GetInt(field.Name, value);
                break;
            }
            case ValueType::UInt32:
            {
                uint32_t& value = *static_cast<uint32_t*>(fieldPtr);
                value = static_cast<uint32_t>(json.GetInt(field.Name, static_cast<int>(value)));
                break;
            }
            case ValueType::Float:
            {
                float& value = *static_cast<float*>(fieldPtr);
                value = json.GetFloat(field.Name, value);
                break;
            }
            case ValueType::String:
            {
                const char*& value = *static_cast<const char**>(fieldPtr);
                const char* text = json.GetString(field.Name, nullptr);
                if (text)
                    value = text[0] ? strings.Store(text) : nullptr;
                break;
            }
            default:
                break;
            }
        }
        return true;
    }

    std::string DescribeReflected(const TypeSchema& schema, const void* object)
    {
        std::string out;
        if (!object)
            return out;

        const uint8_t* base = static_cast<const uint8_t*>(object);
        for (uint32_t i = 0; i < schema.FieldCount; ++i)
        {
            const FieldSchema& field = schema.Fields[i];
            const void* fieldPtr = base + field.Offset;
            std::string value;
            switch (field.ValueKind)
            {
            case ValueType::Bool:
                value = *static_cast<const bool*>(fieldPtr) ? "true" : "false";
                break;
            case ValueType::Int32:
                value = std::to_string(*static_cast<const int32_t*>(fieldPtr));
                break;
            case ValueType::UInt32:
                value = std::to_string(*static_cast<const uint32_t*>(fieldPtr));
                break;
            case ValueType::Float:
                value = std::to_string(*static_cast<const float*>(fieldPtr));
                break;
            case ValueType::String:
            {
                const char* text = *static_cast<const char* const*>(fieldPtr);
                value = text ? text : "(null)";
                break;
            }
            default:
                continue;
            }

            if (!out.empty())
                out += ' ';
            out += field.Name;
            out += '=';
            out += value;
        }
        return out;
    }
}
