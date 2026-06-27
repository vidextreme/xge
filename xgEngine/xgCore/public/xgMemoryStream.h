#pragma once
#include "xgStream.h"
#include <cstdint>
#include <cstddef>

namespace xg
{
    class MemoryStream : public MemoryStreamBase
    {
    public:
        MemoryStream();
        explicit MemoryStream(size_t capacity);
        ~MemoryStream() override;

        // Stream interface
        int Read(void* buffer, int size) override;
        int Write(const void* buffer, int size) override;

        bool Seek(long offset, FileOrigin origin) override;

        bool Eof() const override;
        long Length() const override;
        long Position() const override;

        void* GetBuffer() override { return _data; }
        const void* GetBuffer() const { return _data; }

        // MemoryStream-specific
        void Clear();
        void Resize(size_t newSize);

        void* Data() { return _data; }
        const void* Data() const { return _data; }
        int Size() const { return static_cast<int>(_size); }

    private:
        uint8_t* _data;
        size_t   _size;      // current size of valid data
        size_t   _capacity;  // allocated capacity
        size_t   _pos;       // read/write cursor

        void EnsureCapacity(size_t required);
    };
}
