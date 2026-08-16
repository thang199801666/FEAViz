#ifndef FVIZ_IO_MESH_READER_H
#define FVIZ_IO_MESH_READER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Mesh/FVizPolyData.h>

FVIZ_EXTERN_C_BEGIN

FVIZ_IO_API FVizResult fviz_mesh_read(const char* path, FVizPolyData** out_poly_data);
FVIZ_IO_API FVizBool fviz_mesh_format_supported(const char* path);

FVIZ_EXTERN_C_END

#endif /* FVIZ_IO_MESH_READER_H */
