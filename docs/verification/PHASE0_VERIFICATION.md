# Phase 0 Verification

Current archive: FEAViz 0.0.5.

## Windows baseline for this archive

The intended Windows path is CMake CLI + NMake + MSVC v145. Visual Studio 2026 supplies the compiler, linker, NMake, Windows headers, and SDK environment; the IDE and Visual Studio CMake generator are not used.

FEAViz ships **no `.bat` or `.cmd` files**. Open **x64 Developer Command Prompt for Visual Studio 2026** and run CMake directly.

Expected workflow:

```text
where cl
where nmake
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

Release:

```text
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release
```

The supplied Windows presets use `NMake Makefiles`, so CMake 3.30 is sufficient. No Ninja, GCC, MinGW, `.sln`, project-local batch script, or `Visual Studio 18 2026` generator is required.

Actual MSVC/NMake execution cannot be performed in the Linux packaging environment, so the final Windows-side compiler invocation remains a host verification step.

## Linux regression environment

The Phase 0 implementation and package behavior are regression-tested in the available Linux environment with CMake, GCC, and Ninja. These checks validate that the Windows build-configuration refresh did not break the portable C17 project.

### Shared Debug + warnings as errors

```sh
cmake -S . -B out/verify/debug -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DFVIZ_BUILD_SHARED=ON \
  -DFVIZ_BUILD_TESTS=ON \
  -DFVIZ_BUILD_EXAMPLES=ON \
  -DFVIZ_WARNINGS_AS_ERRORS=ON
cmake --build out/verify/debug --parallel
ctest --test-dir out/verify/debug --output-on-failure
```

### Static Release + LTO + warnings as errors

```sh
cmake -S . -B out/verify/static -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DFVIZ_BUILD_SHARED=OFF \
  -DFVIZ_BUILD_TESTS=ON \
  -DFVIZ_BUILD_EXAMPLES=ON \
  -DFVIZ_WARNINGS_AS_ERRORS=ON \
  -DFVIZ_ENABLE_LTO=ON
cmake --build out/verify/static --parallel
ctest --test-dir out/verify/static --output-on-failure
```

## Install / package consumer

The verification also installs FEAViz to a clean prefix and builds a separate C17 consumer using:

```cmake
find_package(FEAViz CONFIG REQUIRED)
target_link_libraries(consumer PRIVATE FEAViz::FEAViz)
```

This checks exported headers, package metadata, import targets, compile definitions, linking, and public version reporting.

## Phase 0 acceptance status

- Repository/public/internal architecture: PASS
- C17 configuration: PASS
- Shared/static configuration: PASS
- Warnings-as-errors regression: PASS
- CTest smoke tests: PASS
- Install/export packaging: PASS
- External `find_package` consumer: PASS
- Project-local `.bat` / `.cmd` files: NONE
- Windows CMake preset generator: `NMake Makefiles`
- Windows CMake minimum: 3.24 (CMake 3.30 supported)
- Windows compiler policy: MSVC v145 when `FVIZ_REQUIRE_MSVC_V145=ON`
- Windows host execution: pending confirmation on the user's VS2026 machine
