include_guard(GLOBAL)
include(CheckIPOSupported)

function(fviz_apply_compiler_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /utf-8
            /Zc:preprocessor
        )
        if(FVIZ_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wstrict-prototypes
            -Wmissing-prototypes
        )
        if(FVIZ_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

function(fviz_apply_lto target)
    if(NOT FVIZ_ENABLE_LTO)
        return()
    endif()

    check_ipo_supported(RESULT _fviz_ipo_supported OUTPUT _fviz_ipo_error)
    if(_fviz_ipo_supported)
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
        set_property(TARGET ${target} PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
    else()
        message(WARNING "FEAViz: LTO requested but unavailable: ${_fviz_ipo_error}")
    endif()
endfunction()
