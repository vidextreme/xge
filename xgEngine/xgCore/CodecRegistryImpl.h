#pragma once
#include "xgCodecRegistry.h"

namespace xg
{
	class CodecRegistryImpl : public CodecRegistry
	{
	public:
		CodecRegistryImpl();
		~CodecRegistryImpl();
		void RegisterEncoder(const char* typeName,
			EncoderFn fn) override;
		void RegisterDecoder(const char* typeName,
			DecoderFn fn) override;
		EncoderFn GetEncoder(const char* typeName) const override;
		DecoderFn GetDecoder(const char* typeName) const override;

		bool Encode(const char* typeName,
			const void* src,
			const TypeSchema* schema,
			ScriptMessage& outMessage) const override;

		bool Decode(const char* typeName,
			const ScriptMessage& message,
			const TypeSchema* schema,
			void* dst) const override;
	protected:
		bool EncodeDynamic(const DynamicObject& obj,
			ScriptMessage& outMessage) const override;

		bool DecodeDynamic(const ScriptMessage& message,
			const TypeSchema* schema,
			DynamicObject& outObj) const override;
	private:
		std::unordered_map<std::string, CodecPair> Codecs;
	};
}