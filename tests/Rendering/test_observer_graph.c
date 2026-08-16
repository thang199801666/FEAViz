#include <stdio.h>

#include <FViz/Algorithms/FVizCubeSource.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Math/FVizTransform.h>
#include <FViz/Mesh/FVizPolyData.h>
#include <FViz/Pipeline/FVizFilter.h>
#include <FViz/Rendering/FVizActor.h>
#include <FViz/Rendering/FVizCamera.h>
#include <FViz/Rendering/FVizMapper.h>
#include <FViz/Rendering/FVizLight.h>
#include <FViz/Rendering/FVizRenderPass.h>
#include <FViz/Rendering/FVizScalarLegend.h>
#include <FViz/Rendering/FVizTextActor.h>
#include <FViz/Rendering/FVizRenderer.h>
#include <FViz/Rendering/FVizScene.h>

static int require_true(int value, const char* message)
{
    if (!value)
    {
        fprintf(stderr, "%s\n", message);
        return 0;
    }
    return 1;
}

static FVizBool count_modified(
    FVizObject* caller,
    FVizEventId event_id,
    void* call_data,
    void* client_data)
{
    int* count = (int*)client_data;
    (void)caller;
    (void)call_data;
    if (event_id == FVIZ_EVENT_MODIFIED && count != NULL) ++(*count);
    return FVIZ_FALSE;
}

static int test_shared_mapper_dependencies(void)
{
    FVizActor* actor_a = NULL;
    FVizActor* actor_b = NULL;
    FVizMapper* shared_mapper = NULL;
    FVizMapper* replacement_mapper = NULL;
    FVizObserverTag tag_a = FVIZ_OBSERVER_TAG_INVALID;
    FVizObserverTag tag_b = FVIZ_OBSERVER_TAG_INVALID;
    int modified_a = 0;
    int modified_b = 0;
    int before_a;
    int before_b;

    if (!require_true(fviz_actor_create(&actor_a) == FVIZ_OK, "shared-mapper actor A create failed")) return 0;
    if (!require_true(fviz_actor_create(&actor_b) == FVIZ_OK, "shared-mapper actor B create failed")) return 0;
    if (!require_true(fviz_mapper_create(&shared_mapper) == FVIZ_OK, "shared mapper create failed")) return 0;
    if (!require_true(fviz_mapper_create(&replacement_mapper) == FVIZ_OK, "replacement shared mapper create failed")) return 0;
    if (!require_true(fviz_actor_set_mapper(actor_a, shared_mapper) == FVIZ_OK, "actor A shared mapper attach failed")) return 0;
    if (!require_true(fviz_actor_set_mapper(actor_b, shared_mapper) == FVIZ_OK, "actor B shared mapper attach failed")) return 0;
    if (!require_true(fviz_object_add_observer(
            (FVizObject*)actor_a, FVIZ_EVENT_MODIFIED, 0.0f,
            count_modified, &modified_a, &tag_a) == FVIZ_OK,
            "actor A observer registration failed")) return 0;
    if (!require_true(fviz_object_add_observer(
            (FVizObject*)actor_b, FVIZ_EVENT_MODIFIED, 0.0f,
            count_modified, &modified_b, &tag_b) == FVIZ_OK,
            "actor B observer registration failed")) return 0;

    before_a = modified_a;
    before_b = modified_b;
    fviz_mapper_set_scalar_visibility(shared_mapper, FVIZ_TRUE);
    if (!require_true(modified_a > before_a && modified_b > before_b,
            "shared mapper ModifiedEvent did not reach both actors")) return 0;

    if (!require_true(fviz_actor_set_mapper(actor_a, replacement_mapper) == FVIZ_OK,
            "actor A mapper replacement failed")) return 0;
    before_a = modified_a;
    before_b = modified_b;
    fviz_mapper_set_scalar_visibility(shared_mapper, FVIZ_FALSE);
    if (!require_true(modified_a == before_a,
            "actor A retained a ghost dependency on detached shared mapper")) return 0;
    if (!require_true(modified_b > before_b,
            "actor B lost dependency on mapper still shared by it")) return 0;

    before_a = modified_a;
    before_b = modified_b;
    fviz_mapper_set_scalar_visibility(replacement_mapper, FVIZ_TRUE);
    if (!require_true(modified_a > before_a,
            "actor A did not observe its replacement mapper")) return 0;
    if (!require_true(modified_b == before_b,
            "actor B incorrectly observed actor A replacement mapper")) return 0;

    (void)fviz_object_remove_observer((FVizObject*)actor_a, tag_a);
    (void)fviz_object_remove_observer((FVizObject*)actor_b, tag_b);
    fviz_release(replacement_mapper);
    fviz_release(shared_mapper);
    fviz_release(actor_b);
    fviz_release(actor_a);
    return 1;
}

