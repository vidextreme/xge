// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgJson.h"
#include <json.hpp>
#include <cstring>
#include <iterator>
#include <vector>
#include <string>

namespace xg
{
    struct Json::Impl
    {
        nlohmann::ordered_json Data;
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
            impl->Data = nlohmann::ordered_json::parse(buffer.begin(), buffer.end());
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    bool Json::Save(Stream& stream) const
    {
        // Convert JSON to string
        impl->TempString = impl->Data.dump(4);
        const char* text = impl->TempString.c_str();
        size_t len = impl->TempString.size();

        // Write to stream
        int written = stream.Write(text, (int)len);
        return written == (int)len;
    }


    bool Json::GetBool(const char* key, bool defaultValue) const
    {
        auto it = impl->Data.find(key);
        if (it != impl->Data.end() && it->is_boolean())
            return it->get<bool>();
        return defaultValue;
    }

    int Json::GetInt(const char* key, int defaultValue) const
    {
        auto it = impl->Data.find(key);
        if (it != impl->Data.end() && it->is_number())
            return it->get<int>();
        return defaultValue;
    }

    float Json::GetFloat(const char* key, float defaultValue) const
    {
        auto it = impl->Data.find(key);
        if (it != impl->Data.end() && it->is_number())
            return it->get<float>();
        return defaultValue;
    }

    const char* Json::GetString(const char* key, const char* defaultValue) const
    {
        auto it = impl->Data.find(key);
        if (it != impl->Data.end() && it->is_string())
        {
            impl->TempString = it->get<std::string>();
            return impl->TempString.c_str();
        }
        return defaultValue;
    }

    bool Json::Has(const char* key) const
    {
        return impl->Data.contains(key);
    }

    bool Json::GetTable(const char* key, Json& out) const
    {
        auto it = impl->Data.find(key);
        if (it == impl->Data.end() || !it->is_object())
            return false;
        out.impl->Data = *it;
        return true;
    }

    int Json::GetObjectSize(const char* key) const
    {
        auto it = impl->Data.find(key);
        return (it != impl->Data.end() && it->is_object()) ? static_cast<int>(it->size()) : 0;
    }

    const char* Json::GetObjectKey(const char* key, int index) const
    {
        auto it = impl->Data.find(key);
        if (it == impl->Data.end() || !it->is_object() || index < 0 || index >= static_cast<int>(it->size()))
            return nullptr;
        auto member = it->begin();
        std::advance(member, index);
        impl->TempString = member.key();
        return impl->TempString.c_str();
    }

    bool Json::GetObjectItem(const char* key, int index, Json& out) const
    {
        auto it = impl->Data.find(key);
        if (it == impl->Data.end() || !it->is_object() || index < 0 || index >= static_cast<int>(it->size()))
            return false;
        auto member = it->begin();
        std::advance(member, index);
        if (!member->is_object())
            return false;
        out.impl->Data = *member;
        return true;
    }

    const char* Json::GetObjectString(const char* key, int index) const
    {
        auto it = impl->Data.find(key);
        if (it == impl->Data.end() || !it->is_object() || index < 0 || index >= static_cast<int>(it->size()))
            return nullptr;
        auto member = it->begin();
        std::advance(member, index);
        if (!member->is_string())
            return nullptr;
        impl->TempString = member->get<std::string>();
        return impl->TempString.c_str();
    }

    const char* Json::FindKey(const char* name) const
    {
        if (!name || !impl->Data.is_object())
            return nullptr;
        for (auto member = impl->Data.begin(); member != impl->Data.end(); ++member)
        {
            if (_stricmp(member.key().c_str(), name) == 0)
            {
                impl->TempString = member.key();
                return impl->TempString.c_str();
            }
        }
        return nullptr;
    }

    int Json::GetArraySize(const char* key) const
    {
        auto it = impl->Data.find(key);
        return (it != impl->Data.end() && it->is_array()) ? static_cast<int>(it->size()) : 0;
    }

    bool Json::GetArrayItem(const char* key, int index, Json& out) const
    {
        auto it = impl->Data.find(key);
        if (it == impl->Data.end() || !it->is_array() || index < 0 || index >= static_cast<int>(it->size()))
            return false;
        const nlohmann::ordered_json& item = (*it)[static_cast<size_t>(index)];
        if (!item.is_object())
            return false;
        out.impl->Data = item;
        return true;
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
