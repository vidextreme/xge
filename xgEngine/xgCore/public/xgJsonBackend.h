#pragma once
#include "xgJson.h"
#include "xgReflectionSerialization.h"

namespace xg
{
    //
    // ============================================================
    //  JSON → Reflection (Deserializer)
    // ============================================================
    //
    struct JsonDeserializer : IDeserializer
    {
        const Json& J;

        JsonDeserializer(const Json& json)
            : J(json)
        {
        }

        bool ReadBool(const char* key) override
        {
            return J.GetBool(key);
        }

        int ReadInt(const char* key) override
        {
            return J.GetInt(key);
        }

        float ReadFloat(const char* key) override
        {
            return J.GetFloat(key);
        }

        const char* ReadString(const char* key) override
        {
            return J.GetString(key);
        }
    };

    //
    // ============================================================
    //  Reflection → JSON (Serializer)
    // ============================================================
    //
    struct JsonSerializer : ISerializer
    {
        Json& J;

        JsonSerializer(Json& json)
            : J(json)
        {
        }

        void WriteBool(const char* key, bool value) override
        {
            J.SetBool(key, value);
        }

        void WriteInt(const char* key, int value) override
        {
            J.SetInt(key, value);
        }

        void WriteFloat(const char* key, float value) override
        {
            J.SetFloat(key, value);
        }

        void WriteString(const char* key, const char* value) override
        {
            J.SetString(key, value);
        }
    };
}
