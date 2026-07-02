#include "pch.h"
#include "xgCodecRegistry.h"
#include <unordered_map>
#include <string>

namespace xg
{
    struct CodecRegistry::Impl
    {
        std::unordered_map<std::string, CodecPair> Codecs;
    };

    static std::string MakeKey(const char* typeName, PayloadMode mode)
    {
        return std::string(typeName) + ":" + std::to_string((int)mode);
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
        PayloadMode mode,
        EncoderFn fn)
    {
        auto key = MakeKey(typeName, mode);
        _impl->Codecs[key].Encoder = fn;
    }

    void CodecRegistry::RegisterDecoder(const char* typeName,
        PayloadMode mode,
        DecoderFn fn)
    {
        auto key = MakeKey(typeName, mode);
        _impl->Codecs[key].Decoder = fn;
    }

    EncoderFn CodecRegistry::GetEncoder(const char* typeName,
        PayloadMode mode) const
    {
        // 1. Try exact match
        auto keyExact = MakeKey(typeName, mode);
        auto itExact = _impl->Codecs.find(keyExact);
        if (itExact != _impl->Codecs.end() && itExact->second.Encoder)
            return itExact->second.Encoder;

        // 2. Fallback to wildcard "*"
        auto keyWildcard = MakeKey("*", mode);
        auto itWildcard = _impl->Codecs.find(keyWildcard);
        if (itWildcard != _impl->Codecs.end())
            return itWildcard->second.Encoder;

        return nullptr;
    }

    DecoderFn CodecRegistry::GetDecoder(const char* typeName,
        PayloadMode mode) const
    {
        // 1. Try exact match
        auto keyExact = MakeKey(typeName, mode);
        auto itExact = _impl->Codecs.find(keyExact);
        if (itExact != _impl->Codecs.end() && itExact->second.Decoder)
            return itExact->second.Decoder;

        // 2. Fallback to wildcard "*"
        auto keyWildcard = MakeKey("*", mode);
        auto itWildcard = _impl->Codecs.find(keyWildcard);
        if (itWildcard != _impl->Codecs.end())
            return itWildcard->second.Decoder;

        return nullptr;
    }
}
