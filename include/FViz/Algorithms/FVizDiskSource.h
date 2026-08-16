#ifndef FVIZ_ALGORITHMS_DISK_SOURCE_H
#define FVIZ_ALGORITHMS_DISK_SOURCE_H

#include <stdint.h>

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizAlgorithm.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizDiskSource FVizDiskSource;
#define FVIZ_TYPE_DISK_SOURCE UINT64_C(0x87D0E3F12A6B4C59)

FVIZ_FILTERS_API FVizResult fviz_disk_source_create(FVizDiskSource** out_source);
FVIZ_FILTERS_API void fviz_disk_source_set_inner_radius(FVizDiskSource* source, double inner_radius);
FVIZ_FILTERS_API void fviz_disk_source_set_outer_radius(FVizDiskSource* source, double outer_radius);
FVIZ_FILTERS_API void fviz_disk_source_set_radial_resolution(FVizDiskSource* source, uint32_t radial_resolution);
FVIZ_FILTERS_API void fviz_disk_source_set_circumferential_resolution(FVizDiskSource* source, uint32_t circumferential_resolution);
FVIZ_FILTERS_API void fviz_disk_source_set_center(FVizDiskSource* source, FVizVec3 center);
FVIZ_FILTERS_API void fviz_disk_source_set_normal(FVizDiskSource* source, FVizVec3 normal);
FVIZ_FILTERS_API double fviz_disk_source_inner_radius(const FVizDiskSource* source);
FVIZ_FILTERS_API double fviz_disk_source_outer_radius(const FVizDiskSource* source);
FVIZ_FILTERS_API uint32_t fviz_disk_source_radial_resolution(const FVizDiskSource* source);
FVIZ_FILTERS_API uint32_t fviz_disk_source_circumferential_resolution(const FVizDiskSource* source);
FVIZ_FILTERS_API FVizVec3 fviz_disk_source_center(const FVizDiskSource* source);
FVIZ_FILTERS_API FVizVec3 fviz_disk_source_normal(const FVizDiskSource* source);
FVIZ_FILTERS_API FVizAlgorithm* fviz_disk_source_algorithm(FVizDiskSource* source);
FVIZ_FILTERS_API FVizAlgorithmOutput* fviz_disk_source_output_port(FVizDiskSource* source);
FVIZ_FILTERS_API FVizPolyData* fviz_disk_source_output(FVizDiskSource* source);
FVIZ_FILTERS_API FVizResult fviz_disk_source_update(FVizDiskSource* source);

FVIZ_EXTERN_C_END

#endif /* FVIZ_ALGORITHMS_DISK_SOURCE_H */
