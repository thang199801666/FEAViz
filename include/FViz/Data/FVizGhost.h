#ifndef FVIZ_DATA_GHOST_H
#define FVIZ_DATA_GHOST_H

#include <stdint.h>
#include <FViz/Core/FVizApi.h>

FVIZ_EXTERN_C_BEGIN

#define FVIZ_GHOST_ARRAY_NAME "FVizGhostType"
#define FVIZ_VTK_GHOST_ARRAY_NAME "vtkGhostType"
#define FVIZ_GHOST_LEVEL_ARRAY_NAME "FVizGhostLevel"

/* FEAViz point and cell ghost arrays intentionally share the duplicate bit.
 * Piece generation assigns deterministic point ownership across partitions so
 * only one copy of a shared source point is non-duplicate. VTU input using
 * VTK's vtkGhostType convention is normalized to these FEAViz flags. */
typedef enum FVizGhostFlags
{
    FVIZ_GHOST_NONE = 0,
    FVIZ_GHOST_DUPLICATE = 1u << 0,
    FVIZ_GHOST_HIDDEN = 1u << 1
} FVizGhostFlags;

FVIZ_EXTERN_C_END

#endif /* FVIZ_DATA_GHOST_H */
