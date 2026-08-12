cmake_minimum_required(VERSION 3.24)

project(FEAViz
    VERSION 0.4.2
    DESCRIPTION "A C17 visualization and FEA data-processing library"
    LANGUAGES C
)

set(FVIZ_ABI_VERSION 1 CACHE STRING "FEAViz ABI version")

option(FVIZ_BUILD_SHARED "Build FEAViz as a shared library" ON)
option(FVIZ_BUILD_TESTS "Build FEAViz tests" ON)
option(FVIZ_BUILD_EXAMPLES "Build FEAViz examples" ON)
option(FVIZ_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(FVIZ_ENABLE_ASAN "Enable AddressSanitizer where supported" OFF)
option(FVIZ_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer where supported" OFF)
option(FVIZ_ENABLE_LTO "Enable interprocedural optimization for release-like builds" OFF)
option(FVIZ_REQUIRE_MSVC_V145 "Require the Visual Studio 2026/v145 MSVC compiler for Windows MSVC builds" ON)

# Keep runtime artifacts together. This is especially important on Windows:
# FEAViz.dll and FEAVizViewer.exe land in the same directory, so no manual
# DLL copy or PATH edit is required for build-tree examples/tests.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Apply output directories directly to targets as well as globally. This avoids
# stale/subdirectory-specific output behavior and keeps DLLs/executables together.
function(fviz_set_target_output_directories target_name)
    set_target_properties(${target_name} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
        ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib"
    )

    foreach(_cfg DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
        set_target_properties(${target_name} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_BINARY_DIR}/bin"
            LIBRARY_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_BINARY_DIR}/bin"
            ARCHIVE_OUTPUT_DIRECTORY_${_cfg} "${CMAKE_BINARY_DIR}/lib"
        )
    endforeach()
endfunction()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(FVizCompilerOptions)
include(FVizPlatform)
include(FVizSanitizers)
include(FVizVersion)
include(FVizInstall)

fviz_detect_platform()
fviz_configure_generated_headers()

if(FVIZ_BUILD_SHARED)
    set(_fviz_library_type SHARED)
else()
    set(_fviz_library_type STATIC)
endif()

set(_fviz_sources
    src/Core/FVizAllocator.c
    src/Core/FVizArray.c
    src/Core/FVizBitArray.c
    src/Core/FVizBuffer.c
    src/Core/FVizError.c
    src/Core/FVizHashMap.c
    src/Core/FVizLog.c
    src/Core/FVizMemory.c
    src/Core/FVizObject.c
    src/Core/FVizString.c
    src/Core/FVizVersion.c

    src/Data/FVizDataArray.c
    src/Data/FVizDataType.c
    src/Data/FVizAttributeSet.c
    src/Data/FVizDataSet.c

    src/Math/FVizVec2.c
    src/Math/FVizVec3.c
    src/Math/FVizVec4.c
    src/Math/FVizBounds.c
    src/Math/FVizMat3.c
    src/Math/FVizMat4.c
    src/Math/FVizQuat.c
    src/Math/FVizRay.c
    src/Math/FVizPlane.c

    src/Mesh/FVizPolyData.c
    src/Mesh/FVizPoints.c
    src/Mesh/FVizCellArray.c

    src/FEA/FVizUnstructuredGrid.c

    src/Pipeline/FVizFilter.c

    src/Spatial/FVizBVH.c
    src/Spatial/FVizPointLocator.c

    src/IO/FVizMeshReader.c
    src/IO/FVizOBJReader.c
    src/IO/FVizSTLReader.c
    src/IO/FVizVTUReader.c

    src/Rendering/FVizActor.c
    src/Rendering/FVizCamera.c
    src/Rendering/FVizLookupTable.c
    src/Rendering/FVizMapper.c
    src/Rendering/FVizScalarLegend.c
    src/Rendering/FVizScene.c
    src/Rendering/FVizRenderer.c
    src/Rendering/FVizRenderWindow.c
)

if(WIN32)
    list(APPEND _fviz_sources
        src/Rendering/GL/FVizGL.c
        src/Rendering/GL/FVizGLDevice.c
        src/Rendering/Win32/FVizRenderWindowWin32.c
    )
else()
    list(APPEND _fviz_sources src/Rendering/FVizRenderWindowStub.c)
endif()

add_library(FEAViz ${_fviz_library_type} ${_fviz_sources})
add_library(FEAViz::FEAViz ALIAS FEAViz)

set_target_properties(FEAViz PROPERTIES
    C_STANDARD 17
    C_STANDARD_REQUIRED YES
    C_EXTENSIONS NO
    VERSION "${PROJECT_VERSION}"
    SOVERSION "${FVIZ_ABI_VERSION}"
    OUTPUT_NAME "FEAViz"
    POSITION_INDEPENDENT_CODE ON
    C_VISIBILITY_PRESET hidden
    VISIBILITY_INLINES_HIDDEN YES
)

fviz_set_target_output_directories(FEAViz)

message(STATUS "FEAViz runtime output: ${CMAKE_BINARY_DIR}/bin")
message(STATUS "FEAViz archive output: ${CMAKE_BINARY_DIR}/lib")

target_compile_definitions(FEAViz
    PRIVATE
        FVIZ_BUILDING_LIBRARY=1
    PUBLIC
        $<$<BOOL:${FVIZ_BUILD_SHARED}>:FVIZ_SHARED=1>
)

target_include_directories(FEAViz
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/internal"
)

if(WIN32)
    target_link_libraries(FEAViz PRIVATE user32 gdi32 opengl32)
    if(MSVC)
        target_compile_definitions(FEAViz PRIVATE _CRT_SECURE_NO_WARNINGS=1)
    endif()
elseif(UNIX)
    target_link_libraries(FEAViz PRIVATE m)
endif()

fviz_apply_compiler_options(FEAViz)
fviz_apply_sanitizers(FEAViz)
fviz_apply_lto(FEAViz)

if(FVIZ_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(FVIZ_BUILD_EXAMPLES)
    add_subdirectory(examples)
endif()

fviz_configure_install(FEAViz)
