// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"

#include "xgCodecJSON.h"
#include "xgJson.h"
#include "xgTypeRegistry.h"
#include "xgStream.h"
#include <string>
#include <cstring>
#include "xgStream.h"
#include <string>
#include <cstring>

namespace xg
{
    bool Encode_JSON_Generic(const void* object,
        const TypeSchema* schema,
        ScriptMessage& out)
    {
        Json json;

        for (int i = 0; i < schema->FieldCount; ++i)
        {
            const FieldSchema& field = schema->Fields[i];
            const uint8_t* base = reinterpret_cast<const uint8_t*>(object);
            const void* fieldPtr = base + field.Offset;
            const char* type = field.TypeString;

            if (strcmp(type, "bool") == 0)
            {
                json.SetBool(field.Name, *reinterpret_cast<const bool*>(fieldPtr));
            }
            else if (strcmp(type, "int") == 0 ||
                strcmp(type, "int32_t") == 0)
            {
                json.SetInt(field.Name, *reinterpret_cast<const int32_t*>(fieldPtr));
            }
            else if (strcmp(type, "uint32_t") == 0 ||
                strcmp(type, "unsigned int") == 0)
            {
                uint32_t v = *reinterpret_cast<const uint32_t*>(fieldPtr);
                json.SetInt(field.Name, static_cast<int>(v));
            }
            else if (strcmp(type, "float") == 0)
            {
                json.SetFloat(field.Name, *reinterpret_cast<const float*>(fieldPtr));
            }
            else if (strcmp(type, "const char*") == 0 ||
                strcmp(type, "char*") == 0)
            {
                json.SetString(field.Name, reinterpret_cast<const char*>(fieldPtr));
            }
        }

        std::string jsonText;
        {
            auto stream = CreateMemoryStream(256);
            json.Save(*stream);
            jsonText.assign(reinterpret_cast<const char*>(stream->GetBuffer()),
                stream->Size());
        }

        out.TypeName = schema->Name;
        out.PayloadSize = static_cast<int>(jsonText.size());

        char* buffer = new char[out.PayloadSize];
        memcpy(buffer, jsonText.data(), out.PayloadSize);
        out.Payload = buffer;

        return true;
    }


    bool Decode_JSON_Generic(const ScriptMessage& msg,
        const TypeSchema* schema,
        void* outObject)
    {
        auto ms = CreateMemoryStream(const_cast<void*>(msg.Payload), msg.PayloadSize);
        Json json;
        if (!json.Load(*ms))
            return false;

        uint8_t* base = reinterpret_cast<uint8_t*>(outObject);

        for (int i = 0; i < schema->FieldCount; ++i)
        {
            const FieldSchema& field = schema->Fields[i];
            void* fieldPtr = base + field.Offset;
            const char* type = field.TypeString;

            if (strcmp(type, "bool") == 0)
            {
                *reinterpret_cast<bool*>(fieldPtr) =
                    json.GetBool(field.Name, false);
            }
            else if (strcmp(type, "int") == 0 ||
                strcmp(type, "int32_t") == 0)
            {
                *reinterpret_cast<int32_t*>(fieldPtr) =
                    json.GetInt(field.Name, 0);
            }
            else if (strcmp(type, "uint32_t") == 0 ||
                strcmp(type, "unsigned int") == 0)
            {
                int v = json.GetInt(field.Name, 0);
                *reinterpret_cast<uint32_t*>(fieldPtr) =
                    static_cast<uint32_t>(v);
            }
            else if (strcmp(type, "float") == 0)
            {
                *reinterpret_cast<float*>(fieldPtr) =
                    json.GetFloat(field.Name, 0.0f);
            }
            else if (strcmp(type, "const char*") == 0 ||
                strcmp(type, "char*") == 0)
            {
                const char* s = json.GetString(field.Name, "");
                size_t len = strlen(s);
                size_t copyLen = (len < field.Size - 1) ? len : (field.Size - 1);
                memcpy(fieldPtr, s, copyLen);
                reinterpret_cast<char*>(fieldPtr)[copyLen] = '\0';
            }
        }

        return true;
    }
}
