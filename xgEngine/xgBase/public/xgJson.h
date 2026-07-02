#pragma once
#include "xgStream.h"

namespace xg
{
    class Json
    {
    public:
        Json();
        ~Json();

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
        const char* ToString() const;

    private:
        struct Impl;
        Impl* impl;
    };
}
