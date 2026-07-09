// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <cstdint>
#include "xgTypeRegistry.h"
#include "xgScriptMessage.h"
#include "xgDynamicObject.h"

namespace xg
{
    //
    // Encoder / Decoder function signatures
    //
    using EncoderFn = bool(*)(const void* src,
        const TypeSchema* schema,
        ScriptMessage& outMessage);

    using DecoderFn = bool(*)(const ScriptMessage& message,
        const TypeSchema* schema,
        void* dst);

    //
    // Pair of encoder/decoder for a given type
    //
    struct CodecPair
    {
        EncoderFn Encoder = nullptr;
        DecoderFn Decoder = nullptr;
    };

    //
    // CodecRegistry
    //
    // - Registers encoders/decoders for native types
    // - Provides dynamic encode/decode for DynamicObject
    // - Selects encoder/decoder based on typeName
    // - No PayloadMode branching (global mode only)
    //
    class CodecRegistry
    {
    public:
        CodecRegistry();
        ~CodecRegistry();

        //
        // Register native encoders/decoders
        //
        void RegisterEncoder(const char* typeName,
            EncoderFn fn);

        void RegisterDecoder(const char* typeName,
            DecoderFn fn);

        //
        // Lookup
        //
        EncoderFn GetEncoder(const char* typeName) const;
        DecoderFn GetDecoder(const char* typeName) const;

        //
        // Public encode/decode entry points
        //
        bool Encode(const char* typeName,
            const void* src,
            const TypeSchema* schema,
            ScriptMessage& outMessage) const;

        bool Decode(const char* typeName,
            const ScriptMessage& message,
            const TypeSchema* schema,
            void* dst) const;

    private:
        struct Impl;
        Impl* _impl;

        //
        // Dynamic encode/decode (JSON only)
        //
        bool EncodeDynamic(const DynamicObject& obj,
            ScriptMessage& outMessage) const;

        bool DecodeDynamic(const ScriptMessage& message,
            const TypeSchema* schema,
            DynamicObject& outObj) const;
    };
}
