# Modification time and cache invalidation

FEAViz 0.8.0 introduces a VTK-style modification-time contract for demand-driven execution and rendering caches.

Every `FVizObject` receives a non-zero 64-bit timestamp when it is created. `fviz_object_modified()` advances that timestamp using a process-wide atomic counter, and `fviz_object_mtime()` returns the current value.

```c
FVizMTime before = fviz_object_mtime((FVizObject*)array);

fviz_data_array_set_tuple(array, tuple_id, values);

FVizMTime after = fviz_object_mtime((FVizObject*)array);
/* after > before */
```

## Composite MTime

Owning data objects return the maximum of their local time and relevant child-object times:

```text
FVizUnstructuredGrid
  +-- FVizPoints
  +-- FVizCellArray
  +-- FVizDataSet
        +-- point FVizAttributeSet -> FVizDataArray(s)
        +-- cell  FVizAttributeSet -> FVizDataArray(s)
        +-- field FVizAttributeSet -> FVizDataArray(s)
```

`FVizPolyData` similarly includes topology arrays, normals, active scalars, and point attributes. Therefore changing a retained child array automatically changes the parent's observed MTime without parent callbacks or ownership cycles.

Connected filters, contour filters, OpenGL resources, and picking BVHs cache the composite input MTime. A displacement or result-field edit is consequently visible at the next renderer update.

## Mutable raw pointers

Normal mutating functions call `fviz_object_modified()` automatically. The library cannot detect writes through raw pointers returned by functions such as `fviz_data_array_data()` or `fviz_buffer_data()`. After such writes, applications must explicitly mark the owner:

```c
float* values = (float*)fviz_data_array_data(array);
values[index] = new_value;
fviz_object_modified((FVizObject*)array);
```

MTime reads and updates are atomic, and concurrent calls to `fviz_object_modified()` cannot move an object's timestamp backward. This does not make container or mesh mutation itself thread-safe; callers must still synchronize simultaneous data writes.
