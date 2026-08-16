#ifndef FVIZ_IO_VTP_WRITER_H
#define FVIZ_IO_VTP_WRITER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Mesh/FVizPolyData.h>

FVIZ_EXTERN_C_BEGIN

typedef enum FVizVTPOutputMode
{
    FVIZ_VTP_OUTPUT_ASCII = 0
} FVizVTPOutputMode;

typedef struct FVizVTPWriterOptions
{
    uint32_t struct_size;
    FVizVTPOutputMode output_mode;
} FVizVTPWriterOptions;

FVIZ_API void fviz_vtp_writer_options_initialize(FVizVTPWriterOptions* options);
FVIZ_API FVizResult fviz_vtp_write(const char* file_path, const FVizPolyData* poly_data,
                                   const FVizVTPWriterOptions* options);

FVIZ_EXTERN_C_END

#endif /* FVIZ_IO_VTP_WRITER_H */
