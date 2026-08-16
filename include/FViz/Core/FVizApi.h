#ifndef FVIZ_CORE_API_H
#define FVIZ_CORE_API_H

/*
 * FEAViz symbol export/import macros.
 *
 * FEAViz builds as several shared modules (like VTK's vtkCommonCore /
 * vtkRenderingOpenGL2 / ...). Each public header belongs to one module and
 * annotates its functions with that module's export macro:
 *
 *   FVIZ_CORE_API        - FEAVizCore (runtime, math, system)
 *   FVIZ_DATA_API        - FEAVizCommonData (mesh, data sets, spatial)
 *   FVIZ_FILTERS_API     - FEAVizFilters (algorithms, pipeline)
 *   FVIZ_IO_API          - FEAVizIO
 *   FVIZ_RENDERING_API   - FEAVizRendering (core rendering + GL backend)
 *   FVIZ_INTERACTION_API - FEAVizInteraction
 *   FVIZ_PARALLEL_API    - FEAVizParallel
 *   FVIZ_FEA_API         - FEAVizFEA (optional module, see FVizFEAApi.h)
 *
 * A module target compiles with its FVIZ_<MODULE>_BUILDING_LIBRARY macro so
 * the owning module exports its symbols and all other modules import them.
 *
 * The legacy monolithic shared build defines FVIZ_BUILDING_LIBRARY on the
 * single FEAViz library; FVIZ_MONOLITHIC_BUILD is set below in that case so
 * every module macro exports and the whole codebase can live in one DLL.
 */

#if defined(_WIN32) || defined(__CYGWIN__)

#define FVIZ_DECL_EXPORT __declspec(dllexport)
#define FVIZ_DECL_IMPORT __declspec(dllimport)
#define FVIZ_DECL_LOCAL

#else

#if defined(__GNUC__) || defined(__clang__)
#define FVIZ_DECL_EXPORT __attribute__((visibility("default")))
#define FVIZ_DECL_IMPORT __attribute__((visibility("default")))
#define FVIZ_DECL_LOCAL __attribute__((visibility("hidden")))
#else
#define FVIZ_DECL_EXPORT
#define FVIZ_DECL_IMPORT
#define FVIZ_DECL_LOCAL
#endif

#endif

/* In the monolithic shared build every module macro exports. */
#if defined(FVIZ_SHARED) && defined(FVIZ_BUILDING_LIBRARY)
#define FVIZ_MONOLITHIC_BUILD 1
#endif

/* Core module. */
#if defined(FVIZ_SHARED)
#if defined(FVIZ_MONOLITHIC_BUILD) || defined(FVIZ_CORE_BUILDING_LIBRARY)
#define FVIZ_CORE_API FVIZ_DECL_EXPORT
#else
#define FVIZ_CORE_API FVIZ_DECL_IMPORT
#endif
#else
#define FVIZ_CORE_API FVIZ_DECL_EXPORT
#endif

/* Common data module (mesh, data sets, spatial structures). */
#if defined(FVIZ_SHARED)
#if defined(FVIZ_MONOLITHIC_BUILD) || defined(FVIZ_DATA_BUILDING_LIBRARY)
#define FVIZ_DATA_API FVIZ_DECL_EXPORT
#else
#define FVIZ_DATA_API FVIZ_DECL_IMPORT
#endif
#else
#define FVIZ_DATA_API FVIZ_DECL_EXPORT
#endif

/* Filters module (algorithms + pipeline executive). */
#if defined(FVIZ_SHARED)
#if defined(FVIZ_MONOLITHIC_BUILD) || defined(FVIZ_FILTERS_BUILDING_LIBRARY)
#define FVIZ_FILTERS_API FVIZ_DECL_EXPORT
#else
#define FVIZ_FILTERS_API FVIZ_DECL_IMPORT
#endif
#else
#define FVIZ_FILTERS_API FVIZ_DECL_EXPORT
#endif

/* IO module. */
#if defined(FVIZ_SHARED)
#if defined(FVIZ_MONOLITHIC_BUILD) || defined(FVIZ_IO_BUILDING_LIBRARY)
#define FVIZ_IO_API FVIZ_DECL_EXPORT
#else
#define FVIZ_IO_API FVIZ_DECL_IMPORT
#endif
#else
#define FVIZ_IO_API FVIZ_DECL_EXPORT
#endif

/* Rendering module (core rendering + OpenGL backend). */
#if defined(FVIZ_SHARED)
#if defined(FVIZ_MONOLITHIC_BUILD) || defined(FVIZ_RENDERING_BUILDING_LIBRARY)
#define FVIZ_RENDERING_API FVIZ_DECL_EXPORT
#else
#define FVIZ_RENDERING_API FVIZ_DECL_IMPORT
#endif
#else
#define FVIZ_RENDERING_API FVIZ_DECL_EXPORT
#endif

/* Interaction module. */
#if defined(FVIZ_SHARED)
#if defined(FVIZ_MONOLITHIC_BUILD) || defined(FVIZ_INTERACTION_BUILDING_LIBRARY)
#define FVIZ_INTERACTION_API FVIZ_DECL_EXPORT
#else
#define FVIZ_INTERACTION_API FVIZ_DECL_IMPORT
#endif
#else
#define FVIZ_INTERACTION_API FVIZ_DECL_EXPORT
#endif

/* Parallel module. */
#if defined(FVIZ_SHARED)
#if defined(FVIZ_MONOLITHIC_BUILD) || defined(FVIZ_PARALLEL_BUILDING_LIBRARY)
#define FVIZ_PARALLEL_API FVIZ_DECL_EXPORT
#else
#define FVIZ_PARALLEL_API FVIZ_DECL_IMPORT
#endif
#else
#define FVIZ_PARALLEL_API FVIZ_DECL_EXPORT
#endif

/* Legacy single-module alias (now maps to the core module). */
#define FVIZ_API FVIZ_CORE_API
#define FVIZ_LOCAL FVIZ_DECL_LOCAL

#if defined(__cplusplus)
#define FVIZ_EXTERN_C_BEGIN                                                                                            \
    extern "C"                                                                                                         \
    {
#define FVIZ_EXTERN_C_END }
#else
#define FVIZ_EXTERN_C_BEGIN
#define FVIZ_EXTERN_C_END
#endif

#if defined(_MSC_VER)
#define FVIZ_INLINE __forceinline
#elif defined(__GNUC__) || defined(__clang__)
#define FVIZ_INLINE inline __attribute__((always_inline))
#else
#define FVIZ_INLINE inline
#endif

#define FVIZ_UNUSED(x) ((void)(x))
#define FVIZ_ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

#endif /* FVIZ_CORE_API_H */
