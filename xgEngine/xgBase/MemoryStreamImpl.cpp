// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "MemoryStreamImpl.h"


namespace xg
{
    // ------------------------------------------------------------
    // Constructors / Destructor
    // ------------------------------------------------------------

    MemoryStreamImpl::MemoryStreamImpl()
        : _data(nullptr)
        , _size(0)
        , _capacity(0)
        , _pos(0)
        , _ownsData(false)
    {}

    MemoryStreamImpl::MemoryStreamImpl(size_t capacity)
        : _data(nullptr)
        , _size(0)
        , _capacity(0)
        , _pos(0)
        , _ownsData(false)
    {
        if (capacity > 0)
        {
            _data = (uint8_t*)malloc(capacity);
            _capacity = capacity;
            _ownsData = true;
        }
    }

    MemoryStreamImpl::MemoryStreamImpl(void* existingBuffer, size_t size)
        : _data(reinterpret_cast<uint8_t*>(existingBuffer))
        , _size(size)
        , _capacity(size)
        , _pos(0)
        , _ownsData(false) // external buffer — DO NOT free
    {}

    MemoryStreamImpl::~MemoryStreamImpl()
    {
        if (_ownsData && _data)
            free(_data);
    }

    // ------------------------------------------------------------
    // Internal capacity management
    // ------------------------------------------------------------

    void MemoryStreamImpl::EnsureCapacity(size_t required)
    {
        if (required <= _capacity)
            return;

        size_t newCap = (_capacity == 0) ? 256 : _capacity;
        while (newCap < required)
            newCap *= 2;

        uint8_t* newData = (uint8_t*)malloc(newCap);

        if (_data && _size > 0)
            memcpy(newData, _data, _size);

        if (_ownsData)
            free(_data);

        _data = newData;
        _capacity = newCap;
        _ownsData = true;
    }

    // ------------------------------------------------------------
    // Read / Write
    // ------------------------------------------------------------

    int MemoryStreamImpl::Read(void* buffer, int size)
    {
        if (_pos >= _size)
            return 0;

        size_t remaining = _size - _pos;
        size_t toRead = (remaining < (size_t)size) ? remaining : (size_t)size;

        memcpy(buffer, _data + _pos, toRead);
        _pos += toRead;
        return (int)toRead;
    }

    int MemoryStreamImpl::Write(const void* buffer, int size)
    {
        size_t endPos = _pos + size;
        EnsureCapacity(endPos);

        memcpy(_data + _pos, buffer, size);
        _pos = endPos;

        if (_pos > _size)
            _size = _pos;

        return size;
    }

    // ------------------------------------------------------------
    // Seeking
    // ------------------------------------------------------------

    bool MemoryStreamImpl::Seek(long offset, FileOrigin origin)
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

    // ------------------------------------------------------------
    // Status
    // ------------------------------------------------------------

    bool MemoryStreamImpl::Eof() const
    {
        return _pos >= _size;
    }

    long MemoryStreamImpl::Length() const
    {
        return (long)_size;
    }

    long MemoryStreamImpl::Position() const
    {
        return (long)_pos;
    }

    // ------------------------------------------------------------
    // Buffer management
    // ------------------------------------------------------------

    void MemoryStreamImpl::Clear()
    {
        _size = 0;
        _pos = 0;
    }

    void MemoryStreamImpl::Resize(size_t newSize)
    {
        EnsureCapacity(newSize);
        _size = newSize;

        if (_pos > _size)
            _pos = _size;
    }

    // ------------------------------------------------------------
    // Data access
    // ------------------------------------------------------------

    void* MemoryStreamImpl::Data()
    {
        return _data;
    }

    const void* MemoryStreamImpl::Data() const
    {
        return _data;
    }

    int MemoryStreamImpl::Size() const
    {
        return (int)_size;
    }

    void* MemoryStreamImpl::GetBuffer()
    {
        return _data;
    }

    // ------------------------------------------------------------
    // Factory
    // ------------------------------------------------------------


    XG_DECLARE_UNIQUE_TYPE(MemoryStreamImpl)
    MemoryStreamUnique CreateMemoryStream(uint64_t capacity)
    {
        return MemoryStreamUnique(
            xgUniqueHandle{ MakeMemoryStreamImplUniqueHandle(new MemoryStreamImpl(capacity)) }
        );
    }

    MemoryStreamUnique CreateMemoryStream(void* existingBuffer, size_t size)
    {
        return MemoryStreamUnique(
            xgUniqueHandle{ MakeMemoryStreamImplUniqueHandle(new MemoryStreamImpl(existingBuffer, size)) }
        );
    }
}
