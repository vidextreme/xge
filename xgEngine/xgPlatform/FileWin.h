#pragma once
#include "xgStream.h"

namespace xg
{
    class FileWin : public File
    {
    public:
        explicit FileWin(const wchar_t* path);
        ~FileWin() override;

        // Stream
        int Read(void* buffer, int size) override;
        int Write(const void* buffer, int size) override;
        bool Seek(long offset, FileOrigin origin) override;
        bool Eof() const override;
        long Length() const override;
        long Position() const override;

        // File
        bool Open(FileAccessMode mode) override;
        void Close() override;
        void Flush() override;
        void* GetHandle() override;
        const wchar_t* GetPath() const override;

        void* GetBuffer() override { return nullptr; }

    private:
        struct Impl;
        Impl* impl;
    };
}
