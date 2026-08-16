#ifndef FVIZ_IO_VTK_LEGACY_READER_H
#define FVIZ_IO_VTK_LEGACY_READER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Data/FVizUnstructuredGrid.h>

FVIZ_EXTERN_C_BEGIN

FVIZ_API FVizResult fviz_vtk_legacy_read(const char* file_path, FVizUnstructuredGrid** out_grid);

FVIZ_EXTERN_C_END

#endif /* FVIZ_IO_VTK_LEGACY_READER_H */
