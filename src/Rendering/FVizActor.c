#include <FViz/Core/FVizError.h>
#include <FViz/Math/FVizMat3.h>
#include <FViz/Rendering/FVizActor.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizActorPrivate.h>
#include <FViz/Rendering/FVizMapperPrivate.h>

static void fviz_actor_destroy(FVizObject* object);
static const FVizObjectClass g_fviz_actor_class = {
    FVIZ_TYPE_ACTOR,
    "FVizActor",
    &g_fviz_object_class,
    fviz_actor_destroy
};

static void fviz_actor_destroy(FVizObject* object)
{
    FVizActor* actor = (FVizActor*)object;
    fviz_release(actor->mapper);
    actor->mapper = NULL;
}

FVizResult fviz_actor_create(FVizActor** out_actor)
{
    FVizActor* actor;
    if (out_actor == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "out_actor must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    *out_actor = NULL;
    actor = (FVizActor*)fviz_internal_object_allocate(sizeof(FVizActor), &g_fviz_actor_class, NULL);
    if (actor == NULL) return fviz_last_error_code();
    if (fviz_mapper_create(&actor->mapper) != FVIZ_OK)
    {
        fviz_release(actor);
        return fviz_last_error_code();
    }
    actor->color[0] = 0.72f;
    actor->color[1] = 0.78f;
    actor->color[2] = 0.88f;
    actor->visible = FVIZ_TRUE;
    actor->wireframe = FVIZ_FALSE;
    actor->position = fviz_vec3(0.0f, 0.0f, 0.0f);
    actor->orientation = fviz_quat_identity();
    actor->scale = fviz_vec3(1.0f, 1.0f, 1.0f);
    *out_actor = actor;
    return FVIZ_OK;
}

FVizResult fviz_actor_set_mapper(FVizActor* actor, FVizMapper* mapper)
{
    if (actor == NULL || mapper == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "actor and mapper must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_retain(mapper) == NULL) return fviz_last_error_code();
    fviz_release(actor->mapper);
    actor->mapper = mapper;
    return FVIZ_OK;
}

FVizMapper* fviz_actor_mapper(FVizActor* actor) { return actor != NULL ? actor->mapper : NULL; }

FVizResult fviz_actor_set_poly_data(FVizActor* actor, FVizPolyData* poly_data)
{
    if (actor == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "actor must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    return fviz_mapper_set_poly_data(actor->mapper, poly_data);
}

FVizPolyData* fviz_actor_poly_data(FVizActor* actor) { return actor != NULL ? fviz_mapper_poly_data(actor->mapper) : NULL; }
const FVizPolyData* fviz_actor_const_poly_data(const FVizActor* actor) { return actor != NULL ? fviz_mapper_const_poly_data(actor->mapper) : NULL; }

void fviz_actor_set_color(FVizActor* actor, float red, float green, float blue)
{
    if (actor == NULL) return;
    actor->color[0] = red;
    actor->color[1] = green;
    actor->color[2] = blue;
}

void fviz_actor_get_color(const FVizActor* actor, float* red, float* green, float* blue)
{
    if (actor == NULL) return;
    if (red != NULL) *red = actor->color[0];
    if (green != NULL) *green = actor->color[1];
    if (blue != NULL) *blue = actor->color[2];
}

void fviz_actor_set_visible(FVizActor* actor, FVizBool visible)
{
    if (actor != NULL) actor->visible = visible != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
}
FVizBool fviz_actor_is_visible(const FVizActor* actor) { return actor != NULL ? actor->visible : FVIZ_FALSE; }
void fviz_actor_set_wireframe(FVizActor* actor, FVizBool enabled) { if (actor != NULL) actor->wireframe = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE; }
FVizBool fviz_actor_wireframe(const FVizActor* actor) { return actor != NULL ? actor->wireframe : FVIZ_FALSE; }

void fviz_actor_set_position(FVizActor* actor, FVizVec3 position) { if (actor != NULL) actor->position = position; }
FVizVec3 fviz_actor_position(const FVizActor* actor) { return actor != NULL ? actor->position : fviz_vec3(0.0f, 0.0f, 0.0f); }
void fviz_actor_set_orientation(FVizActor* actor, FVizQuat orientation)
{
    if (actor != NULL) actor->orientation = fviz_quat_normalize(orientation);
}
FVizQuat fviz_actor_orientation(const FVizActor* actor) { return actor != NULL ? actor->orientation : fviz_quat_identity(); }
void fviz_actor_set_scale(FVizActor* actor, FVizVec3 scale)
{
    if (actor != NULL)
    {
        actor->scale.x = scale.x != 0.0f ? scale.x : 1.0f;
        actor->scale.y = scale.y != 0.0f ? scale.y : 1.0f;
        actor->scale.z = scale.z != 0.0f ? scale.z : 1.0f;
    }
}
FVizVec3 fviz_actor_scale(const FVizActor* actor) { return actor != NULL ? actor->scale : fviz_vec3(1.0f, 1.0f, 1.0f); }

FVizMat4 fviz_actor_transform_matrix(const FVizActor* actor)
{
    FVizMat3 rotation;
    FVizMat4 result = fviz_mat4_identity();
    if (actor == NULL) return result;
    rotation = fviz_mat3_from_quaternion(actor->orientation);

    result.m[0] = rotation.m[0] * actor->scale.x;
    result.m[1] = rotation.m[1] * actor->scale.x;
    result.m[2] = rotation.m[2] * actor->scale.x;
    result.m[4] = rotation.m[3] * actor->scale.y;
    result.m[5] = rotation.m[4] * actor->scale.y;
    result.m[6] = rotation.m[5] * actor->scale.y;
    result.m[8] = rotation.m[6] * actor->scale.z;
    result.m[9] = rotation.m[7] * actor->scale.z;
    result.m[10] = rotation.m[8] * actor->scale.z;
    result.m[12] = actor->position.x;
    result.m[13] = actor->position.y;
    result.m[14] = actor->position.z;
    return result;
}
