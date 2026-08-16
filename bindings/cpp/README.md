# FEAViz C++ binding

A header-only **C++17** layer over the FEAViz C ABI. The C ABI remains the
source of truth; the binding adds RAII ownership, typed math value types, and
ergonomic methods so application code reads like C++ while staying ABI
compatible with the core library.

## Layout

```text
include/FVizCpp/
  FVizCpp.hpp          umbrella header (include this one)
  FVizCppMath.hpp      Vec2/Vec3/Vec4, Mat4, Quat, Bounds, Plane, Ray
  FVizCppObject.hpp    fviz::Error, RAII Object<T> (refcounted)
  FVizCppData.hpp      DataArray, AttributeSet, Points, CellArray,
                       UnstructuredGrid, PolyData, ImageData, StructuredGrid,
                       RectilinearGrid, Transform
  FVizCppRendering.hpp Camera, LookupTable, Mapper, Actor, Scene, Renderer,
                       ScalarLegend, RendererWidget, RenderWindow, Light,
                       TextActor2D, BillboardTextActor3D, LabelSet3D
  FVizCppInteraction.hpp InteractionEvent, InteractorStyle, RenderWindowInteractor
  FVizCppFilter.hpp    Filter + Threshold / Warp / CellDataToPoint / Surface /
                       Slice / Transform filters
  FVizCppParallel.hpp  CancellationToken, Future, Executor, parallel::forEach
  FVizCppIO.hpp        readVtu / readVtkLegacy / readObj / readStl / readVtp,
                       writeVtu / writeVtp / writePly, PVDCollection
  FVizCppFEA.hpp       FEA module: HistorySeries/Region, Frame, Field, Step,
                       ResultDatabase, PrimaryVariable, DeformedShape,
                       ScalarBarActor, fea contour/edges/result/cut helpers
```

The FEA wrappers (`FVizCppFEA.hpp`) link against `FEAViz::FEA`; everything else
links against `FEAViz::Core`.

## Usage

Add the include directory and link the C core (shared or static):

```cmake
find_package(FEAViz 0.41 REQUIRED COMPONENTS Core FEA)   # FEA only if needed
target_include_directories(my_app PRIVATE path/to/bindings/cpp/include)
target_link_libraries(my_app PRIVATE FEAViz::FEA)        # or FEAViz::Core
```

Then:

```cpp
#include <FVizCpp/FVizCpp.hpp>

using namespace fviz;

// Build a grid.
UnstructuredGrid grid = UnstructuredGrid::create();
grid.addPoint(Vec3(0.0f, 0.0f, 0.0f));
// ... add points and cells ...

// Extract a surface and color it by a scalar field.
FVizPolyData* raw = nullptr;
fviz_unstructured_grid_extract_geometry(grid.get(), &raw);
PolyData surface(raw);
surface.computeNormals();

LookupTable lut = LookupTable::create(256u);
lut.setRange(0.0f, 100.0f);
lut.buildPreset(FVIZ_COLOR_MAP_RAINBOW);

Mapper mapper = Mapper::create();
mapper.setPolyData(surface);
mapper.setLookupTable(lut);
mapper.setArraySelection(FVIZ_ASSOCIATION_POINTS, "stress");
mapper.setScalarVisibility(true);

Actor actor = Actor::create();
actor.setMapper(mapper);

Scene scene = Scene::create();
scene.addActor(actor);

Renderer renderer = Renderer::create();
renderer.setScene(scene);
renderer.fitCamera(1.2f);

// Or read a result file directly.
UnstructuredGrid loaded = readVtu("part.vtu");
```

## Ownership model

Every wrapper is an `fviz::Object<T>` that owns one reference through
`fviz_retain` / `fviz_release`:

- construction from a fresh `fviz_*_create` result **adopts** the reference;
- copy retains, move transfers, destruction releases;
- methods that return borrowed C pointers wrap them with a retained view
  (`retain()`), so temporary wrappers never double-release.

Functions throw `fviz::Error` on failure; `Error::code()` returns the C
`FVizResult`.

## Testing

The binding is exercised by three CTest targets:

- `FViz.Cpp.Binding` (`FVizTestCppBinding`) — math operators, RAII refcounting,
  grid construction, data arrays, readers, and headless rendering assembly.
- `FViz.Cpp.FEABinding` (`FVizTestCppFEABinding`, only when `FEAViz::FEA` is
  built) — result database/step/frame/field, primary-variable invariants,
  deformed-shape control, the Abaqus-style scalar bar, and the contour/cut
  helpers.
- `FViz.Cpp.Features` (`FVizTestCppFeatures`, only when `FEAViz::FEA` is
  built) — filter chain (threshold/warp/surface/slice/cell-to-point), the
  trackball interaction driving the camera through `processEvent`, and a
  headless `fea::FramePlayer` animation controller over ResultDatabase frames.
