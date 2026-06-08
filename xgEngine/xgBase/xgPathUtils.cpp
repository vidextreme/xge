#include "pch.h"
#include "public/xgPathUtils.h"

namespace xg
{
    const char* GetExtension(const char* path)
    {
        if (!path)
            return "";

        const char* dot = strrchr(path, '.');
        return dot ? dot : "";
    }

    const char* GetFilename(const char* path)
    {
        if (!path)
            return "";

        const char* slash1 = strrchr(path, '/');
        const char* slash2 = strrchr(path, '\\');
        const char* slash = (slash1 > slash2) ? slash1 : slash2;

        return slash ? slash + 1 : path;
    }

    const char* GetDirectory(const char* path)
    {
        static std::string buffer;

        if (!path)
        {
            buffer.clear();
            return buffer.c_str();
        }

        const char* slash1 = strrchr(path, '/');
        const char* slash2 = strrchr(path, '\\');
        const char* slash = (slash1 > slash2) ? slash1 : slash2;

        if (!slash)
        {
            buffer.clear();
            return buffer.c_str();
        }

        buffer.assign(path, slash - path);
        return buffer.c_str();
    }
}