int main(void)
{
    FVizRenderer* renderer = NULL;
    FVizScene* original_scene = NULL;
    FVizScene* replacement_scene = NULL;
    FVizActor* actor = NULL;
    FVizMapper* original_mapper = NULL;
    FVizMapper* replacement_mapper = NULL;
    FVizTransform* transform = NULL;
    FVizFilter* surface_filter = NULL;
    FVizPolyData* direct_poly_data = NULL;
    FVizCubeSource* cube_source = NULL;
    FVizLight* extra_light = NULL;
    FVizScalarLegend* legend = NULL;
    FVizTextActor2D* text_actor = NULL;
    FVizRenderPass* custom_pass = NULL;
    FVizObserverTag renderer_tag = FVIZ_OBSERVER_TAG_INVALID;
    int modified_count = 0;
    int before;

    if (!test_shared_mapper_dependencies()) return 1;
    if (!require_true(fviz_renderer_create(&renderer) == FVIZ_OK, "renderer create failed")) return 1;
    original_scene = fviz_renderer_scene(renderer);
    if (!require_true(original_scene != NULL, "renderer scene missing")) return 1;
    if (!require_true(fviz_object_add_observer(
            (FVizObject*)renderer, FVIZ_EVENT_MODIFIED, 0.0f,
            count_modified, &modified_count, &renderer_tag) == FVIZ_OK,
            "renderer observer registration failed")) return 1;

    before = modified_count;
    fviz_camera_orbit(fviz_renderer_camera(renderer), 0.05f, -0.02f);
    if (!require_true(modified_count > before, "camera ModifiedEvent did not propagate to renderer")) return 1;

    if (!require_true(fviz_light_create(&extra_light) == FVIZ_OK, "light create failed")) return 1;
    if (!require_true(fviz_renderer_add_light(renderer, extra_light) == FVIZ_OK, "add light failed")) return 1;
    before = modified_count;
    fviz_light_set_intensity(extra_light, 0.65f);
    if (!require_true(modified_count > before, "light ModifiedEvent did not propagate to renderer")) return 1;
    if (!require_true(fviz_renderer_remove_light(renderer, extra_light) == FVIZ_OK, "remove light failed")) return 1;
    before = modified_count;
    fviz_light_set_intensity(extra_light, 0.45f);
    if (!require_true(modified_count == before, "detached light still propagated ghost ModifiedEvent")) return 1;

    if (!require_true(fviz_scalar_legend_create(&legend) == FVIZ_OK, "scalar legend create failed")) return 1;
    fviz_renderer_set_scalar_legend(renderer, legend);
    before = modified_count;
    fviz_scalar_legend_set_range(legend, -1.0f, 1.0f);
    if (!require_true(modified_count > before, "scalar-legend ModifiedEvent did not propagate to renderer")) return 1;
    before = modified_count;
    fviz_text_property_set_font_size(fviz_scalar_legend_title_text_property(legend), 18.0f);
    if (!require_true(modified_count > before,
            "scalar-legend text-property ModifiedEvent did not propagate to renderer")) return 1;
    fviz_renderer_set_scalar_legend(renderer, NULL);
    before = modified_count;
    fviz_scalar_legend_set_range(legend, -2.0f, 2.0f);
    if (!require_true(modified_count == before, "detached scalar legend still propagated ghost ModifiedEvent")) return 1;

    if (!require_true(fviz_text_actor_2d_create(&text_actor) == FVIZ_OK, "text actor create failed")) return 1;
    if (!require_true(fviz_renderer_add_text_actor_2d(renderer, text_actor) == FVIZ_OK, "add text actor failed")) return 1;
    before = modified_count;
    if (!require_true(fviz_text_actor_2d_set_text(text_actor, "observer graph") == FVIZ_OK, "text actor modify failed")) return 1;
    if (!require_true(modified_count > before, "text-actor ModifiedEvent did not propagate to renderer")) return 1;
    before = modified_count;
    fviz_text_property_set_font_size(fviz_text_actor_2d_text_property(text_actor), 20.0f);
    if (!require_true(modified_count > before,
            "text-property ModifiedEvent did not propagate through text actor to renderer")) return 1;
    if (!require_true(fviz_renderer_remove_text_actor_2d(renderer, text_actor) == FVIZ_OK, "remove text actor failed")) return 1;
    before = modified_count;
    fviz_text_actor_2d_set_visible(text_actor, FVIZ_FALSE);
    if (!require_true(modified_count == before, "detached text actor still propagated ghost ModifiedEvent")) return 1;

    if (!require_true(fviz_render_pass_create(
            FVIZ_RENDER_PASS_OVERLAY, NULL, NULL, NULL, &custom_pass) == FVIZ_OK,
            "render pass create failed")) return 1;
    if (!require_true(fviz_renderer_add_pass(renderer, custom_pass) == FVIZ_OK, "add render pass failed")) return 1;
    before = modified_count;
    fviz_object_modified((FVizObject*)custom_pass);
    if (!require_true(modified_count > before, "render-pass ModifiedEvent did not propagate to renderer")) return 1;
    if (!require_true(fviz_renderer_remove_pass(renderer, custom_pass) == FVIZ_OK, "remove render pass failed")) return 1;
    before = modified_count;
    fviz_object_modified((FVizObject*)custom_pass);
    if (!require_true(modified_count == before, "detached render pass still propagated ghost ModifiedEvent")) return 1;

    if (!require_true(fviz_actor_create(&actor) == FVIZ_OK, "actor create failed")) return 1;
    before = modified_count;
    if (!require_true(fviz_scene_add_actor(original_scene, actor) == FVIZ_OK, "scene add actor failed")) return 1;
    if (!require_true(modified_count > before, "scene structure change did not propagate to renderer")) return 1;

    before = modified_count;
    fviz_actor_set_color(actor, 0.2f, 0.4f, 0.8f);
    if (!require_true(modified_count > before, "actor ModifiedEvent did not propagate through scene")) return 1;

    original_mapper = (FVizMapper*)fviz_retain(fviz_actor_mapper(actor));
    if (!require_true(original_mapper != NULL, "retain original mapper failed")) return 1;
    if (!require_true(fviz_mapper_create(&replacement_mapper) == FVIZ_OK, "mapper create failed")) return 1;
    if (!require_true(fviz_actor_set_mapper(actor, replacement_mapper) == FVIZ_OK, "set mapper failed")) return 1;
    before = modified_count;
    fviz_mapper_set_scalar_visibility(original_mapper, FVIZ_TRUE);
    if (!require_true(modified_count == before, "detached mapper still propagated ghost ModifiedEvent")) return 1;
    before = modified_count;
    fviz_mapper_set_scalar_visibility(replacement_mapper, FVIZ_TRUE);
    if (!require_true(modified_count > before, "mapper ModifiedEvent did not propagate through actor/scene")) return 1;

    if (!require_true(fviz_surface_filter_create(FVIZ_TRUE, &surface_filter) == FVIZ_OK,
            "surface filter create failed")) return 1;
    if (!require_true(fviz_mapper_set_input_connection(replacement_mapper, surface_filter) == FVIZ_OK,
            "mapper pipeline connection failed")) return 1;
    before = modified_count;
    if (!require_true(fviz_surface_filter_set_transfer_scalars(surface_filter, FVIZ_FALSE) == FVIZ_OK,
            "surface filter modification failed")) return 1;
    if (!require_true(modified_count > before,
            "producer Algorithm ModifiedEvent did not propagate through mapper/actor/scene")) return 1;

    before = modified_count;
    if (!require_true(fviz_lookup_table_set_color(
            fviz_mapper_lookup_table(replacement_mapper), 0u, 0.1f, 0.2f, 0.3f) == FVIZ_OK,
            "lookup-table modification failed")) return 1;
    if (!require_true(modified_count > before,
            "lookup-table ModifiedEvent did not propagate through mapper/actor/scene")) return 1;

    if (!require_true(fviz_poly_data_create(&direct_poly_data) == FVIZ_OK, "poly data create failed")) return 1;
    if (!require_true(fviz_mapper_set_poly_data(replacement_mapper, direct_poly_data) == FVIZ_OK,
            "mapper direct poly data failed")) return 1;
    before = modified_count;
    if (!require_true(fviz_poly_data_add_point(
            direct_poly_data, fviz_vec3(1.0f, 0.0f, 0.0f), NULL) == FVIZ_OK,
            "poly data modification failed")) return 1;
    if (!require_true(modified_count > before,
            "poly-data ModifiedEvent did not propagate through mapper/actor/scene")) return 1;

    if (!require_true(fviz_mapper_set_input_connection(replacement_mapper, surface_filter) == FVIZ_OK,
            "mapper reconnect failed")) return 1;
    before = modified_count;
    if (!require_true(fviz_poly_data_add_point(
            direct_poly_data, fviz_vec3(2.0f, 0.0f, 0.0f), NULL) == FVIZ_OK,
            "detached poly data modification failed")) return 1;
    if (!require_true(modified_count == before,
            "detached poly-data dependency still propagated ghost ModifiedEvent")) return 1;

    if (!require_true(fviz_cube_source_create(&cube_source) == FVIZ_OK, "cube source create failed")) return 1;
    if (!require_true(fviz_mapper_set_algorithm_connection(
            replacement_mapper, fviz_cube_source_output_port(cube_source)) == FVIZ_OK,
            "mapper cube-source connection failed")) return 1;
    before = modified_count;
    if (!require_true(fviz_cube_source_set_lengths(cube_source, 2.0, 3.0, 4.0) == FVIZ_OK,
            "cube source modification failed")) return 1;
    if (!require_true(modified_count > before,
            "observable source state did not propagate through Algorithm/mapper/actor/scene")) return 1;

    if (!require_true(fviz_transform_create(&transform) == FVIZ_OK, "transform create failed")) return 1;
    if (!require_true(fviz_actor_set_user_transform(actor, transform) == FVIZ_OK, "set transform failed")) return 1;
    before = modified_count;
    fviz_transform_translate(transform, fviz_vec3(1.0f, 2.0f, 3.0f));
    if (!require_true(modified_count > before, "transform ModifiedEvent did not propagate through actor/scene")) return 1;

    if (!require_true(fviz_scene_create(&replacement_scene) == FVIZ_OK, "replacement scene create failed")) return 1;
    if (!require_true(fviz_renderer_set_scene(renderer, replacement_scene) == FVIZ_OK, "replace scene failed")) return 1;
    before = modified_count;
    fviz_actor_set_color(actor, 0.9f, 0.1f, 0.2f);
    if (!require_true(modified_count == before, "detached scene still propagated ghost ModifiedEvent")) return 1;

    if (!require_true(fviz_object_remove_observer((FVizObject*)renderer, renderer_tag) == FVIZ_OK,
            "renderer observer removal failed")) return 1;
    fviz_release(custom_pass);
    fviz_release(text_actor);
    fviz_release(legend);
    fviz_release(extra_light);
    fviz_release(cube_source);
    fviz_release(direct_poly_data);
    fviz_release(surface_filter);
    fviz_release(transform);
    fviz_release(original_mapper);
    fviz_release(replacement_mapper);
    fviz_release(actor);
    fviz_release(replacement_scene);
    fviz_release(renderer);
    return 0;
}
