include_guard(GLOBAL)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

function(fviz_configure_install target)
    install(TARGETS ${target}
        EXPORT FEAVizTargets
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # Core package headers are installed independently of optional domain modules.
    # In a Core-only build, no FEA-domain header is installed, so the package is a
    # genuinely standalone generic visualization SDK rather than a Core binary next
    # to unusable FEA declarations.
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/FViz"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
        FILES_MATCHING
        PATTERN "*.h"
        PATTERN "FEA" EXCLUDE
    )

    if(FVIZ_BUILD_FEA)
        install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/include/FViz/FEA"
            DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/FViz"
            FILES_MATCHING PATTERN "*.h"
        )
    endif()

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/generated/include/FViz/FVizVersion.h"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/include/FViz/FVizConfig.h"
        DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/FViz"
    )

    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/FEAVizConfigVersion.cmake"
        VERSION "${PROJECT_VERSION}"
        COMPATIBILITY SameMajorVersion
    )

    configure_package_config_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/FEAVizConfig.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/FEAVizConfig.cmake"
        INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FEAViz"
    )

    install(EXPORT FEAVizTargets
        FILE FEAVizTargets.cmake
        NAMESPACE FEAViz::
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FEAViz"
    )

    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/FEAVizConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/FEAVizConfigVersion.cmake"
        DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/FEAViz"
    )
endfunction()
