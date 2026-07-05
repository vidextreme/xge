#pragma once
#include "xgHandles.h"
#include <cstdint>
#include <cstddef>
namespace xg
{
    // ------------------------------------------------------------
    // File Access Mode
    // ------------------------------------------------------------
    enum class FileAccessMode : uint8_t
    {
        Create = 0x01,
        Read = 0x02,
        Write = 0x04,
        ReadWrite = Read | Write
    };

    XG_ENABLE_FLAGS(FileAccessMode)

    // ------------------------------------------------------------
    // File Seek Origin
    // ------------------------------------------------------------
    enum class FileOrigin : uint8_t
    {
        Begin,
        Current,
        End
    };

    // ------------------------------------------------------------
    // Base Stream Interface
    // ------------------------------------------------------------
    class Stream
    {
    public:
        virtual ~Stream() = default;

        virtual int Read(void* buffer, int size) = 0;
        virtual int Write(const void* buffer, int size) = 0;

        virtual bool Seek(long offset, FileOrigin origin) = 0;

        virtual bool Eof() const = 0;
        virtual long Length() const = 0;
        virtual long Position() const = 0;

        // Optional for memory streams
        virtual void* GetBuffer() { return nullptr; }
    };

    // ------------------------------------------------------------
    // Memory Stream Interface
    // ------------------------------------------------------------
    class MemoryStreamBase : public Stream
    {
    public:
        virtual ~MemoryStreamBase() = default;

        virtual void* Data() = 0;
        virtual const void* Data() const = 0;
        virtual int Size() const = 0;
        virtual void Clear() = 0;
        virtual void Resize(size_t newSize) = 0;
    };

    XG_DECLARE_UNIQUE(MemoryStreamBase)

    // ------------------------------------------------------------
    // File Stream Interface
    // ------------------------------------------------------------
    class File : public Stream
    {
    public:
        virtual ~File() = default;

        virtual const wchar_t* GetPath() const = 0;
        virtual bool Open(FileAccessMode mode) = 0;
        virtual void Close() = 0;
        virtual void Flush() = 0;
        virtual void* GetHandle() = 0;
    };

    XG_DECLARE_UNIQUE(File)

    // ------------------------------------------------------------
    // Factory Functions
    // ------------------------------------------------------------
    XG_API FileUnique LoadFile(const wchar_t* path, FileAccessMode mode);
    XG_API bool GetExeRootDirectory(wchar_t* output, const wchar_t* localizedPath);

    XG_API MemoryStreamBaseUnique CreateMemoryStream(uint64_t capacity = 256);
}
