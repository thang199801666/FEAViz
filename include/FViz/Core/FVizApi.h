#ifndef FVIZ_CORE_API_H
#define FVIZ_CORE_API_H

#if defined(_WIN32) || defined(__CYGWIN__)
    #if defined(FVIZ_SHARED)
        #if defined(FVIZ_BUILDING_LIBRARY)
            #define FVIZ_API __declspec(dllexport)
        #else
            #define FVIZ_API __declspec(dllimport)
        #endif
    #else
        #define FVIZ_API
    #endif
    #define FVIZ_LOCAL
#else
    #if defined(__GNUC__) || defined(__clang__)
        #define FVIZ_API __attribute__((visibility("default")))
        #define FVIZ_LOCAL __attribute__((visibility("hidden")))
    #else
        #define FVIZ_API
        #define FVIZ_LOCAL
    #endif
#endif

#if defined(__cplusplus)
    #define FVIZ_EXTERN_C_BEGIN extern "C" {
    #define FVIZ_EXTERN_C_END }
#else
    #define FVIZ_EXTERN_C_BEGIN
    #define FVIZ_EXTERN_C_END
#endif

#if defined(_MSC_VER)
    #define FVIZ_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
    #define FVIZ_INLINE inline __attribute__((always_inline))
#else
    #define FVIZ_INLINE inline
#endif

#define FVIZ_UNUSED(x) ((void)(x))
#define FVIZ_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

#endif /* FVIZ_CORE_API_H */
