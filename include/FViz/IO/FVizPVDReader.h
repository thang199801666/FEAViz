#ifndef FVIZ_IO_PVD_READER_H
#define FVIZ_IO_PVD_READER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Data/FVizDataObject.h>
#include <FViz/IO/FVizPVD.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizPVDReader FVizPVDReader;
#define FVIZ_TYPE_PVD_READER UINT64_C(0xA2E6D014B7F39C55)

typedef struct FVizPVDCacheStatistics
{
    FVizSize capacity;
    FVizSize size;
    uint64_t hits;
    uint64_t misses;
    uint64_t evictions;
    FVizSize byte_capacity;
    FVizSize bytes;
    uint64_t oversize_skips;
} FVizPVDCacheStatistics;

FVIZ_API FVizResult fviz_pvd_reader_create(FVizPVDReader** out_reader);
FVIZ_API FVizResult fviz_pvd_reader_set_file_name(FVizPVDReader* reader, const char* file_path);
FVIZ_API const char* fviz_pvd_reader_file_name(const FVizPVDReader* reader);
FVIZ_API const FVizPVDCollection* fviz_pvd_reader_collection(const FVizPVDReader* reader);
FVIZ_API FVizAlgorithm* fviz_pvd_reader_algorithm(FVizPVDReader* reader);
FVIZ_API FVizAlgorithmOutput* fviz_pvd_reader_output_port(FVizPVDReader* reader);
FVIZ_API FVizDataObject* fviz_pvd_reader_output(FVizPVDReader* reader);
FVIZ_API FVizResult fviz_pvd_reader_update(FVizPVDReader* reader);
FVIZ_API FVizResult fviz_pvd_reader_update_time(FVizPVDReader* reader, double time);
/* Demand-driven temporal piece request. For a PVD timestep backed by one PVTU
 * file this loads only the requested VTU piece. For a timestep containing
 * multiple PVD entries, one matching part/position is loaded. The default
 * update_time() path continues to materialize the whole timestep group. */
FVIZ_API FVizResult fviz_pvd_reader_update_piece_time(FVizPVDReader* reader, double time, uint32_t piece,
                                                      uint32_t number_of_pieces, uint32_t ghost_levels);
FVIZ_API FVizResult fviz_pvd_reader_piece_count_at_time(FVizPVDReader* reader, double time, uint32_t* out_piece_count);
FVIZ_API double fviz_pvd_reader_selected_time(const FVizPVDReader* reader);
/* Synchronously warms the frame cache for the timestep nearest to time without
 * replacing the reader output or selected_time. Useful for animation look-ahead. */
FVIZ_API FVizResult fviz_pvd_reader_prefetch_time(FVizPVDReader* reader, double time);
FVIZ_API FVizResult fviz_pvd_reader_prefetch_piece_time(FVizPVDReader* reader, double time, uint32_t piece,
                                                        uint32_t number_of_pieces, uint32_t ghost_levels);
/* A small LRU working set avoids reparsing recent frames while scrubbing animations.
 * The default capacity is three timestep groups. Set zero to disable frame caching. */
FVIZ_API FVizResult fviz_pvd_reader_set_cache_capacity(FVizPVDReader* reader, FVizSize capacity);
FVIZ_API FVizSize fviz_pvd_reader_cache_capacity(const FVizPVDReader* reader);
FVIZ_API FVizResult fviz_pvd_reader_set_cache_byte_capacity(FVizPVDReader* reader, FVizSize byte_capacity);
FVIZ_API FVizSize fviz_pvd_reader_cache_byte_capacity(const FVizPVDReader* reader);
FVIZ_API void fviz_pvd_reader_clear_cache(FVizPVDReader* reader);
FVIZ_API FVizPVDCacheStatistics fviz_pvd_reader_cache_statistics(const FVizPVDReader* reader);

FVIZ_EXTERN_C_END

#endif /* FVIZ_IO_PVD_READER_H */
