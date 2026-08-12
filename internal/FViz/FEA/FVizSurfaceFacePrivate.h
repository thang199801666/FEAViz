#ifndef FVIZ_INTERNAL_FEA_SURFACE_FACE_PRIVATE_H
#define FVIZ_INTERNAL_FEA_SURFACE_FACE_PRIVATE_H

#include <stdint.h>

typedef struct FVizSurfaceFace
{
    uint32_t ids[4];
    uint32_t sorted[4];
    uint32_t count;
    uint32_t occurrences;
    FVizId source_cell;
    FVizId source_face;
} FVizSurfaceFace;

#endif /* FVIZ_INTERNAL_FEA_SURFACE_FACE_PRIVATE_H */
