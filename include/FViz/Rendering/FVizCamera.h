#ifndef FVIZ_RENDERING_CAMERA_H
#define FVIZ_RENDERING_CAMERA_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Math/FVizMat4.h>
#include <FViz/Math/FVizVec3.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizCamera FVizCamera;
#define FVIZ_TYPE_CAMERA UINT64_C(0x027BBE745BF99062)

FVIZ_API FVizResult fviz_camera_create(FVizCamera** out_camera);
FVIZ_API void fviz_camera_set_position(FVizCamera* camera, FVizVec3 position);
FVIZ_API void fviz_camera_set_target(FVizCamera* camera, FVizVec3 target);
FVIZ_API void fviz_camera_set_up(FVizCamera* camera, FVizVec3 up);
FVIZ_API FVizVec3 fviz_camera_position(const FVizCamera* camera);
FVIZ_API FVizVec3 fviz_camera_target(const FVizCamera* camera);
FVIZ_API FVizVec3 fviz_camera_up(const FVizCamera* camera);
FVIZ_API void fviz_camera_set_perspective(FVizCamera* camera, float vertical_fov_degrees, float near_plane, float far_plane);
FVIZ_API float fviz_camera_fov_degrees(const FVizCamera* camera);
FVIZ_API FVizMat4 fviz_camera_view_matrix(const FVizCamera* camera);
FVIZ_API FVizMat4 fviz_camera_projection_matrix(const FVizCamera* camera, float aspect_ratio);
FVIZ_API void fviz_camera_fit_bounds(FVizCamera* camera, const FVizBounds* bounds, float padding);
FVIZ_API void fviz_camera_orbit(FVizCamera* camera, float yaw_radians, float pitch_radians);
FVIZ_API void fviz_camera_dolly(FVizCamera* camera, float factor);
FVIZ_API void fviz_camera_pan(FVizCamera* camera, float right_amount, float up_amount);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_CAMERA_H */
