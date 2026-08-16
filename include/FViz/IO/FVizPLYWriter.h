#ifndef FVIZ_IO_PLY_WRITER_H
#define FVIZ_IO_PLY_WRITER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Mesh/FVizPolyData.h>

FVIZ_EXTERN_C_BEGIN

typedef enum FVizPLYOutputMode
{
    FVIZ_PLY_OUTPUT_ASCII = 0,
    FVIZ_PLY_OUTPUT_BINARY_LITTLE_ENDIAN = 1
} FVizPLYOutputMode;

FVIZ_IO_API FVizResult fviz_ply_write(const char* file_path, const FVizPolyData* poly_data, FVizPLYOutputMode output_mode);

FVIZ_EXTERN_C_END

#endif /* FVIZ_IO_PLY_WRITER_H */
