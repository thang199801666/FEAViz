if(NOT CMAKE_C_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "FVIZ_BUILD_FUZZERS requires Clang with libFuzzer")
endif()

if(NOT UNIX)
    message(FATAL_ERROR "The current FEAViz reader fuzz harness requires a POSIX temporary file")
endif()

add_executable(FVizFuzzReaders fuzz_readers.c)
target_link_libraries(FVizFuzzReaders PRIVATE FEAViz::FEAViz)
fviz_apply_compiler_options(FVizFuzzReaders)
target_compile_options(FVizFuzzReaders PRIVATE -fsanitize=fuzzer,address,undefined)
target_link_options(FVizFuzzReaders PRIVATE -fsanitize=fuzzer,address,undefined)
