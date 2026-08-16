#ifndef FVIZ_INTERNAL_RENDERING_GLYPH_MAPPER_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_GLYPH_MAPPER_PRIVATE_H

#include <FViz/Core/FVizArray.h>
#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Rendering/FVizGlyphMapper.h>

#define FVIZ_GLYPH_DIRTY_HISTORY_CAPACITY 16u

typedef struct FVizGlyphDirtyRecord
{
    FVizMTime mtime;
    FVizSize first;
    FVizSize count;
    FVizBool full;
} FVizGlyphDirtyRecord;

struct FVizGlyphMapper
{
    FVizObject base;
    FVizPolyData* source;
    FVizArray* instances;
    FVizBool has_translucent_instances;
    FVizBool gpu_residency_pinned;
    FVizGlyphDirtyRecord dirty_history[FVIZ_GLYPH_DIRTY_HISTORY_CAPACITY];
    uint32_t dirty_history_begin;
    uint32_t dirty_history_count;
};

FVizMTime fviz_internal_glyph_mapper_instances_mtime(const FVizGlyphMapper* mapper);
FVizResult fviz_internal_glyph_mapper_dirty_range_since(const FVizGlyphMapper* mapper, FVizMTime since_mtime,
                                                        FVizDirtyRange* out_range);

#endif /* FVIZ_INTERNAL_RENDERING_GLYPH_MAPPER_PRIVATE_H */
