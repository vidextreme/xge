// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgJSON.h"
#include <json.hpp>
#include <cstring>
#include <iterator>
#include <vector>
#include <string>

namespace xg
{
    struct JSON::Impl
    {
        nlohmann::ordered_json Data;
        std::string TempString; // used to return stable const char*
    };

    JSON::JSON()
    {
        impl = new Impl();
    }

    JSON::~JSON()
    {
        delete impl;
    }

    bool JSON::Load(Stream& stream)
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

    bool JSON::Save(Stream& stream) const
    {
        // Convert JSON to string
        impl->TempString = impl->Data.dump(4);
        const char* text = impl->TempString.c_str();
        size_t len = impl->TempString.size();

        // Write to stream
        int written = stream.Write(text, (int)len);
        return written == (int)len;
    }


    bool JSON::GetBool(const char* key, bool defaultValue) const
    {
        auto it = impl->Data.find(key);
        if (it != impl->Data.end() && it->is_boolean())
            return it->get<bool>();
        return defaultValue;
    }

    int JSON::GetInt(const char* key, int defaultValue) const
    {
        auto it = impl->Data.find(key);
        if (it != impl->Data.end() && it->is_number())
            return it->get<int>();
        return defaultValue;
    }

    float JSON::GetFloat(const char* key, float defaultValue) const
    {
        auto it = impl->Data.find(key);
        if (it != impl->Data.end() && it->is_number())
            return it->get<float>();
        return defaultValue;
    }

    const char* JSON::GetString(const char* key, const char* defaultValue) const
    {
        auto it = impl->Data.find(key);
        if (it != impl->Data.end() && it->is_string())
        {
            impl->TempString = it->get<std::string>();
            return impl->TempString.c_str();
        }
        return defaultValue;
    }

    bool JSON::Has(const char* key) const
    {
        return impl->Data.contains(key);
    }

    bool JSON::GetTable(const char* key, JSON& out) const
    {
        auto it = impl->Data.find(key);
        if (it == impl->Data.end() || !it->is_object())
            return false;
        out.impl->Data = *it;
        return true;
    }

    int JSON::GetObjectSize(const char* key) const
    {
        auto it = impl->Data.find(key);
        return (it != impl->Data.end() && it->is_object()) ? static_cast<int>(it->size()) : 0;
    }

    const char* JSON::GetObjectKey(const char* key, int index) const
    {
        auto it = impl->Data.find(key);
        if (it == impl->Data.end() || !it->is_object() || index < 0 || index >= static_cast<int>(it->size()))
            return nullptr;
        auto member = it->begin();
        std::advance(member, index);
        impl->TempString = member.key();
        return impl->TempString.c_str();
    }

    bool JSON::GetObjectItem(const char* key, int index, JSON& out) const
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

    const char* JSON::GetObjectString(const char* key, int index) const
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

    const char* JSON::FindKey(const char* name) const
    {
        if (!name || !impl->Data.is_object())
            return nullptr;
        for (auto member = impl->Data.begin(); member != impl->Data.end(); ++member)
        {
            if (xg::StrICmp(member.key().c_str(), name) == 0)
            {
                impl->TempString = member.key();
                return impl->TempString.c_str();
            }
        }
        return nullptr;
    }

    int JSON::GetArraySize(const char* key) const
    {
        auto it = impl->Data.find(key);
        return (it != impl->Data.end() && it->is_array()) ? static_cast<int>(it->size()) : 0;
    }

    bool JSON::GetArrayItem(const char* key, int index, JSON& out) const
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


    void JSON::SetBool(const char* key, bool value)
    {
        impl->Data[key] = value;
    }

    void JSON::SetInt(const char* key, int value)
    {
        impl->Data[key] = value;
    }

    void JSON::SetFloat(const char* key, float value)
    {
        impl->Data[key] = value;
    }

    void JSON::SetString(const char* key, const char* value)
    {
        impl->Data[key] = value ? value : "";
    }

    const char* JSON::ToString() const
    {
        impl->TempString = impl->Data.dump(4); // pretty print
        return impl->TempString.c_str();
    }


}
