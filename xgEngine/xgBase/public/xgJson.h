// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgStream.h"

namespace xg
{
    class Json
    {
    public:
        Json();
        ~Json();
        Json(const Json&) = delete;
        Json& operator=(const Json&) = delete;

        bool Load(Stream& stream);
        bool Save(Stream& stream) const;

        // Accessors
        bool GetBool(const char* key, bool defaultValue = false) const;
        int GetInt(const char* key, int defaultValue = 0) const;
        float GetFloat(const char* key, float defaultValue = 0.0f) const;
        const char* GetString(const char* key, const char* defaultValue = "") const;
        void SetBool(const char* key, bool value);
        void SetInt(const char* key, int value);
        void SetFloat(const char* key, float value);
        void SetString(const char* key, const char* value);

        bool Has(const char* key) const;

        // Copies the object stored under key into out; false when key is missing or not an object.
        bool GetTable(const char* key, Json& out) const;
        // Members of the object stored under key, in file order. GetObjectKey returns null and GetObjectItem false
        // when key is missing, not an object or index is out of range; GetObjectItem copies object values only and
        // GetObjectString returns string values only (null otherwise).
        int GetObjectSize(const char* key) const;
        const char* GetObjectKey(const char* key, int index) const;
        bool GetObjectItem(const char* key, int index, Json& out) const;
        const char* GetObjectString(const char* key, int index) const;
        // This document's key that matches name ignoring case, or null.
        const char* FindKey(const char* name) const;
        // Number of elements of the array stored under key; 0 when key is missing or not an array.
        int GetArraySize(const char* key) const;
        // Copies the object at index of the array stored under key into out; false when it is not an object.
        bool GetArrayItem(const char* key, int index, Json& out) const;
        const char* ToString() const;

    private:
        struct Impl;
        Impl* impl;
    };
}
