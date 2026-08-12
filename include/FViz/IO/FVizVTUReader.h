#ifndef FVIZ_IO_VTU_READER_H
#define FVIZ_IO_VTU_READER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/FEA/FVizUnstructuredGrid.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizVTUReaderOptions
{
    uint32_t struct_size;
    FVizSize maximum_file_bytes;
    FVizSize maximum_points;
    FVizSize maximum_cells;
    FVizSize maximum_connectivity_values;
    FVizSize maximum_array_values;
} FVizVTUReaderOptions;

FVIZ_API void fviz_vtu_reader_options_initialize(FVizVTUReaderOptions* options);
FVIZ_API FVizResult fviz_vtu_read(const char* file_path, FVizUnstructuredGrid** out_grid);
FVIZ_API FVizResult fviz_vtu_read_with_options(
    const char* file_path,
    const FVizVTUReaderOptions* options,
    FVizUnstructuredGrid** out_grid);

FVIZ_EXTERN_C_END

#endif /* FVIZ_IO_VTU_READER_H */
