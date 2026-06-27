#include "pch.h"
#include "xgMemoryStream.h"
#include <cstring> // memcpy

namespace xg
{
    MemoryStream::MemoryStream()
        : _data(nullptr), _size(0), _capacity(0), _pos(0)
    {}

    MemoryStream::MemoryStream(size_t capacity)
        : _data(nullptr), _size(0), _capacity(0), _pos(0)
    {
        if (capacity > 0)
        {
            _data = (uint8_t*)malloc(capacity);
            _capacity = capacity;
        }
    }

    MemoryStream::~MemoryStream()
    {
        if (_data)
            free(_data);
    }

    void MemoryStream::EnsureCapacity(size_t required)
    {
        if (required <= _capacity)
            return;

        size_t newCap = (_capacity == 0) ? 256 : _capacity;
        while (newCap < required)
            newCap *= 2;

        uint8_t* newData = (uint8_t*)malloc(newCap);

        if (_data && _size > 0)
            memcpy(newData, _data, _size);

        free(_data);
        _data = newData;
        _capacity = newCap;
    }

    int MemoryStream::Read(void* buffer, int size)
    {
        if (_pos >= _size)
            return 0;

        size_t remaining = _size - _pos;
        size_t toRead = (remaining < (size_t)size) ? remaining : (size_t)size;

        memcpy(buffer, _data + _pos, toRead);
        _pos += toRead;
        return (int)toRead;
    }

    int MemoryStream::Write(const void* buffer, int size)
    {
        size_t endPos = _pos + size;
        EnsureCapacity(endPos);

        memcpy(_data + _pos, buffer, size);
        _pos = endPos;

        if (_pos > _size)
            _size = _pos;

        return size;
    }

    bool MemoryStream::Seek(long offset, FileOrigin origin)
    {
        size_t newPos = 0;

        switch (origin)
        {
        case FileOrigin::Begin:
            newPos = (size_t)offset;
            break;

        case FileOrigin::Current:
            newPos = _pos + offset;
            break;

        case FileOrigin::End:
            newPos = _size + offset;
            break;
        }

        if (newPos > _size)
            return false;

        _pos = newPos;
        return true;
    }

    bool MemoryStream::Eof() const
    {
        return _pos >= _size;
    }

    long MemoryStream::Length() const
    {
        return (long)_size;
    }

    long MemoryStream::Position() const
    {
        return (long)_pos;
    }

    void MemoryStream::Clear()
    {
        _size = 0;
        _pos = 0;
    }

    void MemoryStream::Resize(size_t newSize)
    {
        EnsureCapacity(newSize);
        _size = newSize;
        if (_pos > _size)
            _pos = _size;
    }

    // Factory helper
    std::unique_ptr<MemoryStreamBase> CreateMemoryStream(uint64_t capacity)
    {
        return std::make_unique<MemoryStream>(capacity);
    }
}
