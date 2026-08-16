#ifndef FVIZ_IO_VTP_READER_H
#define FVIZ_IO_VTP_READER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Mesh/FVizPolyData.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizVTPReaderOptions
{
    uint32_t struct_size;
    FVizSize maximum_file_bytes;
    FVizSize maximum_points;
    FVizSize maximum_cells;
    FVizSize maximum_connectivity_values;
    FVizSize maximum_array_values;
} FVizVTPReaderOptions;

FVIZ_API void fviz_vtp_reader_options_initialize(FVizVTPReaderOptions* options);
FVIZ_API FVizResult fviz_vtp_read(const char* file_path, FVizPolyData** out_poly_data);
FVIZ_API FVizResult fviz_vtp_read_with_options(
    const char* file_path,
    const FVizVTPReaderOptions* options,
    FVizPolyData** out_poly_data);

FVIZ_EXTERN_C_END

#endif /* FVIZ_IO_VTP_READER_H */
