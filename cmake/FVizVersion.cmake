include_guard(GLOBAL)

function(fviz_configure_generated_headers)
    set(_fviz_generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated/include/FViz")
    file(MAKE_DIRECTORY "${_fviz_generated_dir}")

    if(FVIZ_BUILD_SHARED)
        set(FVIZ_CONFIG_SHARED 1)
    else()
        set(FVIZ_CONFIG_SHARED 0)
    endif()

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/FVizVersion.h.in"
        "${_fviz_generated_dir}/FVizVersion.h"
        @ONLY
    )

    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/templates/FVizConfig.h.in"
        "${_fviz_generated_dir}/FVizConfig.h"
        @ONLY
    )
endfunction()
