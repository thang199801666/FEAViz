# Build FEAViz on Windows with CMake + MSVC + NMake

This is the default Windows build workflow for FEAViz Phase 0. It intentionally avoids the Visual Studio IDE, Visual Studio CMake generators, Ninja, GCC, MinGW, and project-local batch scripts.

## Toolchain

```text
CMake >= 3.24
  -> NMake Makefiles
  -> nmake.exe
  -> MSVC cl.exe / link.exe
  -> FEAViz
```

The compiler and NMake come from the installed Visual Studio 2026 C++ workload. Visual Studio is used only as the provider of the MSVC toolchain; `devenv.exe` is not involved in the build.

## Requirements

- Windows x64
- CMake 3.24 or newer
- Visual Studio 2026 Professional
- Desktop development with C++ workload
- MSVC Platform Toolset v145
- Windows SDK

You do not need:

- Ninja
- GCC / MinGW
- a `.sln` file
- project-local `.bat` / `.cmd` scripts
- the `Visual Studio 18 2026` CMake generator
- CMake 4.2+

## 1. Open the correct terminal

From the Windows Start menu, open **x64 Developer Command Prompt for Visual Studio 2026**.

Do not use a normal Command Prompt unless it already has the MSVC environment configured. FEAViz does not ship or require a setup script.

Then change to the repository root:

```text
cd /d D:\Code\CPlusPlus\FEAViz
```

Verify the toolchain:

```text
where cmake
where cl
where nmake
cl /Bv
```

`cl.exe` should report the Visual Studio 2026 / MSVC 19.5x compiler family. FEAViz validates this during CMake configure when `FVIZ_REQUIRE_MSVC_V145=ON`.

## 2. Debug build

```text
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

Build directory:

```text
out\build\windows-msvc-debug
```

The preset selects:

```text
generator          = NMake Makefiles
CMAKE_BUILD_TYPE    = Debug
FVIZ_BUILD_SHARED   = ON
warnings-as-errors  = ON
MSVC v145 guard     = ON
```

## 3. Release build

```text
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release
```

Build directory:

```text
out\build\windows-msvc-release
```

Release also enables FEAViz LTO when supported.

## 4. Build without presets

Debug:

```text
cmake -S . -B build-debug -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Debug -DFVIZ_BUILD_SHARED=ON -DFVIZ_BUILD_TESTS=ON -DFVIZ_BUILD_EXAMPLES=ON -DFVIZ_WARNINGS_AS_ERRORS=ON
cmake --build build-debug
ctest --test-dir build-debug --output-on-failure
```

Release:

```text
cmake -S . -B build-release -G "NMake Makefiles" -DCMAKE_BUILD_TYPE=Release -DFVIZ_BUILD_SHARED=ON -DFVIZ_BUILD_TESTS=ON -DFVIZ_BUILD_EXAMPLES=ON -DFVIZ_WARNINGS_AS_ERRORS=ON -DFVIZ_ENABLE_LTO=ON
cmake --build build-release
ctest --test-dir build-release --output-on-failure
```

NMake is a single-configuration generator, so Debug and Release intentionally use separate build directories.

## 5. If `cl` or `nmake` is not found

If:

```text
where cl
where nmake
```

returns nothing, close that terminal and open **x64 Developer Command Prompt for Visual Studio 2026** from the Start menu.

If the tools are still unavailable there, open Visual Studio Installer and confirm that **Desktop development with C++**, MSVC v145, and a Windows SDK are installed.

## 6. Clean rebuild

Delete the matching build directory, then configure again. For example, for Debug:

```text
cmake -E remove_directory out\build\windows-msvc-debug
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug
```

This uses `cmake -E` rather than shell-specific cleanup scripts.

## 7. Why this baseline is used

This workflow keeps FEAViz independent of IDE/version-specific CMake generators while still using Microsoft's native C compiler, linker, NMake, and Windows SDK. It works with CMake 3.30 and requires no build scripts stored in the FEAViz repository.
