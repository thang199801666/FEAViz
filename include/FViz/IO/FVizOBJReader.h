#ifndef FVIZ_IO_OBJ_READER_H
#define FVIZ_IO_OBJ_READER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Mesh/FVizPolyData.h>

FVIZ_EXTERN_C_BEGIN

FVIZ_IO_API FVizResult fviz_obj_read(const char* path, FVizPolyData** out_poly_data);

FVIZ_EXTERN_C_END

#endif /* FVIZ_IO_OBJ_READER_H */
