#pragma once

#include <memory>


template <class T>
class Buffer
{
public:
    Buffer() = default;
    ~Buffer() = default;

    explicit Buffer(UINT size)
    {
        ExpandIfNeeded(size);
    }

    bool Empty() const
    {
        return size_ == 0;
    }

    void ExpandIfNeeded(UINT size)
    {
        if (size > size_)
        {
            size_ = size;
            value_ = std::make_unique<T[]>(size);
        }
    }

    void Clear()
    {
        ZeroMemory(value_.get(), size_);
    }

    void Clear(int value)
    {
        memset(value_.get(), value, sizeof(T) * size_);
    }

    void Reset()
    {
        value_.reset();
        size_ = 0;
    }

    UINT Size() const
    {
        return size_;
    }

    T* Get() const
    {
        return value_.get();
    }

    T* Get(UINT offset) const
    {
        return (value_.get() + offset);
    }

    template <class U>
    U* As() const
    {
        return reinterpret_cast<U*>(Get());
    }

    template <class U>
    U* As(UINT offset) const
    {
        return reinterpret_cast<U*>(Get(offset));
    }

    operator bool() const
    {
        return value_ != nullptr;
    }

    const T& operator [](UINT index) const
    {
        if (index >= size_)
        {
            Debug::Error("Array index out of range: ", index, size_);
            return value_[0];
        }
        return value_[index];
    }

    T& operator [](UINT index)
    {
        if (index >= size_)
        {
            Debug::Error("Array index out of range: ", index, size_);
            return value_[0];
        }
        return value_[index];
    }

private:
    std::unique_ptr<T[]> value_;
    UINT size_ = 0;
};