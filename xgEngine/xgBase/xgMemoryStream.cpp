#include "pch.h"
#include "xgMemoryStream.h"
#include <cstring> // memcpy

namespace xg
{
    // ------------------------------------------------------------
    // Constructors / Destructor
    // ------------------------------------------------------------

    MemoryStream::MemoryStream()
        : _data(nullptr)
        , _size(0)
        , _capacity(0)
        , _pos(0)
        , _ownsData(false)
    {}

    MemoryStream::MemoryStream(size_t capacity)
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

    MemoryStream::MemoryStream(void* existingBuffer, size_t size)
        : _data(reinterpret_cast<uint8_t*>(existingBuffer))
        , _size(size)
        , _capacity(size)
        , _pos(0)
        , _ownsData(false) // external buffer — DO NOT free
    {}

    MemoryStream::~MemoryStream()
    {
        if (_ownsData && _data)
            free(_data);
    }

    // ------------------------------------------------------------
    // Internal capacity management
    // ------------------------------------------------------------

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

        if (_ownsData)
            free(_data);

        _data = newData;
        _capacity = newCap;
        _ownsData = true;
    }

    // ------------------------------------------------------------
    // Read / Write
    // ------------------------------------------------------------

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

    // ------------------------------------------------------------
    // Seeking
    // ------------------------------------------------------------

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

    // ------------------------------------------------------------
    // Status
    // ------------------------------------------------------------

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

    // ------------------------------------------------------------
    // Buffer management
    // ------------------------------------------------------------

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

    // ------------------------------------------------------------
    // Data access
    // ------------------------------------------------------------

    void* MemoryStream::Data()
    {
        return _data;
    }

    const void* MemoryStream::Data() const
    {
        return _data;
    }

    int MemoryStream::Size() const
    {
        return (int)_size;
    }

    void* MemoryStream::GetBuffer()
    {
        return _data;
    }

    // ------------------------------------------------------------
    // Factory
    // ------------------------------------------------------------

    static void DeleteMemoryStream(void* ptr)
    {
        delete static_cast<MemoryStream*>(ptr);
    }

    xgUnique<MemoryStreamBase> CreateMemoryStream(uint64_t capacity)
    {
        return xgUnique<MemoryStreamBase>(
            xgUniqueHandle{ new MemoryStream((size_t)capacity), &DeleteMemoryStream }
        );
    }
}
