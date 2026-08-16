#ifndef FVIZ_IO_PVTU_WRITER_H
#define FVIZ_IO_PVTU_WRITER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Data/FVizPartitionedDataSet.h>
#include <FViz/IO/FVizVTUWriter.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizPVTUWriterOptions
{
    uint32_t struct_size;
    FVizVTUWriterOptions piece_options;
} FVizPVTUWriterOptions;

FVIZ_API void fviz_pvtu_writer_options_initialize(FVizPVTUWriterOptions* options);

/* Writes one .pvtu manifest plus one sibling .vtu file per partition. Piece
 * file names are derived from the manifest basename as
 * <stem>_pieceNNNNN.vtu and referenced relatively from the manifest. Every
 * partition must be an UnstructuredGrid. */
FVIZ_API FVizResult fviz_pvtu_write(const char* file_path, const FVizPartitionedDataSet* data_set,
                                    const FVizPVTUWriterOptions* options);

FVIZ_EXTERN_C_END

#endif /* FVIZ_IO_PVTU_WRITER_H */
