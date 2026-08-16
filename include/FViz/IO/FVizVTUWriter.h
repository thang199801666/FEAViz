#ifndef FVIZ_IO_VTU_WRITER_H
#define FVIZ_IO_VTU_WRITER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Data/FVizUnstructuredGrid.h>

FVIZ_EXTERN_C_BEGIN

typedef enum FVizVTUOutputMode
{
    FVIZ_VTU_OUTPUT_ASCII = 0,
    FVIZ_VTU_OUTPUT_APPENDED_RAW = 1
} FVizVTUOutputMode;

typedef enum FVizVTUHeaderWidth
{
    FVIZ_VTU_HEADER_UINT32 = 4,
    FVIZ_VTU_HEADER_UINT64 = 8
} FVizVTUHeaderWidth;

typedef struct FVizVTUWriterOptions
{
    uint32_t struct_size;
    FVizVTUOutputMode output_mode;
    FVizVTUHeaderWidth header_width;
    FVizBool compress;
} FVizVTUWriterOptions;

FVIZ_API void fviz_vtu_writer_options_initialize(FVizVTUWriterOptions* options);
FVIZ_API FVizResult fviz_vtu_write(
    const char* file_path,
    const FVizUnstructuredGrid* grid,
    const FVizVTUWriterOptions* options);

FVIZ_EXTERN_C_END

#endif /* FVIZ_IO_VTU_WRITER_H */
