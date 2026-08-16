#ifndef FVIZ_SYSTEM_PLATFORM_H
#define FVIZ_SYSTEM_PLATFORM_H

#if defined(_WIN32)
#define FVIZ_PLATFORM_WINDOWS 1
#else
#define FVIZ_PLATFORM_WINDOWS 0
#endif

#if defined(__linux__)
#define FVIZ_PLATFORM_LINUX 1
#else
#define FVIZ_PLATFORM_LINUX 0
#endif

#if defined(__APPLE__)
#define FVIZ_PLATFORM_MACOS 1
#else
#define FVIZ_PLATFORM_MACOS 0
#endif

#if defined(_MSC_VER)
#define FVIZ_COMPILER_MSVC 1
#else
#define FVIZ_COMPILER_MSVC 0
#endif

#if defined(__clang__)
#define FVIZ_COMPILER_CLANG 1
#else
#define FVIZ_COMPILER_CLANG 0
#endif

#if defined(__GNUC__) && !defined(__clang__)
#define FVIZ_COMPILER_GCC 1
#else
#define FVIZ_COMPILER_GCC 0
#endif

#if defined(_M_X64) || defined(__x86_64__)
#define FVIZ_ARCH_X64 1
#else
#define FVIZ_ARCH_X64 0
#endif

#if defined(_M_ARM64) || defined(__aarch64__)
#define FVIZ_ARCH_ARM64 1
#else
#define FVIZ_ARCH_ARM64 0
#endif

#endif /* FVIZ_SYSTEM_PLATFORM_H */
