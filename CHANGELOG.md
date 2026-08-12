# Changelog

## 0.2.0 - Complete core containers and math primitives

- Added `FVizBitArray`: compact 64-bit-word backed bit storage with set/test, resize, clear, set-all, and hardware pop-count.
- Added `FVizHashMap`: open-addressing hash map with linear probing and tombstone erase, keyed by `FVizId` with `void*` values, automatic growth at 70% load, and iteration support.
- Expanded `FViz.Core.Containers` test coverage for both containers including growth and erase patterns.

## 0.2.0 - Complete math primitives

- Added `FVizVec2` and `FVizVec4` value types with add/sub/scale/dot/length/normalize operations.
- Added `FVizMat3` (column-major): identity, multiply, transpose, adjugate-based inverse, `transform_vec3`, and `from_quaternion`.
- Added `FVizQuat`: identity, `from_axis_angle`, Hamilton multiply, normalize, `rotate_vec3`, dot.
- Added `FVizRay` and `FVizPlane` for picking/intersection foundations: ray point/distance-to-point, ray-sphere intersection, plane from point+normal, signed point distance, and point projection.
- Extended the `FVizMath.h` umbrella header to include the new primitives.
- Expanded `FViz.Math.Core` test coverage for every new primitive.
- Fixed `FVizMat3` inverse index mapping (column-major result layout).

## 0.1.4 - Modern OpenGL renderer

- Added an internal OpenGL function loader (`FVizGL`) for the OpenGL 3.3 core subset, resolving entry points through `wglGetProcAddress` with an `opengl32.dll` fallback and no third-party dependency.
- Added `FVizGLDevice`, a shader-based render device with a built-in GLSL 330 program and per-actor GPU resource cache (VAO + position/normal VBO + index EBO).
- GPU geometry is uploaded once per mesh and reused across frames; the cache rebuilds only when the `FVizPolyData` generation counter changes.
- Reworked the Win32/WGL context creation to request an OpenGL 3.3 core-profile context via `wglChoosePixelFormatARB`/`wglCreateContextAttribsARB` (probe-context based), with a seamless fallback to the legacy 1.1 compatibility path.
- Replaced fixed-function lighting with per-pixel shader lighting (Lambert diffuse + ambient) and automatic flat-shading fallback for meshes without computed normals.
- Kept the public scene/actor/render-window API unchanged; all renderer changes are internal.
- Added a mesh mutation generation counter to `FVizPolyData` used for GPU-cache invalidation.
- Verified: OpenGL 3.3 core-profile path active on the Windows build, all 19 CTest tests pass.

## 0.1.3

- Fixed MSVC 19.50 / C17 build failure caused by direct use of `max_align_t`.
- Added an internal portable maximum-fundamental-alignment abstraction used by the allocator and object runtime.
- Fixed misleading VS 2026 toolset reporting under CMake 3.30/NMake by deriving v145 from `MSVC_VERSION=1950..1959`.
- Added explicit 32-bit/64-bit architecture reporting at configure time and a warning for 32-bit Windows builds.
- Kept the CMake/NMake workflow IDE-independent and added no `.bat`/`.cmd` files.

## 0.1.2

- Bind runtime/library/archive output directories directly to FEAViz and every example target.
- Keep FEAViz.dll and example/viewer executables together under the build `bin/` directory.
- Add configure-time output path diagnostics to make stale build trees easier to identify.

## 0.1.1 - Simple model example

- Added `FEAVizSimpleModel`, a minimal interactive cube example built entirely through the public C API.
- Added `examples/06_Tutorial/simple_model.c` and tutorial build/run notes.
- No public ABI changes.

## 0.1.0 - First interactive 3D scene

- Added `FVizBuffer`, zero-copy external-memory wrapping, `FVizArray`, and `FVizString`.
- Added typed/component-aware `FVizDataArray`.
- Added Vec3, Mat4, bounds, view/projection math and camera navigation primitives.
- Added triangle `FVizPolyData`, bounds, validation and smooth normal generation.
- Added OBJ and ASCII/binary STL mesh readers plus extension dispatch.
- Added `FVizActor`, multi-actor `FVizScene`, `FVizRenderer`, and `FVizCamera`.
- Added the public `FVizRenderWindow` abstraction.
- Added a native Windows Win32/WGL OpenGL backend with depth testing, lighting, indexed triangles, wireframe mode, orbit, pan, zoom and fit-view interaction.
- Added `FEAVizViewer`, which loads OBJ/STL from the command line or displays a built-in cube.
- Standardized build-tree runtime output so `FEAViz.dll` and `FEAVizViewer.exe` are colocated under `bin/`.
- Expanded the suite to 14 CTest tests and public-header isolation checks.
- Retained a CMake-first, no-project-`.bat`/`.cmd` build workflow.

