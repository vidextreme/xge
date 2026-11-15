// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgStream.h"

namespace xg
{
    class FilePosix : public File
    {
    public:
        explicit FilePosix(const char* path);
        ~FilePosix() override;

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
        const char* GetPath() const override;

        void* GetBuffer() override { return nullptr; }

    private:
        struct Impl;
        Impl* impl;
    };
}
