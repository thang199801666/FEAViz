#ifndef FVIZ_INTERNAL_CORE_ATOMIC_H
#define FVIZ_INTERNAL_CORE_ATOMIC_H

#include <stdint.h>

#if defined(_MSC_VER)
    #include <intrin.h>

typedef struct FVizAtomicU32
{
    volatile long value;
} FVizAtomicU32;

typedef struct FVizSpinLock
{
    volatile long value;
} FVizSpinLock;

static __forceinline uint32_t fviz_atomic_u32_load(const FVizAtomicU32* atomic_value)
{
    return (uint32_t)_InterlockedCompareExchange(
        (volatile long*)&atomic_value->value,
        0,
        0);
}

static __forceinline uint32_t fviz_atomic_u32_fetch_add(FVizAtomicU32* atomic_value, uint32_t value)
{
    return (uint32_t)_InterlockedExchangeAdd(&atomic_value->value, (long)value);
}

static __forceinline uint32_t fviz_atomic_u32_fetch_sub(FVizAtomicU32* atomic_value, uint32_t value)
{
    return (uint32_t)_InterlockedExchangeAdd(&atomic_value->value, -(long)value);
}

static __forceinline int fviz_atomic_u32_compare_exchange(
    FVizAtomicU32* atomic_value,
    uint32_t* expected,
    uint32_t desired)
{
    const long previous = _InterlockedCompareExchange(
        &atomic_value->value,
        (long)desired,
        (long)*expected);
    if ((uint32_t)previous == *expected)
    {
        return 1;
    }
    *expected = (uint32_t)previous;
    return 0;
}

static __forceinline void fviz_spin_lock(FVizSpinLock* lock)
{
    while (_InterlockedCompareExchange(&lock->value, 1, 0) != 0)
    {
        _ReadWriteBarrier();
    }
}

static __forceinline void fviz_spin_unlock(FVizSpinLock* lock)
{
    _InterlockedExchange(&lock->value, 0);
}

#elif defined(__GNUC__) || defined(__clang__)

typedef struct FVizAtomicU32
{
    uint32_t value;
} FVizAtomicU32;

typedef struct FVizSpinLock
{
    uint32_t value;
} FVizSpinLock;

static inline uint32_t fviz_atomic_u32_load(const FVizAtomicU32* atomic_value)
{
    return __atomic_load_n(&atomic_value->value, __ATOMIC_ACQUIRE);
}

static inline uint32_t fviz_atomic_u32_fetch_add(FVizAtomicU32* atomic_value, uint32_t value)
{
    return __atomic_fetch_add(&atomic_value->value, value, __ATOMIC_ACQ_REL);
}

static inline uint32_t fviz_atomic_u32_fetch_sub(FVizAtomicU32* atomic_value, uint32_t value)
{
    return __atomic_fetch_sub(&atomic_value->value, value, __ATOMIC_ACQ_REL);
}

static inline int fviz_atomic_u32_compare_exchange(
    FVizAtomicU32* atomic_value,
    uint32_t* expected,
    uint32_t desired)
{
    return __atomic_compare_exchange_n(
        &atomic_value->value,
        expected,
        desired,
        0,
        __ATOMIC_ACQ_REL,
        __ATOMIC_ACQUIRE);
}

static inline void fviz_spin_lock(FVizSpinLock* lock)
{
    uint32_t expected;
    do
    {
        expected = 0;
    }
    while (!__atomic_compare_exchange_n(
        &lock->value,
        &expected,
        1,
        0,
        __ATOMIC_ACQUIRE,
        __ATOMIC_RELAXED));
}

static inline void fviz_spin_unlock(FVizSpinLock* lock)
{
    __atomic_store_n(&lock->value, 0, __ATOMIC_RELEASE);
}

#else
    #error "FEAViz Phase 1 requires an atomic implementation for this compiler."
#endif

#endif /* FVIZ_INTERNAL_CORE_ATOMIC_H */
