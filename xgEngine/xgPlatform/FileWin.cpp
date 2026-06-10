#include "pch.h"
#include "xgModules.h"
#include "FileWin.h"

namespace xg
{
    struct FileWin::Impl
    {
        HANDLE Handle = INVALID_HANDLE_VALUE;
        std::wstring Path;
    };

    // ------------------------------------------------------------
    // Helpers
    // ------------------------------------------------------------
    static DWORD ToWinAccess(FileAccessMode mode)
    {
        DWORD access = 0;
        if (HasFlag(mode, FileAccessMode::Read))
            access |= GENERIC_READ;
        if (HasFlag(mode, FileAccessMode::Write))
            access |= GENERIC_WRITE;
        return access;
    }

    static DWORD ToWinCreate(FileAccessMode mode)
    {
        if (HasFlag(mode, FileAccessMode::Create))
            return CREATE_ALWAYS;
        return OPEN_EXISTING;
    }

    // ------------------------------------------------------------
    // Constructor / Destructor
    // ------------------------------------------------------------
    FileWin::FileWin(const wchar_t* path)
    {
        impl = new Impl();
        impl->Path = path;
    }

    FileWin::~FileWin()
    {
        Close();
        delete impl;
    }

    // ------------------------------------------------------------
    // File Operations
    // ------------------------------------------------------------
    bool FileWin::Open(FileAccessMode mode)
    {
        if (impl->Handle != INVALID_HANDLE_VALUE)
            return false;

        impl->Handle = ::CreateFileW(
            impl->Path.c_str(),
            ToWinAccess(mode),
            FILE_SHARE_READ,
            nullptr,
            ToWinCreate(mode),
            FILE_ATTRIBUTE_NORMAL,
            nullptr
        );

        return impl->Handle != INVALID_HANDLE_VALUE;
    }

    void FileWin::Close()
    {
        if (impl->Handle != INVALID_HANDLE_VALUE)
        {
            ::CloseHandle(impl->Handle);
            impl->Handle = INVALID_HANDLE_VALUE;
        }
    }

    void FileWin::Flush()
    {
        if (impl->Handle != INVALID_HANDLE_VALUE)
            ::FlushFileBuffers(impl->Handle);
    }

    void* FileWin::GetHandle()
    {
        return impl->Handle;
    }

    const wchar_t* FileWin::GetPath() const
    {
        return impl->Path.c_str();
    }

    // ------------------------------------------------------------
    // Stream Operations
    // ------------------------------------------------------------
    int FileWin::Read(void* buffer, int size)
    {
        DWORD bytesRead = 0;
        if (!::ReadFile(impl->Handle, buffer, size, &bytesRead, nullptr))
            return -1;
        return static_cast<int>(bytesRead);
    }

    int FileWin::Write(const void* buffer, int size)
    {
        DWORD bytesWritten = 0;
        if (!::WriteFile(impl->Handle, buffer, size, &bytesWritten, nullptr))
            return -1;
        return static_cast<int>(bytesWritten);
    }

    bool FileWin::Seek(long offset, FileOrigin origin)
    {
        DWORD method = FILE_BEGIN;
        if (origin == FileOrigin::Current) method = FILE_CURRENT;
        if (origin == FileOrigin::End)     method = FILE_END;

        DWORD result = ::SetFilePointer(impl->Handle, offset, nullptr, method);
        return result != INVALID_SET_FILE_POINTER;
    }

    long FileWin::Position() const
    {
        return ::SetFilePointer(impl->Handle, 0, nullptr, FILE_CURRENT);
    }

    long FileWin::Length() const
    {
        return static_cast<long>(::GetFileSize(impl->Handle, nullptr));
    }

    bool FileWin::Eof() const
    {
        return Position() == Length();
    }
}
