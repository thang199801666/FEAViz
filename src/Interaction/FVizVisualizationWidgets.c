#include <FViz/Core/FVizError.h>
#include <FViz/Interaction/FVizVisualizationWidgets.h>
#include <FViz/Rendering/FVizGlyphMapper.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Interaction/FVizVisualizationWidgetsPrivate.h>

static FVizVec3 fviz_widget_transform_point(FVizMat4 matrix, FVizVec3 point)
{
    return fviz_vec3(matrix.m[0] * point.x + matrix.m[4] * point.y + matrix.m[8] * point.z + matrix.m[12],
                     matrix.m[1] * point.x + matrix.m[5] * point.y + matrix.m[9] * point.z + matrix.m[13],
                     matrix.m[2] * point.x + matrix.m[6] * point.y + matrix.m[10] * point.z + matrix.m[14]);
}

static void fviz_selection_highlight_destroy(FVizObject* object)
{
    FVizSelectionHighlight* highlight = (FVizSelectionHighlight*)object;
    if (highlight->renderer != NULL && highlight->actor != NULL)
        (void)fviz_scene_remove_actor(fviz_renderer_scene(highlight->renderer), highlight->actor);
    if (highlight->renderer != NULL && highlight->point_actor != NULL)
        (void)fviz_scene_remove_actor(fviz_renderer_scene(highlight->renderer), highlight->point_actor);
    fviz_release(highlight->point_actor);
    fviz_release(highlight->actor);
    fviz_release(highlight->selection);
    fviz_release(highlight->renderer);
    highlight->point_actor = NULL;
    highlight->actor = NULL;
    highlight->selection = NULL;
    highlight->renderer = NULL;
}

static const FVizObjectClass g_fviz_selection_highlight_class = {FVIZ_TYPE_SELECTION_HIGHLIGHT,
                                                                 "FVizSelectionHighlight", &g_fviz_object_class,
                                                                 fviz_selection_highlight_destroy, NULL};

