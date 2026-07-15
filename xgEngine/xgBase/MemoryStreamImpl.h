// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgStream.h"
#include <cstdint>
#include <cstddef>

namespace xg
{
    class MemoryStreamImpl : public MemoryStream
    {
    public:
        MemoryStreamImpl();                         // empty, no buffer
        explicit MemoryStreamImpl(size_t capacity); // allocates buffer
        MemoryStreamImpl(void* existingBuffer, size_t size); // external view
        ~MemoryStreamImpl() override;

        int  Read(void* buffer, int size) override;
        int  Write(const void* buffer, int size) override;

        bool Seek(long offset, FileOrigin origin) override;
        bool Eof() const override;

        long Length() const override;
        long Position() const override;

        void Clear() override;
        void Resize(size_t newSize) override;

        void* Data() override;
        const void* Data() const override;
        int         Size() const override;

        void* GetBuffer() override;

    private:
        void EnsureCapacity(size_t required);

        uint8_t* _data;
        size_t   _size;
        size_t   _capacity;
        size_t   _pos;
        bool     _ownsData;
    };
}
