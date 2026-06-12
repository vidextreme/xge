#include "pch.h"
#include "xgJson.h"
#include <json.hpp>
#include <vector>
#include <string>

namespace xg
{
    struct Json::Impl
    {
        nlohmann::json Data;
        std::string TempString; // used to return stable const char*
    };

    Json::Json()
    {
        impl = new Impl();
    }

    Json::~Json()
    {
        delete impl;
    }

    bool Json::Load(Stream& stream)
    {
        long len = stream.Length();
        if (len <= 0)
            return false;

        std::vector<uint8_t> buffer(len);
        int read = stream.Read(buffer.data(), (int)len);
        if (read != len)
            return false;

        try
        {
            impl->Data = nlohmann::json::parse(buffer.begin(), buffer.end());
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool Json::GetBool(const char* key, bool defaultValue) const
    {
        if (impl->Data.contains(key))
            return impl->Data[key].get<bool>();
        return defaultValue;
    }

    int Json::GetInt(const char* key, int defaultValue) const
    {
        if (impl->Data.contains(key))
            return impl->Data[key].get<int>();
        return defaultValue;
    }

    float Json::GetFloat(const char* key, float defaultValue) const
    {
        if (impl->Data.contains(key))
            return impl->Data[key].get<float>();
        return defaultValue;
    }

    const char* Json::GetString(const char* key, const char* defaultValue) const
    {
        if (impl->Data.contains(key))
        {
            impl->TempString = impl->Data[key].get<std::string>();
            return impl->TempString.c_str();
        }
        return defaultValue;
    }

    bool Json::Has(const char* key) const
    {
        return impl->Data.contains(key);
    }


    void Json::SetBool(const char* key, bool value)
    {
        impl->Data[key] = value;
    }

    void Json::SetInt(const char* key, int value)
    {
        impl->Data[key] = value;
    }

    void Json::SetFloat(const char* key, float value)
    {
        impl->Data[key] = value;
    }

    void Json::SetString(const char* key, const char* value)
    {
        impl->Data[key] = value ? value : "";
    }

    const char* Json::ToString() const
    {
        impl->TempString = impl->Data.dump(4); // pretty print
        return impl->TempString.c_str();
    }


}