FVizResult fviz_selection_highlight_create(FVizRenderer* renderer, FVizSelection* selection,
                                           FVizSelectionHighlight** out_highlight)
{
    FVizSelectionHighlight* highlight;
    if (renderer == NULL || selection == NULL || out_highlight == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_highlight = NULL;
    highlight = (FVizSelectionHighlight*)fviz_internal_object_allocate(sizeof(*highlight),
                                                                       &g_fviz_selection_highlight_class, NULL);
    if (highlight == NULL) return fviz_last_error_code();
    highlight->renderer = (FVizRenderer*)fviz_retain(renderer);
    highlight->selection = (FVizSelection*)fviz_retain(selection);
    highlight->enabled = FVIZ_TRUE;
    if (highlight->renderer == NULL || highlight->selection == NULL ||
        fviz_actor_create(&highlight->actor) != FVIZ_OK || fviz_actor_create(&highlight->point_actor) != FVIZ_OK)
    {
        fviz_release(highlight);
        return fviz_last_error_code();
    }
    fviz_actor_set_color(highlight->actor, 1.0f, 0.85f, 0.05f);
    fviz_actor_set_wireframe(highlight->actor, FVIZ_TRUE);
    fviz_actor_set_edge_visibility(highlight->actor, FVIZ_TRUE);
    fviz_actor_set_edge_color(highlight->actor, 1.0f, 0.85f, 0.05f);
    fviz_actor_set_line_width(highlight->actor, 3.0f);
    fviz_actor_set_pickable(highlight->actor, FVIZ_FALSE);
    fviz_actor_set_color(highlight->point_actor, 1.0f, 0.85f, 0.05f);
    fviz_actor_set_point_color(highlight->point_actor, 1.0f, 0.85f, 0.05f);
    fviz_actor_set_point_visibility(highlight->point_actor, FVIZ_TRUE);
    fviz_actor_set_point_shape(highlight->point_actor, FVIZ_POINT_SPHERE_IMPOSTOR);
    fviz_actor_set_point_size(highlight->point_actor, 11.0f);
    fviz_actor_set_pickable(highlight->point_actor, FVIZ_FALSE);
    if (fviz_scene_add_actor(fviz_renderer_scene(renderer), highlight->actor) != FVIZ_OK ||
        fviz_scene_add_actor(fviz_renderer_scene(renderer), highlight->point_actor) != FVIZ_OK ||
        fviz_selection_highlight_update(highlight) != FVIZ_OK)
    {
        fviz_release(highlight);
        return fviz_last_error_code();
    }
    *out_highlight = highlight;
    return FVIZ_OK;
}

FVizResult fviz_selection_highlight_update(FVizSelectionHighlight* highlight)
{
    FVizPolyData* geometry = NULL;
    FVizPolyData* point_geometry = NULL;
    FVizSize i;
    if (highlight == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_selection_refresh(highlight->selection) != FVIZ_OK || fviz_poly_data_create(&geometry) != FVIZ_OK ||
        fviz_poly_data_create(&point_geometry) != FVIZ_OK)
        goto fail;
    for (i = 0u; i < fviz_selection_count(highlight->selection); ++i)
    {
        FVizSelectionRecord record;
        const FVizPolyData* source;
        FVizMat4 model;
        if (fviz_selection_get_record(highlight->selection, i, &record) != FVIZ_OK ||
            record.state != FVIZ_SELECTION_VALID || record.actor == NULL)
            continue;
        model = fviz_actor_transform_matrix(record.actor);
        if (record.association == FVIZ_SELECTION_GLYPH_INSTANCE)
        {
            const FVizGlyphMapper* glyphs = fviz_actor_const_glyph_mapper(record.actor);
            FVizGlyphInstance instance;
            uint32_t id;
            if (glyphs == NULL || record.rendered_id >= fviz_glyph_mapper_instance_count(glyphs) ||
                fviz_glyph_mapper_get_instance(glyphs, record.rendered_id, &instance) != FVIZ_OK)
                continue;
            if (fviz_poly_data_add_point(point_geometry, fviz_widget_transform_point(model, instance.position), &id) !=
                    FVIZ_OK ||
                fviz_poly_data_add_vertex(point_geometry, id) != FVIZ_OK)
                goto fail;
            continue;
        }
        source = fviz_actor_const_poly_data(record.actor);
        if (source == NULL) continue;
        if (record.association == FVIZ_SELECTION_POINT)
        {
            uint32_t id;
            if (record.rendered_id >= fviz_poly_data_point_count(source)) continue;
            if (fviz_poly_data_add_point(
                    point_geometry,
                    fviz_widget_transform_point(model, fviz_poly_data_points(source)[record.rendered_id]),
                    &id) != FVIZ_OK ||
                fviz_poly_data_add_vertex(point_geometry, id) != FVIZ_OK)
                goto fail;
        }
        else if (record.association == FVIZ_SELECTION_EDGE)
        {
            const FVizSize triangle_index = record.rendered_id / 3u;
            const FVizSize local_edge = record.rendered_id % 3u;
            const uint32_t* triangles;
            const FVizVec3* points;
            uint32_t a_out;
            uint32_t b_out;
            if (triangle_index >= fviz_poly_data_triangle_count(source)) continue;
            triangles = fviz_poly_data_triangle_indices(source) + triangle_index * 3u;
            points = fviz_poly_data_points(source);
            if (fviz_poly_data_add_point(geometry, fviz_widget_transform_point(model, points[triangles[local_edge]]),
                                         &a_out) != FVIZ_OK ||
                fviz_poly_data_add_point(geometry,
                                         fviz_widget_transform_point(model, points[triangles[(local_edge + 1u) % 3u]]),
                                         &b_out) != FVIZ_OK ||
                fviz_poly_data_add_line(geometry, a_out, b_out) != FVIZ_OK)
                goto fail;
        }
        else if (record.association == FVIZ_SELECTION_CELL)
        {
            const FVizVec3* points;
            const uint32_t* triangle;
            uint32_t output_ids[3];
            FVizSize corner;
            if (record.rendered_id >= fviz_poly_data_triangle_count(source)) continue;
            points = fviz_poly_data_points(source);
            triangle = fviz_poly_data_triangle_indices(source) + record.rendered_id * 3u;
            for (corner = 0u; corner < 3u; ++corner)
                if (fviz_poly_data_add_point(geometry, fviz_widget_transform_point(model, points[triangle[corner]]),
                                             &output_ids[corner]) != FVIZ_OK)
                    goto fail;
            if (fviz_poly_data_add_triangle(geometry, output_ids[0], output_ids[1], output_ids[2]) != FVIZ_OK)
                goto fail;
        }
        else if (record.association == FVIZ_SELECTION_ACTOR)
        {
            const FVizBounds bounds = fviz_actor_bounds(record.actor);
            FVizVec3 corners[8];
            uint32_t ids[8];
            static const uint8_t edges[12][2] = {{0, 1}, {1, 2}, {2, 3}, {3, 0}, {4, 5}, {5, 6},
                                                 {6, 7}, {7, 4}, {0, 4}, {1, 5}, {2, 6}, {3, 7}};
            FVizSize corner;
            FVizSize edge;
            if (bounds.valid == FVIZ_FALSE) continue;
            /* actor_bounds is already world-space; do not apply model again. */
            corners[0] = fviz_vec3(bounds.min.x, bounds.min.y, bounds.min.z);
            corners[1] = fviz_vec3(bounds.max.x, bounds.min.y, bounds.min.z);
            corners[2] = fviz_vec3(bounds.max.x, bounds.max.y, bounds.min.z);
            corners[3] = fviz_vec3(bounds.min.x, bounds.max.y, bounds.min.z);
            corners[4] = fviz_vec3(bounds.min.x, bounds.min.y, bounds.max.z);
            corners[5] = fviz_vec3(bounds.max.x, bounds.min.y, bounds.max.z);
            corners[6] = fviz_vec3(bounds.max.x, bounds.max.y, bounds.max.z);
            corners[7] = fviz_vec3(bounds.min.x, bounds.max.y, bounds.max.z);
            for (corner = 0u; corner < 8u; ++corner)
                if (fviz_poly_data_add_point(geometry, corners[corner], &ids[corner]) != FVIZ_OK) goto fail;
            for (edge = 0u; edge < 12u; ++edge)
                if (fviz_poly_data_add_line(geometry, ids[edges[edge][0]], ids[edges[edge][1]]) != FVIZ_OK) goto fail;
        }
    }
    if (fviz_actor_set_poly_data(highlight->actor, geometry) != FVIZ_OK ||
        fviz_actor_set_poly_data(highlight->point_actor, point_geometry) != FVIZ_OK)
        goto fail;
    fviz_actor_set_visible(highlight->actor,
                           highlight->enabled != FVIZ_FALSE && (fviz_poly_data_triangle_count(geometry) != 0u ||
                                                                fviz_poly_data_line_count(geometry) != 0u));
    fviz_actor_set_visible(highlight->point_actor,
                           highlight->enabled != FVIZ_FALSE && fviz_poly_data_point_count(point_geometry) != 0u);
    fviz_release(point_geometry);
    fviz_release(geometry);
    return FVIZ_OK;
fail:
    fviz_release(point_geometry);
    fviz_release(geometry);
    return fviz_last_error_code();
}

void fviz_selection_highlight_set_color(FVizSelectionHighlight* highlight, float red, float green, float blue)
{
    if (highlight == NULL) return;
    fviz_actor_set_color(highlight->actor, red, green, blue);
    fviz_actor_set_edge_color(highlight->actor, red, green, blue);
    fviz_actor_set_color(highlight->point_actor, red, green, blue);
    fviz_actor_set_point_color(highlight->point_actor, red, green, blue);
}

void fviz_selection_highlight_set_enabled(FVizSelectionHighlight* highlight, FVizBool enabled)
{
    if (highlight == NULL) return;
    highlight->enabled = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    if (highlight->enabled == FVIZ_FALSE)
    {
        fviz_actor_set_visible(highlight->actor, FVIZ_FALSE);
        fviz_actor_set_visible(highlight->point_actor, FVIZ_FALSE);
    }
    else
        (void)fviz_selection_highlight_update(highlight);
}

FVizBool fviz_selection_highlight_enabled(const FVizSelectionHighlight* highlight)
{
    return highlight != NULL ? highlight->enabled : FVIZ_FALSE;
}

FVizActor* fviz_selection_highlight_actor(FVizSelectionHighlight* highlight)
{
    return highlight != NULL ? highlight->actor : NULL;
}

FVizActor* fviz_selection_highlight_point_actor(FVizSelectionHighlight* highlight)
{
    return highlight != NULL ? highlight->point_actor : NULL;
}

static void fviz_orientation_axes_widget_destroy(FVizObject* object)
{
    FVizOrientationAxesWidget* widget = (FVizOrientationAxesWidget*)object;
    if (widget->window != NULL && widget->overlay_renderer != NULL)
        (void)fviz_render_window_remove_renderer(widget->window, widget->overlay_renderer);
    fviz_release(widget->actor);
    fviz_release(widget->overlay_renderer);
    fviz_release(widget->target_renderer);
    fviz_release(widget->window);
}

static const FVizObjectClass g_fviz_orientation_axes_widget_class = {FVIZ_TYPE_ORIENTATION_AXES_WIDGET,
                                                                     "FVizOrientationAxesWidget", &g_fviz_object_class,
                                                                     fviz_orientation_axes_widget_destroy, NULL};

static FVizResult fviz_orientation_axes_geometry(FVizPolyData** out_geometry)
{
    static const FVizVec3 points[6] = {{0, 0, 0}, {1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {0, 0, 0}, {0, 0, 1}};
    static const uint8_t colors[6][3] = {{255, 32, 32}, {255, 32, 32}, {32, 255, 32},
                                         {32, 255, 32}, {48, 96, 255}, {48, 96, 255}};
    FVizPolyData* geometry = NULL;
    FVizDataArray* color_array = NULL;
    FVizSize i;
    if (fviz_poly_data_create(&geometry) != FVIZ_OK ||
        fviz_data_array_create(FVIZ_DATA_UINT8, 3u, &color_array) != FVIZ_OK)
        goto fail;
    for (i = 0u; i < 6u; ++i)
    {
        uint32_t id;
        if (fviz_poly_data_add_point(geometry, points[i], &id) != FVIZ_OK ||
            fviz_data_array_append_tuple(color_array, colors[i]) != FVIZ_OK)
            goto fail;
    }
    if (fviz_poly_data_add_line(geometry, 0u, 1u) != FVIZ_OK || fviz_poly_data_add_line(geometry, 2u, 3u) != FVIZ_OK ||
        fviz_poly_data_add_line(geometry, 4u, 5u) != FVIZ_OK ||
        fviz_attribute_set_add(fviz_poly_data_point_data(geometry), "FVizAxesColors", color_array) != FVIZ_OK)
        goto fail;
    fviz_release(color_array);
    *out_geometry = geometry;
    return FVIZ_OK;
fail:
    fviz_release(color_array);
    fviz_release(geometry);
    return fviz_last_error_code();
}

FVizResult fviz_orientation_axes_widget_create(FVizRenderWindow* window, FVizRenderer* target_renderer,
                                               FVizOrientationAxesWidget** out_widget)
{
    FVizOrientationAxesWidget* widget;
    FVizPolyData* geometry = NULL;
    FVizArraySelection selection;
    if (window == NULL || target_renderer == NULL || out_widget == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_widget = NULL;
    widget = (FVizOrientationAxesWidget*)fviz_internal_object_allocate(sizeof(*widget),
                                                                       &g_fviz_orientation_axes_widget_class, NULL);
    if (widget == NULL) return fviz_last_error_code();
    widget->window = (FVizRenderWindow*)fviz_retain(window);
    widget->target_renderer = (FVizRenderer*)fviz_retain(target_renderer);
    widget->enabled = FVIZ_TRUE;
    if (widget->window == NULL || widget->target_renderer == NULL ||
        fviz_renderer_create(&widget->overlay_renderer) != FVIZ_OK || fviz_actor_create(&widget->actor) != FVIZ_OK ||
        fviz_orientation_axes_geometry(&geometry) != FVIZ_OK)
        goto fail;
    if (fviz_actor_set_poly_data(widget->actor, geometry) != FVIZ_OK) goto fail;
    fviz_actor_set_line_width(widget->actor, 3.0f);
    fviz_array_selection_initialize(&selection);
    selection.name = "FVizAxesColors";
    selection.association = FVIZ_ASSOCIATION_POINTS;
    selection.component_mode = FVIZ_COMPONENT_COLOR;
    if (fviz_mapper_set_array_selection(fviz_actor_mapper(widget->actor), &selection) != FVIZ_OK) goto fail;
    fviz_mapper_set_scalar_visibility(fviz_actor_mapper(widget->actor), FVIZ_TRUE);
    fviz_renderer_set_layer(widget->overlay_renderer, fviz_renderer_layer(target_renderer) + 1);
    fviz_renderer_set_interactive(widget->overlay_renderer, FVIZ_FALSE);
    if (fviz_scene_add_actor(fviz_renderer_scene(widget->overlay_renderer), widget->actor) != FVIZ_OK ||
        fviz_render_window_add_renderer(window, widget->overlay_renderer) != FVIZ_OK ||
        fviz_orientation_axes_widget_update(widget) != FVIZ_OK)
        goto fail;
    fviz_release(geometry);
    *out_widget = widget;
    return FVIZ_OK;
fail:
    fviz_release(geometry);
    fviz_release(widget);
    return fviz_last_error_code();
}

FVizResult fviz_orientation_axes_widget_update(FVizOrientationAxesWidget* widget)
{
    float x0, y0, x1, y1;
    FVizCamera* target_camera;
    FVizCamera* axes_camera;
    FVizVec3 direction;
    if (widget == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    fviz_renderer_get_viewport(widget->target_renderer, &x0, &y0, &x1, &y1);
    x1 = x0 + (x1 - x0) * 0.22f;
    y1 = y0 + (y1 - y0) * 0.22f;
    if (fviz_renderer_set_viewport(widget->overlay_renderer, x0, y0, x1, y1) != FVIZ_OK) return fviz_last_error_code();
    target_camera = fviz_renderer_camera(widget->target_renderer);
    axes_camera = fviz_renderer_camera(widget->overlay_renderer);
    direction =
        fviz_vec3_normalize(fviz_vec3_sub(fviz_camera_position(target_camera), fviz_camera_target(target_camera)));
    fviz_camera_set_target(axes_camera, fviz_vec3(0.0f, 0.0f, 0.0f));
    fviz_camera_set_position(axes_camera, fviz_vec3_scale(direction, 4.0f));
    fviz_camera_set_up(axes_camera, fviz_camera_up(target_camera));
    fviz_renderer_fit_camera(widget->overlay_renderer, 1.4f);
    return FVIZ_OK;
}

void fviz_orientation_axes_widget_set_enabled(FVizOrientationAxesWidget* widget, FVizBool enabled)
{
    if (widget == NULL) return;
    widget->enabled = enabled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    fviz_actor_set_visible(widget->actor, widget->enabled);
}

FVizBool fviz_orientation_axes_widget_enabled(const FVizOrientationAxesWidget* widget)
{
    return widget != NULL ? widget->enabled : FVIZ_FALSE;
}

FVizRenderer* fviz_orientation_axes_widget_renderer(FVizOrientationAxesWidget* widget)
{
    return widget != NULL ? widget->overlay_renderer : NULL;
}
