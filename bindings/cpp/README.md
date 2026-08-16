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
                       UnstructuredGrid, PolyData
  FVizCppRendering.hpp Camera, LookupTable, Mapper, Actor, Scene, Renderer,
                       ScalarLegend, RendererWidget
  FVizCppIO.hpp        readVtu / readVtkLegacy / readObj / readStl
```

## Usage

Add the include directory and link the C core (shared or static):

```cmake
find_package(FEAViz 0.41 REQUIRED COMPONENTS Core)
target_include_directories(my_app PRIVATE path/to/bindings/cpp/include)
target_link_libraries(my_app PRIVATE FEAViz::Core)
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

The binding is exercised by `FVizTestCppBinding` (registered with CTest as
`FViz.Cpp.Binding`), which covers math operators, RAII refcounting, grid
construction, data arrays, readers, and headless rendering-object assembly.
