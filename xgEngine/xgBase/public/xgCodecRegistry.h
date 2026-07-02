#pragma once
#include "xgTypeRegistry.h"
#include "xgScriptMessage.h"
#include <cstdint>

namespace xg
{
    //
    // Encoder/Decoder function signatures
    //
    using EncoderFn = bool(*)(const void* object,
        const TypeSchema* schema,
        ScriptMessage& outMessage);

    using DecoderFn = bool(*)(const ScriptMessage& message,
        const TypeSchema* schema,
        void* outObject);

    //
    // CodecRegistry
    //
    // Stores encoder/decoder functions for each type + payload mode.
    // Each ScriptHost owns its own registry.
    //
    class CodecRegistry
    {
    public:
        CodecRegistry();
        ~CodecRegistry();

        //
        // Register encoder/decoder for a given type + payload mode
        //
        void RegisterEncoder(const char* typeName,
            PayloadMode mode,
            EncoderFn fn);

        void RegisterDecoder(const char* typeName,
            PayloadMode mode,
            DecoderFn fn);

        //
        // Lookup encoder/decoder
        //
        EncoderFn GetEncoder(const char* typeName,
            PayloadMode mode) const;

        DecoderFn GetDecoder(const char* typeName,
            PayloadMode mode) const;

    private:
        struct CodecPair
        {
            EncoderFn Encoder = nullptr;
            DecoderFn Decoder = nullptr;
        };

        struct Impl;
        Impl* _impl = nullptr;
    };
}
