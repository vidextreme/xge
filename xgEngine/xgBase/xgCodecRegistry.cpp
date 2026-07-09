// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgCodecRegistry.h"
#include "xgTypeRegistry.h"
#include "xgDynamicObject.h"
#include "xgScriptMessage.h"
#include "xgJson.h"
#include "xgMemoryStream.h"
#include <unordered_map>
#include <string>

namespace xg
{
    struct CodecRegistry::Impl
    {
        std::unordered_map<std::string, CodecPair> Codecs;
    };

    static std::string MakeKey(const char* typeName)
    {
        return std::string(typeName);
    }

    CodecRegistry::CodecRegistry()
        : _impl(new Impl())
    {}

    CodecRegistry::~CodecRegistry()
    {
        delete _impl;
        _impl = nullptr;
    }

    void CodecRegistry::RegisterEncoder(const char* typeName,
        EncoderFn fn)
    {
        auto key = MakeKey(typeName);
        _impl->Codecs[key].Encoder = fn;
    }

    void CodecRegistry::RegisterDecoder(const char* typeName,
        DecoderFn fn)
    {
        auto key = MakeKey(typeName);
        _impl->Codecs[key].Decoder = fn;
    }

    EncoderFn CodecRegistry::GetEncoder(const char* typeName) const
    {
        auto keyExact = MakeKey(typeName);
        auto itExact = _impl->Codecs.find(keyExact);
        if (itExact != _impl->Codecs.end() && itExact->second.Encoder)
            return itExact->second.Encoder;

        auto itWildcard = _impl->Codecs.find("*");
        if (itWildcard != _impl->Codecs.end())
            return itWildcard->second.Encoder;

        return nullptr;
    }

    DecoderFn CodecRegistry::GetDecoder(const char* typeName) const
    {
        auto keyExact = MakeKey(typeName);
        auto itExact = _impl->Codecs.find(keyExact);
        if (itExact != _impl->Codecs.end() && itExact->second.Decoder)
            return itExact->second.Decoder;

        auto itWildcard = _impl->Codecs.find("*");
        if (itWildcard != _impl->Codecs.end())
            return itWildcard->second.Decoder;

        return nullptr;
    }

    // ============================================================
    //  DYNAMIC ENCODE (JSON only)
    // ============================================================
    bool CodecRegistry::EncodeDynamic(const DynamicObject& obj,
        ScriptMessage& outMessage) const
    {
        const TypeSchema* schema = obj.GetSchema();
        if (!schema)
            return false;

        Json json;

        for (uint32_t i = 0; i < obj.GetFieldCount(); ++i)
        {
            const DynamicField* field = obj.GetFieldByIndex(i);
            const FieldSchema* fs = field->Schema;

            switch (fs->ValueKind)
            {
            case ValueType::Bool:
                json.SetBool(fs->Name, field->Value.BoolValue);
                break;
            case ValueType::Int32:
                json.SetInt(fs->Name, field->Value.Int32Value);
                break;
            case ValueType::UInt32:
                json.SetInt(fs->Name, (int)field->Value.UInt32Value);
                break;
            case ValueType::Float:
                json.SetFloat(fs->Name, field->Value.FloatValue);
                break;
            case ValueType::String:
                json.SetString(fs->Name, field->Value.StringValue);
                break;
            default:
                break;
            }
        }

        auto ms = CreateMemoryStream(256);
        json.Save(*ms);

        outMessage.TypeName = schema->Name;
        outMessage.PayloadSize = ms->Size();

        char* buffer = new char[outMessage.PayloadSize];
        memcpy(buffer, ms->GetBuffer(), outMessage.PayloadSize);
        outMessage.Payload = buffer;

        return true;
    }

    // ============================================================
    //  DYNAMIC DECODE (JSON only)
    // ============================================================
    bool CodecRegistry::DecodeDynamic(const ScriptMessage& msg,
        const TypeSchema* schema,
        DynamicObject& outObj) const
    {
        MemoryStream ms(const_cast<void*>(msg.Payload), msg.PayloadSize);
        Json json;
        if (!json.Load(ms))
            return false;

        for (uint32_t i = 0; i < schema->FieldCount; ++i)
        {
            const FieldSchema& fs = schema->Fields[i];
            DynamicField* field = outObj.GetFieldByIndex(i);

            switch (fs.ValueKind)
            {
            case ValueType::Bool:
                field->Value.BoolValue = json.GetBool(fs.Name, false);
                break;
            case ValueType::Int32:
                field->Value.Int32Value = json.GetInt(fs.Name, 0);
                break;
            case ValueType::UInt32:
                field->Value.UInt32Value = (uint32_t)json.GetInt(fs.Name, 0);
                break;
            case ValueType::Float:
                field->Value.FloatValue = json.GetFloat(fs.Name, 0.0f);
                break;
            case ValueType::String:
            {
                const char* s = json.GetString(fs.Name, "");
                char* buf = new char[strlen(s) + 1];
                size_t len = strlen(s);
                memcpy(buf, s, len + 1);   // copy null terminator too
                field->Value.StringValue = buf;
                break;
            }
            default:
                break;
            }
        }

        return true;
    }

    // ============================================================
    //  PUBLIC ENCODE / DECODE
    // ============================================================
    bool CodecRegistry::Encode(const char* typeName,
        const void* src,
        const TypeSchema* schema,
        ScriptMessage& outMessage) const
    {
        if (!schema)
            return false;

        if (!schema->NativeLayout)
            return EncodeDynamic(*reinterpret_cast<const DynamicObject*>(src),
                outMessage);

        EncoderFn fn = GetEncoder(typeName);
        if (!fn)
            return false;

        return fn(src, schema, outMessage);
    }

    bool CodecRegistry::Decode(const char* typeName,
        const ScriptMessage& message,
        const TypeSchema* schema,
        void* dst) const
    {
        if (!schema)
            return false;

        if (!schema->NativeLayout)
            return DecodeDynamic(message, schema,
                *reinterpret_cast<DynamicObject*>(dst));

        DecoderFn fn = GetDecoder(typeName);
        if (!fn)
            return false;

        return fn(message, schema, dst);
    }
}
