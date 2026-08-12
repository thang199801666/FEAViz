#include <FViz/Core/FVizError.h>
#include <FViz/Rendering/FVizActor.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizActorPrivate.h>

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
    fviz_release(actor->poly_data);
    actor->poly_data = NULL;
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
    actor->color[0] = 0.72f;
    actor->color[1] = 0.78f;
    actor->color[2] = 0.88f;
    actor->visible = FVIZ_TRUE;
    actor->wireframe = FVIZ_FALSE;
    *out_actor = actor;
    return FVIZ_OK;
}

FVizResult fviz_actor_set_poly_data(FVizActor* actor, FVizPolyData* poly_data)
{
    if (actor == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "actor must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (poly_data != NULL && fviz_retain(poly_data) == NULL)
    {
        return fviz_last_error_code();
    }
    fviz_release(actor->poly_data);
    actor->poly_data = poly_data;
    return FVIZ_OK;
}

FVizPolyData* fviz_actor_poly_data(FVizActor* actor) { return actor != NULL ? actor->poly_data : NULL; }
const FVizPolyData* fviz_actor_const_poly_data(const FVizActor* actor) { return actor != NULL ? actor->poly_data : NULL; }

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