## 0.0.6 - Phase 1 Core Runtime

- Added the public `FVizAllocator` callback interface and portable aligned default allocator.
- Added default allocation/reallocation/free APIs and checked size arithmetic.
- Added opaque `FVizObject`, stable 64-bit type IDs, internal class-parent metadata, and generic retain/release operations.
- Added atomic reference counting with overflow/underflow protection.
- Added extended `FVizResult` diagnostics, result strings, and fixed-capacity thread-local last-error storage.
- Added log levels, filtering, callback sinks, and a default stderr logger.
- Added custom-allocator leak tracking, aligned-memory stress tests, threaded retain/release tests, and TLS isolation tests.
- Added a Core Runtime example and design/verification documentation.
- Kept the CMake-first, no-project-batch-script Windows build baseline unchanged.

## 0.0.5 - Phase 0 script-free Windows build

- Removed all project-local `.bat` and `.cmd` files.
- Windows baseline remains CMake CLI + `NMake Makefiles` + MSVC v145.
- MSVC environment setup is now obtained by opening the x64 Developer Command Prompt for Visual Studio 2026; FEAViz no longer wraps Visual Studio environment scripts.
- Updated build and troubleshooting documentation to use only direct CMake/CTest commands.
- Clean rebuild examples now use `cmake -E remove_directory` rather than shell-specific scripts.

## 0.0.4 - Phase 0 CMake/MSVC/NMake Windows baseline

- Replaced the Windows Ninja/Visual Studio-generator presets with `NMake Makefiles` presets.
- Windows builds now use CMake CLI plus the MSVC v145 `cl.exe`, `link.exe`, and `nmake.exe` supplied by Visual Studio 2026.
- Removed any Windows requirement for Ninja, GCC, MinGW, `.sln` generation, CMake 4.2+, or the `Visual Studio 18 2026` generator.
- Added separate `windows-msvc-debug` and `windows-msvc-release` single-config build directories.
- Added `setup_msvc_env.bat`, environment validation, and Debug/Release convenience scripts.
- Updated the v145 compiler guard to be generator-independent and NMake-friendly.
- Retained portable Ninja presets only for non-Windows development and CI verification.

## 0.0.3 - Phase 0 VS2026 compatibility fix

- Changed the default `vs2026-x64` preset to `Ninja Multi-Config` so VS2026/v145 builds do not require the CMake 4.2 Visual Studio generator.
- Added compiler-family validation using `MSVC_VERSION` so v145 is enforced with both Ninja and Visual Studio generators.
- Added optional `vs2026-msbuild-x64` presets for CMake 4.2+ users who explicitly want native Visual Studio/MSBuild generation.
- Added Windows environment check/configure scripts.
- Expanded troubleshooting for machines where `cmake --help` does not list `Visual Studio 18 2026`.

## 0.0.2 - Phase 0 VS2026 toolchain refresh

- Replaced the Visual Studio 2022 preset with Visual Studio 2026 x64.
- Pinned the Windows platform toolset to `v145,host=x64`.
- Added a VS2026/v145 validation guard and generator/toolset diagnostics.
- Added dedicated Visual Studio 2026 build documentation.
- Documented the CMake 4.2+ requirement for the Visual Studio 18 2026 generator.
- Kept the portable C17/Ninja build path intact.

## 0.0.1 - Phase 0

- Established long-term FEAViz repository layout.
- Added C17 CMake build and presets.
- Added shared/static build support and symbol visibility macros.
- Added generated version/configuration headers.
- Added public platform/compiler detection macros.
- Added install/export package configuration.
- Added compiler warning policy, optional sanitizers, and optional LTO.
- Added CTest-based smoke tests and a first example.
- Reserved module boundaries for Data, Mesh, Geometry, Spatial, Pipeline, Algorithms, Rendering, Interaction, IO, FEA, Parallel, and Plugins.
