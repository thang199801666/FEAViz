#ifndef FVIZ_IO_VTU_READER_H
#define FVIZ_IO_VTU_READER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/FEA/FVizUnstructuredGrid.h>

FVIZ_EXTERN_C_BEGIN

FVIZ_API FVizResult fviz_vtu_read(const char* file_path, FVizUnstructuredGrid** out_grid);

FVIZ_EXTERN_C_END

#endif /* FVIZ_IO_VTU_READER_H */
