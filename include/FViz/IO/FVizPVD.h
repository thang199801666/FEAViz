#ifndef FVIZ_IO_PVD_H
#define FVIZ_IO_PVD_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizPVDCollection FVizPVDCollection;
#define FVIZ_TYPE_PVD_COLLECTION UINT64_C(0xCF087F35A6D02149)

FVIZ_IO_API FVizResult fviz_pvd_collection_create(FVizPVDCollection** out_collection);
FVIZ_IO_API FVizSize fviz_pvd_collection_count(const FVizPVDCollection* collection);
FVIZ_IO_API FVizResult fviz_pvd_collection_add(FVizPVDCollection* collection, double time, uint32_t part,
                                            const char* group, const char* file, FVizSize* out_index);
FVIZ_IO_API double fviz_pvd_collection_time(const FVizPVDCollection* collection, FVizSize index);
FVIZ_IO_API uint32_t fviz_pvd_collection_part(const FVizPVDCollection* collection, FVizSize index);
FVIZ_IO_API const char* fviz_pvd_collection_group(const FVizPVDCollection* collection, FVizSize index);
FVIZ_IO_API const char* fviz_pvd_collection_file(const FVizPVDCollection* collection, FVizSize index);
FVIZ_IO_API FVizResult fviz_pvd_collection_time_range(const FVizPVDCollection* collection, double* out_minimum,
                                                   double* out_maximum);
FVIZ_IO_API FVizResult fviz_pvd_collection_find_nearest(const FVizPVDCollection* collection, double time,
                                                     FVizSize* out_index);
FVIZ_IO_API void fviz_pvd_collection_clear(FVizPVDCollection* collection);
FVIZ_IO_API FVizResult fviz_pvd_read(const char* file_path, FVizPVDCollection** out_collection);
FVIZ_IO_API FVizResult fviz_pvd_write(const char* file_path, const FVizPVDCollection* collection);

FVIZ_EXTERN_C_END

#endif /* FVIZ_IO_PVD_H */
