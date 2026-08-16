#ifndef FVIZ_IO_PVTU_READER_H
#define FVIZ_IO_PVTU_READER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Data/FVizPartitionedDataSet.h>
#include <FViz/Data/FVizUnstructuredGrid.h>
#include <FViz/IO/FVizVTUReader.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizPVTUReader FVizPVTUReader;
#define FVIZ_TYPE_PVTU_READER UINT64_C(0x5B9C7E21D4A860F3)

typedef struct FVizPVTUReaderOptions
{
    uint32_t struct_size;
    FVizSize maximum_file_bytes;
    FVizSize maximum_pieces;
    FVizVTUReaderOptions piece_options;
} FVizPVTUReaderOptions;

typedef struct FVizPVTUCacheStatistics
{
    FVizSize capacity;
    FVizSize size;
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
    FVizSize byte_capacity;
    FVizSize bytes;
    uint64_t oversize_skips;
} FVizPVTUCacheStatistics;

FVIZ_IO_API void fviz_pvtu_reader_options_initialize(FVizPVTUReaderOptions* options);

/* Stateful manifest reader for large partitioned meshes. The manifest is
 * parsed once; individual VTU pieces are loaded on demand and may be retained
 * in a small LRU cache. Returned piece objects carry one reference owned by the
 * caller. */
FVIZ_IO_API FVizResult fviz_pvtu_reader_create(FVizPVTUReader** out_reader);
FVIZ_IO_API FVizResult fviz_pvtu_reader_create_with_options(const FVizPVTUReaderOptions* options,
                                                         FVizPVTUReader** out_reader);
FVIZ_IO_API FVizResult fviz_pvtu_reader_set_file_name(FVizPVTUReader* reader, const char* file_path);
FVIZ_IO_API const char* fviz_pvtu_reader_file_name(const FVizPVTUReader* reader);
FVIZ_IO_API FVizSize fviz_pvtu_reader_piece_count(const FVizPVTUReader* reader);
FVIZ_IO_API const char* fviz_pvtu_reader_piece_source(const FVizPVTUReader* reader, FVizSize piece_index);
FVIZ_IO_API uint32_t fviz_pvtu_reader_ghost_level(const FVizPVTUReader* reader);
FVIZ_IO_API FVizResult fviz_pvtu_reader_load_piece(FVizPVTUReader* reader, FVizSize piece_index,
                                                FVizUnstructuredGrid** out_piece);
FVIZ_IO_API FVizResult fviz_pvtu_reader_prefetch_piece(FVizPVTUReader* reader, FVizSize piece_index);
FVIZ_IO_API FVizResult fviz_pvtu_reader_materialize(FVizPVTUReader* reader, FVizPartitionedDataSet** out_data_set);
FVIZ_IO_API FVizResult fviz_pvtu_reader_set_cache_capacity(FVizPVTUReader* reader, FVizSize capacity);
FVIZ_IO_API FVizSize fviz_pvtu_reader_cache_capacity(const FVizPVTUReader* reader);
/* Optional resident-memory ceiling for cached VTU pieces. Zero means no byte
 * ceiling beyond the entry-count capacity. Objects larger than a non-zero
 * budget are returned normally but bypass the cache. */
FVIZ_IO_API FVizResult fviz_pvtu_reader_set_cache_byte_capacity(FVizPVTUReader* reader, FVizSize byte_capacity);
FVIZ_IO_API FVizSize fviz_pvtu_reader_cache_byte_capacity(const FVizPVTUReader* reader);
FVIZ_IO_API void fviz_pvtu_reader_clear_cache(FVizPVTUReader* reader);
FVIZ_IO_API FVizPVTUCacheStatistics fviz_pvtu_reader_cache_statistics(const FVizPVTUReader* reader);

/* Convenience API that materializes every referenced VTU piece as one child
 * of a PartitionedDataSet. For large datasets prefer the stateful reader above
 * and load/prefetch only the pieces required by the current workflow. */
FVIZ_IO_API FVizResult fviz_pvtu_read(const char* file_path, FVizPartitionedDataSet** out_data_set);
FVIZ_IO_API FVizResult fviz_pvtu_read_with_options(const char* file_path, const FVizPVTUReaderOptions* options,
                                                FVizPartitionedDataSet** out_data_set);

FVIZ_EXTERN_C_END

#endif /* FVIZ_IO_PVTU_READER_H */
