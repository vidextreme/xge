// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "FilePosix.h"

#if !XG_PLATFORM_WINDOWS
#include <cstdio>

namespace xg
{
    struct FilePosix::Impl
    {
        FILE* Handle = nullptr;
        std::string Path;
    };

    // ------------------------------------------------------------
    // Helpers
    // ------------------------------------------------------------
    static const char* ToOpenMode(FileAccessMode mode)
    {
        const bool read = HasFlag(mode, FileAccessMode::Read);
        const bool write = HasFlag(mode, FileAccessMode::Write);
        if (HasFlag(mode, FileAccessMode::Create))
            return read ? "w+b" : "wb";
        if (read && write)
            return "r+b";
        return write ? "r+b" : "rb";
    }

    // ------------------------------------------------------------
    // Constructor / Destructor
    // ------------------------------------------------------------
    FilePosix::FilePosix(const char* path)
    {
        impl = new Impl();
        impl->Path = path ? path : "";
    }

    FilePosix::~FilePosix()
    {
        Close();
        delete impl;
    }

    // ------------------------------------------------------------
    // File Operations
    // ------------------------------------------------------------
    bool FilePosix::Open(FileAccessMode mode)
    {
        if (impl->Handle)
            return false;

        impl->Handle = std::fopen(impl->Path.c_str(), ToOpenMode(mode));
        return impl->Handle != nullptr;
    }

    void FilePosix::Close()
    {
        if (impl->Handle)
        {
            std::fclose(impl->Handle);
            impl->Handle = nullptr;
        }
    }

    void FilePosix::Flush()
    {
        if (impl->Handle)
            std::fflush(impl->Handle);
    }

    void* FilePosix::GetHandle()
    {
        return impl->Handle;
    }

    const char* FilePosix::GetPath() const
    {
        return impl->Path.c_str();
    }

    // ------------------------------------------------------------
    // Stream Operations
    // ------------------------------------------------------------
    int FilePosix::Read(void* buffer, int size)
    {
        if (!impl->Handle || size < 0)
            return -1;
        const size_t count = std::fread(buffer, 1, static_cast<size_t>(size), impl->Handle);
        if (count == 0 && std::ferror(impl->Handle))
            return -1;
        return static_cast<int>(count);
    }

    int FilePosix::Write(const void* buffer, int size)
    {
        if (!impl->Handle || size < 0)
            return -1;
        const size_t count = std::fwrite(buffer, 1, static_cast<size_t>(size), impl->Handle);
        if (count < static_cast<size_t>(size) && std::ferror(impl->Handle))
            return -1;
        return static_cast<int>(count);
    }

    bool FilePosix::Seek(long offset, FileOrigin origin)
    {
        if (!impl->Handle)
            return false;
        int whence = SEEK_SET;
        if (origin == FileOrigin::Current) whence = SEEK_CUR;
        if (origin == FileOrigin::End)     whence = SEEK_END;
        return std::fseek(impl->Handle, offset, whence) == 0;
    }

    long FilePosix::Position() const
    {
        return impl->Handle ? std::ftell(impl->Handle) : -1;
    }

    long FilePosix::Length() const
    {
        if (!impl->Handle)
            return 0;
        const long current = std::ftell(impl->Handle);
        std::fseek(impl->Handle, 0, SEEK_END);
        const long length = std::ftell(impl->Handle);
        std::fseek(impl->Handle, current, SEEK_SET);
        return length;
    }

    bool FilePosix::Eof() const
    {
        return Position() == Length();
    }
}
#endif
