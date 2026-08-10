#include "pch.h"
#include "StridePool.h"

namespace xg
{

    StridePool::StridePool(std::size_t stride)
        : _stride(stride)
    {}

    int StridePool::Add(ActorID actorId)
    {
        const int index = static_cast<int>(_actorIds.size());
        _actorIds.push_back(actorId);

        const std::size_t oldSize = _buffer.size();
        _buffer.resize(oldSize + _stride);

        return index;
    }

    int StridePool::AddCopy(ActorID actorId, const void* src)
    {
        const int index = static_cast<int>(_actorIds.size());
        _actorIds.push_back(actorId);

        const std::size_t oldSize = _buffer.size();
        _buffer.resize(oldSize + _stride);

        std::memcpy(_buffer.data() + index * _stride, src, _stride);

        return index;
    }

    void StridePool::RemoveAt(int index)
    {
        const int last = static_cast<int>(_actorIds.size()) - 1;
        if (index != last)
        {
            _actorIds[index] = _actorIds[last];

            std::memcpy(
                _buffer.data() + index * _stride,
                _buffer.data() + last * _stride,
                _stride
            );
        }

        _actorIds.pop_back();
        _buffer.resize(_buffer.size() - _stride);
    }

    void* StridePool::GetPtr(int index)
    {
        return _buffer.data() + index * _stride;
    }

    int StridePool::Find(ActorID actorId) const
    {
        for (int i = 0; i < static_cast<int>(_actorIds.size()); ++i)
        {
            if (_actorIds[i] == actorId)
                return i;
        }
        return -1;
    }

} // namespace xg
