#ifndef FVIZ_INTERNAL_RENDERING_MAPPER_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_MAPPER_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Rendering/FVizMapper.h>

struct FVizMapper
{
    FVizObject base;
    FVizAlgorithm* input_algorithm;
    uint32_t input_port;
    FVizPolyData* poly_data;
    FVizLookupTable* lookup_table;
    FVizBool scalar_visibility;
    FVizBool scalar_range_valid;
    float scalar_min;
    float scalar_max;
};

#endif /* FVIZ_INTERNAL_RENDERING_MAPPER_PRIVATE_H */
