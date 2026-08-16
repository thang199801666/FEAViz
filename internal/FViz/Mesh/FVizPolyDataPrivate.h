#ifndef FVIZ_INTERNAL_MESH_POLY_DATA_PRIVATE_H
#define FVIZ_INTERNAL_MESH_POLY_DATA_PRIVATE_H

#include <stdint.h>

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Data/FVizAttributeSet.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Mesh/FVizCellArray.h>
#include <FViz/Mesh/FVizPolyData.h>

#define FVIZ_POLY_DATA_DIRTY_HISTORY_CAPACITY 16u

typedef struct FVizPolyDataDirtyRecord
{
    FVizMTime mtime;
    FVizSize first;
    FVizSize count;
    FVizBool full;
} FVizPolyDataDirtyRecord;

struct FVizPolyData
{
    FVizObject base;
    FVizArray* points;
    FVizArray* normals;
    /* Legacy render-ready topology kept for ABI/API compatibility. */
    FVizArray* indices;
    FVizArray* line_indices;
    /* VTK-style logical PolyData topology. */
    FVizCellArray* verts;
    FVizCellArray* lines;
    FVizCellArray* polys;
    FVizCellArray* strips;
    FVizDataArray* scalars;
    FVizAttributeSet* point_data;
    FVizAttributeSet* cell_data;
    FVizAttributeSet* field_data;
    FVizObserverTag scalars_modified_tag;
    FVizObserverTag point_data_modified_tag;
    FVizObserverTag cell_data_modified_tag;
    FVizObserverTag field_data_modified_tag;
    FVizBounds bounds;
    FVizMTime geometry_mtime;
    FVizMTime topology_mtime;
    FVizMTime attribute_mtime;
    FVizBool bounds_dirty;
    FVizBool normals_dirty;
    FVizPolyDataDirtyRecord geometry_dirty_history[FVIZ_POLY_DATA_DIRTY_HISTORY_CAPACITY];
    uint32_t geometry_dirty_begin;
    uint32_t geometry_dirty_count;
};

#endif /* FVIZ_INTERNAL_MESH_POLY_DATA_PRIVATE_H */
