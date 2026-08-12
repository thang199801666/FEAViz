include_guard(GLOBAL)

function(fviz_detect_platform)
    if(WIN32)
        set(FVIZ_PLATFORM_NAME "Windows" CACHE INTERNAL "FEAViz target platform")
    elseif(APPLE)
        set(FVIZ_PLATFORM_NAME "macOS" CACHE INTERNAL "FEAViz target platform")
    elseif(UNIX)
        set(FVIZ_PLATFORM_NAME "Linux" CACHE INTERNAL "FEAViz target platform")
    else()
        set(FVIZ_PLATFORM_NAME "Unknown" CACHE INTERNAL "FEAViz target platform")
    endif()

    message(STATUS "FEAViz target platform: ${FVIZ_PLATFORM_NAME}")
    message(STATUS "FEAViz generator: ${CMAKE_GENERATOR}")
    message(STATUS "FEAViz C compiler: ${CMAKE_C_COMPILER_ID} ${CMAKE_C_COMPILER_VERSION}")

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(STATUS "FEAViz target architecture: 64-bit")
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 4)
        message(STATUS "FEAViz target architecture: 32-bit")
        if(WIN32)
            message(WARNING
                "FEAViz: this is a 32-bit Windows build. It is supported for the current MVP, "
                "but x64 is strongly recommended for large FEA models and rendering workloads.")
        endif()
    endif()

    if(MSVC)
        message(STATUS "FEAViz MSVC_VERSION: ${MSVC_VERSION}")

        # CMake 3.30 predates Visual Studio 2026 and can report a stale
        # MSVC_TOOLSET_VERSION (for example 143) when NMake is driven by the
        # VS 2026 developer environment.  Validate the compiler itself instead.
        if(MSVC_VERSION GREATER_EQUAL 1950 AND MSVC_VERSION LESS 1960)
            set(_fviz_detected_msvc_family "v145")
        elseif(MSVC_VERSION GREATER_EQUAL 1930 AND MSVC_VERSION LESS 1950)
            set(_fviz_detected_msvc_family "v143")
        else()
            set(_fviz_detected_msvc_family "unknown")
        endif()
        message(STATUS "FEAViz MSVC toolset family (compiler-derived): ${_fviz_detected_msvc_family}")

        if(DEFINED MSVC_TOOLSET_VERSION AND NOT MSVC_TOOLSET_VERSION STREQUAL ""
           AND MSVC_VERSION GREATER_EQUAL 1950 AND NOT MSVC_TOOLSET_VERSION STREQUAL "145")
            message(STATUS
                "FEAViz note: CMake reports MSVC_TOOLSET_VERSION=v${MSVC_TOOLSET_VERSION}; "
                "this value is ignored for VS 2026/NMake because CMake 3.30 predates v145 support.")
        endif()

        if(WIN32 AND FVIZ_REQUIRE_MSVC_V145)
            # VS 2026 / v145 starts at MSVC 19.50 (_MSC_VER/MSVC_VERSION 1950).
            # This check is generator-independent and therefore works with the
            # NMake Makefiles baseline used by FEAViz on Windows.
            if(MSVC_VERSION LESS 1950 OR NOT MSVC_VERSION LESS 1960)
                message(FATAL_ERROR
                    "FEAViz: this Windows configuration requires the Visual Studio 2026 / v145 MSVC compiler. "
                    "Detected MSVC_VERSION=${MSVC_VERSION}. "
                    "Open an x64 Developer Command Prompt for Visual Studio 2026, then configure with the windows-msvc-debug or windows-msvc-release preset. "
                    "Set FVIZ_REQUIRE_MSVC_V145=OFF only for an intentional compatibility build.")
            endif()
        endif()
    elseif(WIN32)
        message(STATUS "FEAViz Windows compiler is not MSVC; v145 validation is not applicable.")
    endif()
endfunction()
