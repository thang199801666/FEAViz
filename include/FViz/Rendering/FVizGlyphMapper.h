#ifndef FVIZ_RENDERING_GLYPH_MAPPER_H
#define FVIZ_RENDERING_GLYPH_MAPPER_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Math/FVizQuat.h>
#include <FViz/Math/FVizVec3.h>
#include <FViz/Mesh/FVizPolyData.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizGlyphMapper FVizGlyphMapper;
#define FVIZ_TYPE_GLYPH_MAPPER UINT64_C(0xD4598CB63549B7A2)

typedef struct FVizGlyphInstance
{
    FVizVec3 position;
    FVizQuat orientation;
    FVizVec3 scale;
    float color[4];
} FVizGlyphInstance;

typedef struct FVizVectorGlyphOptions
{
    uint32_t struct_size;
    float scale_factor;
    FVizBool scale_by_magnitude;
    FVizBool color_by_magnitude;
    float low_color[3];
    float high_color[3];
    float opacity;
} FVizVectorGlyphOptions;

FVIZ_API void fviz_glyph_instance_initialize(FVizGlyphInstance* instance);
FVIZ_API void fviz_vector_glyph_options_initialize(FVizVectorGlyphOptions* options);
FVIZ_API FVizResult fviz_glyph_mapper_create(FVizGlyphMapper** out_mapper);
FVIZ_API FVizResult fviz_glyph_mapper_set_source_poly_data(FVizGlyphMapper* mapper, FVizPolyData* source);
FVIZ_API FVizPolyData* fviz_glyph_mapper_source_poly_data(FVizGlyphMapper* mapper);
FVIZ_API const FVizPolyData* fviz_glyph_mapper_const_source_poly_data(const FVizGlyphMapper* mapper);
FVIZ_API FVizResult fviz_glyph_mapper_reserve_instances(FVizGlyphMapper* mapper, FVizSize capacity);
FVIZ_API FVizResult fviz_glyph_mapper_add_instance(FVizGlyphMapper* mapper, const FVizGlyphInstance* instance);
FVIZ_API FVizResult fviz_glyph_mapper_add_instances(FVizGlyphMapper* mapper, const FVizGlyphInstance* instances,
                                                    FVizSize count);
FVIZ_API FVizResult fviz_glyph_mapper_set_instance(FVizGlyphMapper* mapper, FVizSize index,
                                                   const FVizGlyphInstance* instance);
FVIZ_API FVizResult fviz_glyph_mapper_set_instances(FVizGlyphMapper* mapper, FVizSize first,
                                                    const FVizGlyphInstance* instances, FVizSize count);
FVIZ_API void fviz_glyph_mapper_clear_instances(FVizGlyphMapper* mapper);
FVIZ_API FVizSize fviz_glyph_mapper_instance_count(const FVizGlyphMapper* mapper);
FVIZ_API FVizBool fviz_glyph_mapper_has_translucent_instances(const FVizGlyphMapper* mapper);
FVIZ_API const FVizGlyphInstance* fviz_glyph_mapper_instances(const FVizGlyphMapper* mapper);
FVIZ_API FVizResult fviz_glyph_mapper_get_instance(const FVizGlyphMapper* mapper, FVizSize index,
                                                   FVizGlyphInstance* out_instance);
FVIZ_API FVizBounds fviz_glyph_mapper_bounds(const FVizGlyphMapper* mapper);
FVIZ_API void fviz_glyph_mapper_set_gpu_residency_pinned(FVizGlyphMapper* mapper, FVizBool pinned);
FVIZ_API FVizBool fviz_glyph_mapper_gpu_residency_pinned(const FVizGlyphMapper* mapper);
/* Builds one glyph per non-zero point vector. vector_array_name==NULL uses active vectors. */
FVIZ_API FVizResult fviz_glyph_mapper_build_from_point_vectors(FVizGlyphMapper* mapper, const FVizPolyData* input,
                                                               const char* vector_array_name,
                                                               const FVizVectorGlyphOptions* options);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_GLYPH_MAPPER_H */
