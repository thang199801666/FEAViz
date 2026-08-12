#ifndef FVIZ_INTERNAL_RENDERING_CAMERA_PRIVATE_H
#define FVIZ_INTERNAL_RENDERING_CAMERA_PRIVATE_H

#include <FViz/Core/FVizObjectPrivate.h>
#include <FViz/Rendering/FVizCamera.h>

struct FVizCamera
{
    FVizObject base;
    FVizVec3 position;
    FVizVec3 target;
    FVizVec3 up;
    float fov_degrees;
    float near_plane;
    float far_plane;
};

#endif /* FVIZ_INTERNAL_RENDERING_CAMERA_PRIVATE_H */
