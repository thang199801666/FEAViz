#ifndef FVIZ_INTERNAL_CORE_COMPILER_H
#define FVIZ_INTERNAL_CORE_COMPILER_H

#include <stddef.h>
#include <stdint.h>

/*
 * Portable maximum fundamental alignment used by FEAViz internals.
 *
 * MSVC 19.50 in C17 mode does not expose the C11 max_align_t typedef in
 * every CRT/header configuration.  Do not make the runtime allocator depend
 * on that typedef.  The union below has the strictest alignment required by
 * the fundamental scalar categories used by the C ABI.
 */
typedef union FVizInternalMaxAlign
{
    long double long_double_value;
    long long long_long_value;
    void* pointer_value;
    uint64_t uint64_value;
} FVizInternalMaxAlign;

#if defined(_MSC_VER)
#define FVIZ_INTERNAL_ALIGNOF(type) ((size_t)__alignof(type))
#else
#define FVIZ_INTERNAL_ALIGNOF(type) ((size_t)_Alignof(type))
#endif

#define FVIZ_INTERNAL_MAX_ALIGNMENT FVIZ_INTERNAL_ALIGNOF(FVizInternalMaxAlign)

#if defined(_MSC_VER)
#define FVIZ_INTERNAL_ASSUME(expr) __assume(expr)
#define FVIZ_THREAD_LOCAL __declspec(thread)
#elif defined(__clang__)
#define FVIZ_INTERNAL_ASSUME(expr) __builtin_assume(expr)
#define FVIZ_THREAD_LOCAL _Thread_local
#elif defined(__GNUC__)
#define FVIZ_INTERNAL_ASSUME(expr)                                                                                     \
    do                                                                                                                 \
    {                                                                                                                  \
        if (!(expr)) __builtin_unreachable();                                                                          \
    } while (0)
#define FVIZ_THREAD_LOCAL _Thread_local
#else
#define FVIZ_INTERNAL_ASSUME(expr) ((void)0)
#define FVIZ_THREAD_LOCAL _Thread_local
#endif

#if defined(__GNUC__) || defined(__clang__)
#define FVIZ_INTERNAL_LIKELY(x) __builtin_expect(!!(x), 1)
#define FVIZ_INTERNAL_UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
#define FVIZ_INTERNAL_LIKELY(x) (x)
#define FVIZ_INTERNAL_UNLIKELY(x) (x)
#endif

#endif /* FVIZ_INTERNAL_CORE_COMPILER_H */
