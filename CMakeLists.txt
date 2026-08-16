cmake_minimum_required(VERSION 3.24)

project(FEAViz
    VERSION 0.41.0
    DESCRIPTION "A modular C17 visualization platform with an optional FEA post-processing module"
    LANGUAGES C
)

find_package(Threads REQUIRED)

set(FVIZ_ABI_VERSION 1 CACHE STRING "FEAViz ABI version")

option(FVIZ_BUILD_SHARED "Build FEAViz as a shared library" ON)
option(FVIZ_BUILD_TESTS "Build FEAViz tests" ON)
option(FVIZ_BUILD_EXAMPLES "Build FEAViz examples" ON)
option(FVIZ_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(FVIZ_ENABLE_ASAN "Enable AddressSanitizer where supported" OFF)
option(FVIZ_ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer where supported" OFF)
option(FVIZ_ENABLE_LTO "Enable interprocedural optimization for release-like builds" OFF)
option(FVIZ_BUILD_FUZZERS "Build Clang libFuzzer targets" OFF)
option(FVIZ_BUILD_BENCHMARKS "Build FEAViz performance benchmarks" OFF)
option(FVIZ_REQUIRE_MSVC_V145 "Require the Visual Studio 2026/v145 MSVC compiler for Windows MSVC builds" ON)
option(FVIZ_BUILD_FEA "Build optional FEAViz FEA post-processing/result module" ON)
option(FVIZ_BUILD_QT_GUI "Build optional QtGui native-child and Qt-owned OpenGL adapters (Windows)" OFF)
option(FVIZ_BUILD_QT_WIDGETS "Build optional Qt Widgets native-child and Qt-owned OpenGL adapters (Windows)" OFF)

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

set(_fviz_core_sources
    src/Core/FVizAllocator.c
    src/Core/FVizArena.c
    src/Core/FVizArray.c
    src/Core/FVizBitArray.c
    src/Core/FVizBuffer.c
    src/Core/FVizCommand.c
    src/Core/FVizError.c
    src/Core/FVizHashMap.c
    src/Core/FVizLog.c
    src/Core/FVizMemory.c
    src/Core/FVizObject.c
    src/Core/FVizString.c
    src/Core/FVizVersion.c
    src/Core/FVizCacheKey.c

    src/Data/FVizDataArray.c
    src/Data/FVizDataType.c
    src/Data/FVizAttributeSet.c
    src/Data/FVizDataObject.c
    src/Data/FVizDataSet.c
    src/Data/FVizProvenance.c
    src/Data/FVizImageData.c
    src/Data/FVizStructuredGrid.c
    src/Data/FVizRectilinearGrid.c
    src/Data/FVizPartitionedDataSet.c
    src/Data/FVizMultiBlockDataSet.c
    src/Data/FVizTemporalDataSet.c
    src/Data/FVizTemporalFrameCache.c

    src/Math/FVizVec2.c
    src/Math/FVizVec3.c
    src/Math/FVizVec4.c
    src/Math/FVizBounds.c
    src/Math/FVizFrustum.c
    src/Math/FVizMat3.c
    src/Math/FVizMat4.c
    src/Math/FVizQuat.c
    src/Math/FVizRay.c
    src/Math/FVizPlane.c
    src/Math/FVizTensor.c
    src/Math/FVizTransform.c

    src/Mesh/FVizPolyData.c
    src/Mesh/FVizPoints.c
    src/Mesh/FVizCellArray.c
    src/Mesh/FVizCellTypeTraits.c
    src/Mesh/FVizCellLinks.c
    src/Mesh/FVizCellAdjacency.c

    src/Data/FVizUnstructuredGrid.c
    src/Data/FVizFieldStatistics.c

    src/Pipeline/FVizAlgorithm.c
    src/Pipeline/FVizExecutive.c
      src/Pipeline/FVizFilter.c
      src/Pipeline/FVizDataProvider.c

    src/Parallel/FVizParallel.c
    src/Parallel/FVizExecutor.c

    src/Interaction/FVizInteractorStyle.c
    src/Interaction/FVizRenderWindowInteractor.c
    src/Interaction/FVizSelection.c
    src/Interaction/FVizSelectionModel.c
    src/Interaction/FVizWidgetRepresentation.c
    src/Interaction/FVizWidgetManipulator.c
    src/Interaction/FVizWidget.c
    src/Interaction/FVizEngineeringWidgets.c
    src/Interaction/FVizVisualizationWidgets.c

    src/Algorithms/FVizContourFilter.c
    src/Algorithms/FVizDeformation.c
    src/Algorithms/FVizPlaneSource.c
    src/Algorithms/FVizCubeSource.c
    src/Algorithms/FVizSphereSource.c
    src/Algorithms/FVizArrowSource.c
    src/Algorithms/FVizArrayCalculator.c
    src/Algorithms/FVizFieldOperations.c
    src/Algorithms/FVizExpression.c
    src/Algorithms/FVizPolyDataFilters.c
    src/Algorithms/FVizGeometryFilters.c
    src/Algorithms/FVizMeshProcessingFilters.c
    src/Algorithms/FVizClipPolyDataFilter.c
    src/Algorithms/FVizProbeFilter.c
    src/Algorithms/FVizResampleWithDataSet.c
    src/Algorithms/FVizImageDataGeometryFilter.c
    src/Algorithms/FVizStructuredGridGeometryFilter.c
    src/Algorithms/FVizRectilinearGridGeometryFilter.c
    src/Algorithms/FVizRectilinearGridExtractFilter.c
    src/Algorithms/FVizCompositeGeometryFilter.c
    src/Algorithms/FVizStructuredGridExtractFilter.c
    src/Algorithms/FVizUnstructuredGridPieceFilter.c
    src/Algorithms/FVizUnstructuredGridPartitionFilter.c
    src/Algorithms/FVizUnstructuredGridGeometryFilter.c
    src/Algorithms/FVizMeshQualityFilter.c
    src/Algorithms/FVizShellExtrusionFilter.c
    src/Algorithms/FVizTubeFilter.c
    src/Algorithms/FVizWarpVectorFilter.c

    src/Spatial/FVizBVH.c
    src/Spatial/FVizPointLocator.c

    src/IO/FVizMeshReader.c
    src/IO/FVizLocalFileProvider.c
    src/IO/FVizOBJReader.c
    src/IO/FVizPLYWriter.c
    src/IO/FVizSTLReader.c
    src/IO/FVizVTKLegacyReader.c
    src/IO/FVizVTUReader.c
    src/IO/FVizPVTUReader.c
    src/IO/FVizPVTUWriter.c
    src/IO/FVizVTUWriter.c
    src/IO/FVizVTPReader.c
    src/IO/FVizVTPWriter.c
    src/IO/FVizPVD.c
    src/IO/FVizPVDReader.c

    src/Rendering/FVizActor.c
    src/Rendering/FVizCamera.c
    src/Rendering/FVizLookupTable.c
    src/Rendering/FVizLight.c
    src/Rendering/FVizGlyphMapper.c
    src/Rendering/FVizVolumeMapper.c
    src/Rendering/FVizFontAtlas.c
    src/Rendering/FVizFont.c
    src/Rendering/FVizTextProperty.c
      src/Rendering/FVizTextActor.c
      src/Rendering/FVizOverlayLayout.c
      src/Rendering/FVizImageComparison.c
      src/Rendering/FVizLabelSet.c
    src/Rendering/FVizMapper.c
    src/Rendering/FVizScalarLegend.c
    src/Rendering/FVizScene.c
    src/Rendering/FVizRenderer.c
    src/Rendering/FVizRenderPass.c
    src/Rendering/FVizRenderGraph.c
    src/Rendering/FVizRenderTarget.c
    src/Rendering/FVizExternalOpenGLSurface.c
    src/Rendering/FVizRendererWidget.c
    src/Rendering/FVizRenderWindow.c
)

if(WIN32)
    list(APPEND _fviz_core_sources
        src/Rendering/GL/FVizGL.c
        src/Rendering/GL/FVizGLDevice.c
        src/Rendering/Win32/FVizRenderWindowWin32.c
        src/Rendering/Win32/FVizWin32RenderControl.c
    )
else()
    list(APPEND _fviz_core_sources src/Rendering/FVizRenderWindowStub.c)
endif()

add_library(FEAVizCore ${_fviz_library_type} ${_fviz_core_sources})
add_library(FEAViz::Core ALIAS FEAVizCore)
set_target_properties(FEAVizCore PROPERTIES
    EXPORT_NAME Core
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
fviz_set_target_output_directories(FEAVizCore)

target_compile_definitions(FEAVizCore
    PRIVATE FVIZ_BUILDING_LIBRARY=1
    PUBLIC $<$<BOOL:${FVIZ_BUILD_SHARED}>:FVIZ_SHARED=1>
)
target_include_directories(FEAVizCore
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/internal"
)
if(WIN32)
    target_link_libraries(FEAVizCore PRIVATE Threads::Threads user32 gdi32 opengl32)
    if(MSVC)
        target_compile_definitions(FEAVizCore PRIVATE _CRT_SECURE_NO_WARNINGS=1)
    endif()
elseif(UNIX)
    target_link_libraries(FEAVizCore PRIVATE Threads::Threads m)
endif()
fviz_apply_compiler_options(FEAVizCore)
fviz_apply_sanitizers(FEAVizCore)
fviz_apply_lto(FEAVizCore)

if(FVIZ_BUILD_FEA)
    set(_fviz_fea_sources
        src/FEA/FVizIntegrationPointData.c
        src/FEA/FVizResultField.c
        src/FEA/FVizResultDatabase.c
        src/FEA/FVizPrimaryVariable.c
        src/FEA/FVizDeformedShape.c
        src/FEA/FVizVisualization.c
        src/FEA/FVizScalarBarActor.c
        src/FEA/FVizDisplayGroup.c
    )
    add_library(FEAVizFEA ${_fviz_library_type} ${_fviz_fea_sources})
    add_library(FEAViz::FEA ALIAS FEAVizFEA)
    set_target_properties(FEAVizFEA PROPERTIES
        EXPORT_NAME FEA
        C_STANDARD 17
        C_STANDARD_REQUIRED YES
        C_EXTENSIONS NO
        VERSION "${PROJECT_VERSION}"
        SOVERSION "${FVIZ_ABI_VERSION}"
        OUTPUT_NAME "FEAVizFEA"
        POSITION_INDEPENDENT_CODE ON
        C_VISIBILITY_PRESET hidden
        VISIBILITY_INLINES_HIDDEN YES
    )
    fviz_set_target_output_directories(FEAVizFEA)
    target_compile_definitions(FEAVizFEA PRIVATE FVIZ_FEA_BUILDING_LIBRARY=1)
    target_include_directories(FEAVizFEA
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/generated/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
        PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/internal"
    )
    target_link_libraries(FEAVizFEA PUBLIC FEAViz::Core)
    if(UNIX AND NOT WIN32)
        target_link_libraries(FEAVizFEA PRIVATE m)
    endif()
    fviz_apply_compiler_options(FEAVizFEA)
    fviz_apply_sanitizers(FEAVizFEA)
    fviz_apply_lto(FEAVizFEA)
    # Backward-compatible aggregate: old FEAViz::FEAViz consumers still receive
    # the FEA module when it is built. New generic consumers should link Core.
    add_library(FEAViz::FEAViz ALIAS FEAVizFEA)
else()
    add_library(FEAViz::FEAViz ALIAS FEAVizCore)
endif()

message(STATUS "FEAViz Core runtime output: ${CMAKE_BINARY_DIR}/bin")
if(FVIZ_BUILD_FEA)
    message(STATUS "FEAViz FEA runtime output: ${CMAKE_BINARY_DIR}/bin")
endif()
message(STATUS "FEAViz archive output: ${CMAKE_BINARY_DIR}/lib")

# Optional GUI-toolkit adapters. The renderer core stays C17-only and toolkit
# independent.  Each Qt target exposes both the legacy/native-child HWND path and
# a toolkit-owned OpenGL context/FBO path backed by FVizExternalOpenGLSurface.
if(FVIZ_BUILD_QT_GUI OR FVIZ_BUILD_QT_WIDGETS)
    if(NOT WIN32)
        message(FATAL_ERROR "FEAViz Qt adapters currently require the Win32/OpenGL render backend")
    endif()
    enable_language(CXX)
    find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Gui)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Gui)
    if(QT_VERSION_MAJOR EQUAL 6 AND FVIZ_BUILD_QT_GUI)
        find_package(Qt6 REQUIRED COMPONENTS OpenGL)
    endif()
endif()

if(FVIZ_BUILD_QT_GUI)
    add_library(FEAVizQtGui STATIC
        integrations/Qt/FVizQtWindow.cpp
        integrations/Qt/FVizQtWindow.h
        integrations/Qt/FVizQtOpenGLWindow.cpp
        integrations/Qt/FVizQtOpenGLWindow.h
    )
    add_library(FEAViz::QtGui ALIAS FEAVizQtGui)
    set_target_properties(FEAVizQtGui PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
    )
    target_include_directories(FEAVizQtGui
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/integrations/Qt>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    target_link_libraries(FEAVizQtGui
        PUBLIC
            FEAViz::Core
            Qt${QT_VERSION_MAJOR}::Gui
    )
    if(QT_VERSION_MAJOR EQUAL 6)
        target_link_libraries(FEAVizQtGui PUBLIC Qt6::OpenGL)
    endif()
    fviz_set_target_output_directories(FEAVizQtGui)
endif()

if(FVIZ_BUILD_QT_WIDGETS)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets)
    if(QT_VERSION_MAJOR EQUAL 6)
        find_package(Qt6 REQUIRED COMPONENTS OpenGLWidgets)
    endif()
    add_library(FEAVizQtWidgets STATIC
        integrations/Qt/FVizQtWidget.cpp
        integrations/Qt/FVizQtWidget.h
        integrations/Qt/FVizQtOpenGLWidget.cpp
        integrations/Qt/FVizQtOpenGLWidget.h
    )
    add_library(FEAViz::QtWidgets ALIAS FEAVizQtWidgets)
    set_target_properties(FEAVizQtWidgets PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED YES
        CXX_EXTENSIONS NO
        POSITION_INDEPENDENT_CODE ON
    )
    target_include_directories(FEAVizQtWidgets
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/integrations/Qt>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )
    target_link_libraries(FEAVizQtWidgets
        PUBLIC
            FEAViz::Core
            Qt${QT_VERSION_MAJOR}::Widgets
    )
    if(QT_VERSION_MAJOR EQUAL 6)
        target_link_libraries(FEAVizQtWidgets PUBLIC Qt6::OpenGLWidgets)
    endif()
    fviz_set_target_output_directories(FEAVizQtWidgets)
endif()

if(FVIZ_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(FVIZ_BUILD_EXAMPLES)
    # The C++ feature-test example consumes the header-only binding, which
    # requires a C++17 compiler. The tests subdirectory also enables CXX, but
    # examples may be built standalone (tests disabled).
    if(TARGET FEAVizFEA)
        enable_language(CXX)
    endif()
    add_subdirectory(examples)
endif()

if(FVIZ_BUILD_FUZZERS)
    add_subdirectory(fuzz)
endif()

if(FVIZ_BUILD_BENCHMARKS)
    add_subdirectory(benchmarks)
endif()

if(FVIZ_BUILD_QT_GUI)
    install(TARGETS FEAVizQtGui
        EXPORT FEAVizTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(FILES
        integrations/Qt/FVizQtWindow.h
        integrations/Qt/FVizQtOpenGLWindow.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/FViz/Qt
    )
endif()

if(FVIZ_BUILD_QT_WIDGETS)
    install(TARGETS FEAVizQtWidgets
        EXPORT FEAVizTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
    install(FILES
        integrations/Qt/FVizQtWidget.h
        integrations/Qt/FVizQtOpenGLWidget.h
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/FViz/Qt
    )
endif()

if(FVIZ_BUILD_QT_GUI AND FVIZ_BUILD_QT_WIDGETS)
    if(QT_VERSION_MAJOR EQUAL 6)
        set(FVIZ_QT_PACKAGE_DEPENDENCY
            "find_dependency(Qt6 REQUIRED COMPONENTS Gui Widgets OpenGL OpenGLWidgets)")
    else()
        set(FVIZ_QT_PACKAGE_DEPENDENCY
            "find_dependency(Qt5 REQUIRED COMPONENTS Gui Widgets)")
    endif()
elseif(FVIZ_BUILD_QT_WIDGETS)
    if(QT_VERSION_MAJOR EQUAL 6)
        set(FVIZ_QT_PACKAGE_DEPENDENCY
            "find_dependency(Qt6 REQUIRED COMPONENTS Widgets OpenGLWidgets)")
    else()
        set(FVIZ_QT_PACKAGE_DEPENDENCY
            "find_dependency(Qt5 REQUIRED COMPONENTS Widgets)")
    endif()
elseif(FVIZ_BUILD_QT_GUI)
    if(QT_VERSION_MAJOR EQUAL 6)
        set(FVIZ_QT_PACKAGE_DEPENDENCY
            "find_dependency(Qt6 REQUIRED COMPONENTS Gui OpenGL)")
    else()
        set(FVIZ_QT_PACKAGE_DEPENDENCY
            "find_dependency(Qt5 REQUIRED COMPONENTS Gui)")
    endif()
else()
    set(FVIZ_QT_PACKAGE_DEPENDENCY "")
endif()

if(FVIZ_BUILD_FEA)
    install(TARGETS FEAVizFEA
        EXPORT FEAVizTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )
endif()

fviz_configure_install(FEAVizCore)
