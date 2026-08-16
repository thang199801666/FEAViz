// FEAViz C++ binding - RAII object wrapper.
//
// Owns a reference to any FEAViz object through the C refcount
// (fviz_retain / fviz_release). Copying retains, moving transfers, destruction
// releases, matching the C ownership rules.

#ifndef FVIZ_CPP_OBJECT_HPP
#define FVIZ_CPP_OBJECT_HPP

#include <FViz/FViz.h>

#include <cstring>
#include <stdexcept>
#include <string>
#include <utility>

namespace fviz {

// ---------------------------------------------------------------------------
// Exception carrying the C error code and message.
// ---------------------------------------------------------------------------
class Error : public std::runtime_error {
public:
    explicit Error(FVizResult code)
        : std::runtime_error(fviz_result_string(code) != nullptr ? fviz_result_string(code) : "FEAViz error"),
          code_(code) {}

    FVizResult code() const noexcept { return code_; }
    bool ok() const noexcept { return code_ == FVIZ_OK; }

private:
    FVizResult code_;
};

namespace detail {

// Throws if result != FVIZ_OK. Appends the last error message when available.
inline void checkResult(FVizResult result)
{
    if (result != FVIZ_OK)
        throw Error(result);
}

// Converts a C pointer to the RAII owning pointer for T.
template <typename T>
inline T* adopt(void* raw) noexcept
{
    return static_cast<T*>(raw);
}

// Boolean -> FVizBool (uint8_t) without narrowing warnings on /W4.
inline FVizBool fbool(bool value) noexcept
{
    return value ? static_cast<FVizBool>(1) : static_cast<FVizBool>(0);
}

} // namespace detail

// ---------------------------------------------------------------------------
// Object - RAII owner of a single reference to a FEAViz object.
// ---------------------------------------------------------------------------
template <typename T>
class Object {
public:
    Object() noexcept = default;
    ~Object() { releaseRef(); }

    // Adopt an already-owned reference (the typical result of fviz_*_create).
    explicit Object(T* owned) noexcept : ptr_(owned) {}

    // Adopt from void* (create functions return T**; callers cast).
    explicit Object(void* owned) noexcept : ptr_(static_cast<T*>(owned)) {}

    Object(const Object& other) noexcept
        : ptr_(other.ptr_ != nullptr ? static_cast<T*>(fviz_retain(other.ptr_)) : nullptr)
    {
    }

    Object& operator=(const Object& other) noexcept
    {
        if (this != &other)
        {
            T* next = other.ptr_ != nullptr ? static_cast<T*>(fviz_retain(other.ptr_)) : nullptr;
            releaseRef();
            ptr_ = next;
        }
        return *this;
    }

    Object(Object&& other) noexcept : ptr_(other.ptr_) { other.ptr_ = nullptr; }

    Object& operator=(Object&& other) noexcept
    {
        if (this != &other)
        {
            releaseRef();
            ptr_ = other.ptr_;
            other.ptr_ = nullptr;
        }
        return *this;
    }

    T* get() const noexcept { return ptr_; }
    T* operator->() const noexcept { return ptr_; }
    T& operator*() const noexcept { return *ptr_; }

    explicit operator bool() const noexcept { return ptr_ != nullptr; }
    bool operator==(const Object& other) const noexcept { return ptr_ == other.ptr_; }
    bool operator!=(const Object& other) const noexcept { return ptr_ != other.ptr_; }

    T* release() noexcept
    {
        T* out = ptr_;
        ptr_ = nullptr;
        return out;
    }

    // Returns a retained reference (caller owns the returned pointer).
    T* retain() const noexcept
    {
        return ptr_ != nullptr ? static_cast<T*>(fviz_retain(ptr_)) : nullptr;
    }

    void reset() noexcept { releaseRef(); }

    FVizTypeId typeId() const noexcept
    {
        return ptr_ != nullptr ? fviz_object_type_id(reinterpret_cast<const FVizObject*>(ptr_)) : FVIZ_TYPE_OBJECT;
    }

    const char* typeName() const noexcept
    {
        return ptr_ != nullptr ? fviz_object_type_name(reinterpret_cast<const FVizObject*>(ptr_)) : "";
    }

    bool isType(FVizTypeId type) const noexcept
    {
        return ptr_ != nullptr &&
            fviz_object_is_type(reinterpret_cast<const FVizObject*>(ptr_), type) != FVIZ_FALSE;
    }

    uint32_t refCount() const noexcept
    {
        return ptr_ != nullptr ? fviz_object_ref_count(reinterpret_cast<const FVizObject*>(ptr_)) : 0u;
    }

    FVizMTime mtime() const noexcept
    {
        return ptr_ != nullptr ? fviz_object_mtime(reinterpret_cast<const FVizObject*>(ptr_)) : 0u;
    }

    void modified() noexcept
    {
        if (ptr_ != nullptr)
            fviz_object_modified(reinterpret_cast<FVizObject*>(ptr_));
    }

protected:
    void releaseRef() noexcept
    {
        if (ptr_ != nullptr)
        {
            fviz_release(ptr_);
            ptr_ = nullptr;
        }
    }

    T* ptr_ = nullptr;
};

} // namespace fviz

#endif // FVIZ_CPP_OBJECT_HPP
