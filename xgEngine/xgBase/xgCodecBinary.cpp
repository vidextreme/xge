#include "pch.h"
#include "xgCodecBinary.h"
#include "xgTypeRegistry.h"
#include "xgStream.h"
#include <cstring>
#include "xgStream.h"
#include "xgScriptMessage.h"
namespace xg
{
    bool Encode_Binary_Generic(const void* object,
        const TypeSchema* schema,
        ScriptMessage& out)
    {
        auto ms = CreateMemoryStream(256);
        const uint8_t* base = reinterpret_cast<const uint8_t*>(object);

        for (int i = 0; i < schema->FieldCount; ++i)
        {
            const FieldSchema& field = schema->Fields[i];
            const void* fieldPtr = base + field.Offset;
            const char* type = field.TypeString;

            if (strcmp(type, "bool") == 0)
            {
                ms->Write(fieldPtr, sizeof(bool));
            }
            else if (strcmp(type, "int") == 0 ||
                strcmp(type, "int32_t") == 0)
            {
                ms->Write(fieldPtr, sizeof(int32_t));
            }
            else if (strcmp(type, "uint32_t") == 0 ||
                strcmp(type, "unsigned int") == 0)
            {
                ms->Write(fieldPtr, sizeof(uint32_t));
            }
            else if (strcmp(type, "float") == 0)
            {
                ms->Write(fieldPtr, sizeof(float));
            }
            else if (strcmp(type, "const char*") == 0 ||
                strcmp(type, "char*") == 0)
            {
                const char* s = reinterpret_cast<const char*>(fieldPtr);
                uint32_t len = static_cast<uint32_t>(strlen(s));
                ms->Write(&len, sizeof(uint32_t));
                ms->Write(s, len);
            }
        }

        out.TypeName = schema->Name;
        out.PayloadSize = ms->Size();

        char* buffer = new char[out.PayloadSize];
        memcpy(buffer, ms->GetBuffer(), out.PayloadSize);
        out.Payload = buffer;

        return true;
    }


    bool Decode_Binary_Generic(const ScriptMessage& msg,
        const TypeSchema* schema,
        void* outObject)
    {
        auto ms = CreateMemoryStream(const_cast<void*>(msg.Payload), msg.PayloadSize);
        uint8_t* base = reinterpret_cast<uint8_t*>(outObject);

        for (int i = 0; i < schema->FieldCount; ++i)
        {
            const FieldSchema& field = schema->Fields[i];
            void* fieldPtr = base + field.Offset;
            const char* type = field.TypeString;

            if (strcmp(type, "bool") == 0)
            {
                ms->Read(fieldPtr, sizeof(bool));
            }
            else if (strcmp(type, "int") == 0 ||
                strcmp(type, "int32_t") == 0)
            {
                ms->Read(fieldPtr, sizeof(int32_t));
            }
            else if (strcmp(type, "uint32_t") == 0 ||
                strcmp(type, "unsigned int") == 0)
            {
                ms->Read(fieldPtr, sizeof(uint32_t));
            }
            else if (strcmp(type, "float") == 0)
            {
                ms->Read(fieldPtr, sizeof(float));
            }
            else if (strcmp(type, "const char*") == 0 ||
                strcmp(type, "char*") == 0)
            {
                uint32_t len = 0;
                ms->Read(&len, sizeof(uint32_t));

                size_t copyLen = (len < field.Size - 1) ? len : (field.Size - 1);
                ms->Read(fieldPtr, copyLen);
                reinterpret_cast<char*>(fieldPtr)[copyLen] = '\0';

                if (len > copyLen)
                    ms->Seek(len - copyLen, FileOrigin::Current);
            }
        }

        return true;
    }
}
