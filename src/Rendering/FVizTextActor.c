#include <math.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Rendering/FVizTextActor.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizTextActorPrivate.h>
#include <FViz/Rendering/FVizTextLayoutPrivate.h>

static void fviz_text_actor_2d_destroy(FVizObject* object);
static void fviz_billboard_text_actor_3d_destroy(FVizObject* object);

static FVizBool fviz_text_actor_property_modified(FVizObject* caller, FVizEventId event_id, void* call_data,
                                                  void* client_data)
{
    FVizObject* actor = (FVizObject*)client_data;
    FVIZ_UNUSED(caller);
    FVIZ_UNUSED(event_id);
    FVIZ_UNUSED(call_data);
    if (actor != NULL) fviz_object_modified(actor);
    return FVIZ_FALSE;
}

static FVizResult fviz_text_actor_observe_property(FVizObject* actor, FVizTextProperty* property,
                                                   FVizObserverTag* out_tag)
{
    if (actor == NULL || property == NULL || out_tag == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_tag = FVIZ_OBSERVER_TAG_INVALID;
    return fviz_object_add_observer((FVizObject*)property, FVIZ_EVENT_MODIFIED, 0.0f, fviz_text_actor_property_modified,
                                    actor, out_tag);
}

static const FVizObjectClass g_fviz_text_actor_2d_class = {FVIZ_TYPE_TEXT_ACTOR_2D, "FVizTextActor2D",
                                                           &g_fviz_object_class, fviz_text_actor_2d_destroy, NULL};
static const FVizObjectClass g_fviz_billboard_text_actor_3d_class = {FVIZ_TYPE_BILLBOARD_TEXT_ACTOR_3D,
                                                                     "FVizBillboardTextActor3D", &g_fviz_object_class,
                                                                     fviz_billboard_text_actor_3d_destroy, NULL};

static const char* fviz_utf8_next(const char* s, uint32_t* out_cp)
{
    const unsigned char* p = (const unsigned char*)s;
    uint32_t cp;
    if (p == NULL || *p == 0u)
    {
        if (out_cp) *out_cp = 0u;
        return s;
    }
    if (*p < 0x80u)
    {
        if (out_cp) *out_cp = *p;
        return s + 1;
    }
    if ((*p & 0xE0u) == 0xC0u && (p[1] & 0xC0u) == 0x80u)
    {
        cp = ((uint32_t)(p[0] & 0x1Fu) << 6u) | (uint32_t)(p[1] & 0x3Fu);
        if (cp >= 0x80u)
        {
            if (out_cp) *out_cp = cp;
            return s + 2;
        }
    }
    else if ((*p & 0xF0u) == 0xE0u && (p[1] & 0xC0u) == 0x80u && (p[2] & 0xC0u) == 0x80u)
    {
        cp = ((uint32_t)(p[0] & 0x0Fu) << 12u) | ((uint32_t)(p[1] & 0x3Fu) << 6u) | (uint32_t)(p[2] & 0x3Fu);
        if (cp >= 0x800u && !(cp >= 0xD800u && cp <= 0xDFFFu))
        {
            if (out_cp) *out_cp = cp;
            return s + 3;
        }
    }
    else if ((*p & 0xF8u) == 0xF0u && (p[1] & 0xC0u) == 0x80u && (p[2] & 0xC0u) == 0x80u && (p[3] & 0xC0u) == 0x80u)
    {
        cp = ((uint32_t)(p[0] & 7u) << 18u) | ((uint32_t)(p[1] & 0x3Fu) << 12u) | ((uint32_t)(p[2] & 0x3Fu) << 6u) |
             (uint32_t)(p[3] & 0x3Fu);
        if (cp >= 0x10000u && cp <= 0x10FFFFu)
        {
            if (out_cp) *out_cp = cp;
            return s + 4;
        }
    }
    if (out_cp) *out_cp = '?';
    return s + 1;
}

static FVizResult fviz_text_measure_raw(const FVizTextProperty* property, const char* utf8,
                                        FVizTextMetrics* out_metrics)
{
    const FVizFont* font;
    const FVizFontAtlas* atlas;
    const char* cursor;
    float scale;
    float line_width = 0.0f;
    float max_width = 0.0f;
    FVizSize glyph_count = 0u;
    FVizSize line_count = 1u;
    if (property == NULL || utf8 == NULL || out_metrics == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "property, utf8 and out_metrics must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    font = fviz_text_property_const_font(property);
    atlas = fviz_font_const_atlas(font);
    if (atlas == NULL || fviz_font_atlas_nominal_pixel_size(atlas) <= 0.0f) return FVIZ_ERROR_INVALID_STATE;
    scale = fviz_text_property_font_size(property) / fviz_font_atlas_nominal_pixel_size(atlas);
    cursor = utf8;
    while (*cursor != '\0')
    {
        uint32_t cp;
        const FVizFontGlyph* glyph;
        cursor = fviz_utf8_next(cursor, &cp);
        if (cp == '\r') continue;
        if (cp == '\n')
        {
            if (line_width > max_width) max_width = line_width;
            line_width = 0.0f;
            ++line_count;
            continue;
        }
        if (cp == '\t')
        {
            glyph = fviz_font_atlas_find_glyph(atlas, ' ');
            line_width +=
                glyph != NULL ? glyph->advance_x * scale * 4.0f : fviz_text_property_font_size(property) * 2.0f;
            continue;
        }
        glyph = fviz_font_atlas_find_glyph(atlas, cp);
        if (glyph != NULL)
        {
            line_width += glyph->advance_x * scale;
            ++glyph_count;
        }
    }
    if (line_width > max_width) max_width = line_width;
    out_metrics->width = max_width;
    out_metrics->line_height = fviz_text_property_font_size(property) * fviz_text_property_line_spacing(property);
    out_metrics->height = out_metrics->line_height * (float)line_count;
    out_metrics->glyph_count = glyph_count;
    out_metrics->line_count = line_count;
    return FVIZ_OK;
}

FVizResult fviz_internal_text_layout_visit(const FVizTextProperty* property, const char* utf8,
                                           FVizTextGlyphVisitor visitor, void* user_data, FVizTextMetrics* out_metrics)
{
    FVizTextMetrics metrics;
    const FVizFontAtlas* atlas;
    const char* cursor;
    float scale;
    float pen_x = 0.0f;
    float pen_y;
    float align_x = 0.0f;
    float align_y = 0.0f;
    if (fviz_text_measure_raw(property, utf8, &metrics) != FVIZ_OK) return fviz_last_error_code();
    if (out_metrics != NULL) *out_metrics = metrics;
    if (visitor == NULL) return FVIZ_OK;
    atlas = fviz_font_const_atlas(fviz_text_property_const_font(property));
    scale = fviz_text_property_font_size(property) / fviz_font_atlas_nominal_pixel_size(atlas);
    if (fviz_text_property_horizontal_alignment(property) == FVIZ_TEXT_ALIGN_CENTER) align_x = -0.5f * metrics.width;
    else if (fviz_text_property_horizontal_alignment(property) == FVIZ_TEXT_ALIGN_RIGHT)
        align_x = -metrics.width;
    if (fviz_text_property_vertical_alignment(property) == FVIZ_TEXT_ALIGN_MIDDLE) align_y = -0.5f * metrics.height;
    else if (fviz_text_property_vertical_alignment(property) == FVIZ_TEXT_ALIGN_TOP)
        align_y = -metrics.height;
    pen_y = align_y + metrics.height - metrics.line_height;
    cursor = utf8;
    while (*cursor != '\0')
    {
        uint32_t cp;
        const FVizFontGlyph* glyph;
        cursor = fviz_utf8_next(cursor, &cp);
        if (cp == '\r') continue;
        if (cp == '\n')
        {
            pen_x = 0.0f;
            pen_y -= metrics.line_height;
            continue;
        }
        if (cp == '\t')
        {
            glyph = fviz_font_atlas_find_glyph(atlas, ' ');
            pen_x += glyph != NULL ? glyph->advance_x * scale * 4.0f : fviz_text_property_font_size(property) * 2.0f;
            continue;
        }
        glyph = fviz_font_atlas_find_glyph(atlas, cp);
        if (glyph != NULL)
        {
            const float x0 = align_x + pen_x + glyph->bearing_x * scale;
            const float y0 = pen_y;
            const float x1 = x0 + (float)glyph->width * scale;
            const float y1 = y0 + (float)glyph->height * scale;
            FVizResult result = visitor(glyph, cp, x0, y0, x1, y1, user_data);
            if (result != FVIZ_OK) return result;
            pen_x += glyph->advance_x * scale;
        }
    }
    return FVIZ_OK;
}

FVizResult fviz_text_measure_utf8(const FVizTextProperty* property, const char* utf8, FVizTextMetrics* out_metrics)
{
    return fviz_text_measure_raw(property, utf8, out_metrics);
}

static void fviz_text_actor_2d_destroy(FVizObject* object)
{
    FVizTextActor2D* actor = (FVizTextActor2D*)object;
    if (actor->property != NULL && actor->property_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)actor->property, actor->property_modified_tag);
    fviz_release(actor->text);
    fviz_release(actor->property);
    actor->text = NULL;
    actor->property = NULL;
    actor->property_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
}

FVizResult fviz_text_actor_2d_create(FVizTextActor2D** out_actor)
{
    FVizTextActor2D* actor;
    if (out_actor == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_actor = NULL;
    actor = (FVizTextActor2D*)fviz_internal_object_allocate(sizeof(*actor), &g_fviz_text_actor_2d_class, NULL);
    if (actor == NULL) return fviz_last_error_code();
    if (fviz_string_create_from("", &actor->text) != FVIZ_OK || fviz_text_property_create(&actor->property) != FVIZ_OK)
    {
        fviz_release(actor);
        return fviz_last_error_code();
    }
    actor->property_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (fviz_text_actor_observe_property((FVizObject*)actor, actor->property, &actor->property_modified_tag) != FVIZ_OK)
    {
        fviz_release(actor);
        return fviz_last_error_code();
    }
    actor->position[0] = 0.0f;
    actor->position[1] = 0.0f;
    actor->coordinate_system = FVIZ_TEXT_COORDINATE_DISPLAY_PIXELS;
    actor->visible = FVIZ_TRUE;
    *out_actor = actor;
    return FVIZ_OK;
}

FVizResult fviz_text_actor_2d_set_text(FVizTextActor2D* a, const char* t)
{
    if (a == NULL || t == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (strcmp(fviz_string_c_str(a->text), t) == 0) return FVIZ_OK;
    if (fviz_string_set(a->text, t) != FVIZ_OK) return fviz_last_error_code();
    fviz_object_modified((FVizObject*)a);
    return FVIZ_OK;
}

const char* fviz_text_actor_2d_text(const FVizTextActor2D* a)
{
    return a != NULL ? fviz_string_c_str(a->text) : "";
}

FVizResult fviz_text_actor_2d_set_text_property(FVizTextActor2D* a, FVizTextProperty* p)
{
    FVizObserverTag tag = FVIZ_OBSERVER_TAG_INVALID;
    if (a == NULL || p == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (a->property == p) return FVIZ_OK;
    if (fviz_retain(p) == NULL) return fviz_last_error_code();
    if (fviz_text_actor_observe_property((FVizObject*)a, p, &tag) != FVIZ_OK)
    {
        fviz_release(p);
        return fviz_last_error_code();
    }
    if (a->property != NULL && a->property_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)a->property, a->property_modified_tag);
    fviz_release(a->property);
    a->property = p;
    a->property_modified_tag = tag;
    fviz_object_modified((FVizObject*)a);
    return FVIZ_OK;
}

FVizTextProperty* fviz_text_actor_2d_text_property(FVizTextActor2D* a)
{
    return a != NULL ? a->property : NULL;
}

const FVizTextProperty* fviz_text_actor_2d_const_text_property(const FVizTextActor2D* a)
{
    return a != NULL ? a->property : NULL;
}

void fviz_text_actor_2d_set_position(FVizTextActor2D* a, float x, float y)
{
    if (a == NULL || !isfinite(x) || !isfinite(y)) return;
    if (a->position[0] != x || a->position[1] != y)
    {
        a->position[0] = x;
        a->position[1] = y;
        fviz_object_modified((FVizObject*)a);
    }
}

void fviz_text_actor_2d_get_position(const FVizTextActor2D* a, float* x, float* y)
{
    if (a == NULL) return;
    if (x) *x = a->position[0];
    if (y) *y = a->position[1];
}

void fviz_text_actor_2d_set_coordinate_system(FVizTextActor2D* a, FVizTextCoordinateSystem s)
{
    if (a == NULL || (s != FVIZ_TEXT_COORDINATE_DISPLAY_PIXELS && s != FVIZ_TEXT_COORDINATE_NORMALIZED_VIEWPORT))
        return;
    if (a->coordinate_system != s)
    {
        a->coordinate_system = s;
        fviz_object_modified((FVizObject*)a);
    }
}

FVizTextCoordinateSystem fviz_text_actor_2d_coordinate_system(const FVizTextActor2D* a)
{
    return a != NULL ? a->coordinate_system : FVIZ_TEXT_COORDINATE_DISPLAY_PIXELS;
}

void fviz_text_actor_2d_set_visible(FVizTextActor2D* a, FVizBool v)
{
    if (a == NULL) return;
    v = v != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    if (a->visible != v)
    {
        a->visible = v;
        fviz_object_modified((FVizObject*)a);
    }
}

FVizBool fviz_text_actor_2d_is_visible(const FVizTextActor2D* a)
{
    return a != NULL ? a->visible : FVIZ_FALSE;
}

FVizResult fviz_text_actor_2d_measure(const FVizTextActor2D* a, FVizTextMetrics* m)
{
    return a != NULL ? fviz_text_measure_utf8(a->property, fviz_string_c_str(a->text), m) : FVIZ_ERROR_INVALID_ARGUMENT;
}

static void fviz_billboard_text_actor_3d_destroy(FVizObject* object)
{
    FVizBillboardTextActor3D* actor = (FVizBillboardTextActor3D*)object;
    if (actor->property != NULL && actor->property_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)actor->property, actor->property_modified_tag);
    fviz_release(actor->text);
    fviz_release(actor->property);
    actor->text = NULL;
    actor->property = NULL;
    actor->property_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
}

FVizResult fviz_billboard_text_actor_3d_create(FVizBillboardTextActor3D** out_actor)
{
    FVizBillboardTextActor3D* a;
    if (out_actor == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_actor = NULL;
    a = (FVizBillboardTextActor3D*)fviz_internal_object_allocate(sizeof(*a), &g_fviz_billboard_text_actor_3d_class,
                                                                 NULL);
    if (a == NULL) return fviz_last_error_code();
    if (fviz_string_create_from("", &a->text) != FVIZ_OK || fviz_text_property_create(&a->property) != FVIZ_OK)
    {
        fviz_release(a);
        return fviz_last_error_code();
    }
    a->property_modified_tag = FVIZ_OBSERVER_TAG_INVALID;
    if (fviz_text_actor_observe_property((FVizObject*)a, a->property, &a->property_modified_tag) != FVIZ_OK)
    {
        fviz_release(a);
        return fviz_last_error_code();
    }
    a->world_position = fviz_vec3(0, 0, 0);
    a->pixel_offset[0] = 0;
    a->pixel_offset[1] = 0;
    a->depth_test = FVIZ_TRUE;
    a->visible = FVIZ_TRUE;
    *out_actor = a;
    return FVIZ_OK;
}

FVizResult fviz_billboard_text_actor_3d_set_text(FVizBillboardTextActor3D* a, const char* t)
{
    if (a == NULL || t == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (strcmp(fviz_string_c_str(a->text), t) == 0) return FVIZ_OK;
    if (fviz_string_set(a->text, t) != FVIZ_OK) return fviz_last_error_code();
    fviz_object_modified((FVizObject*)a);
    return FVIZ_OK;
}

const char* fviz_billboard_text_actor_3d_text(const FVizBillboardTextActor3D* a)
{
    return a != NULL ? fviz_string_c_str(a->text) : "";
}

FVizResult fviz_billboard_text_actor_3d_set_text_property(FVizBillboardTextActor3D* a, FVizTextProperty* p)
{
    FVizObserverTag tag = FVIZ_OBSERVER_TAG_INVALID;
    if (a == NULL || p == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (a->property == p) return FVIZ_OK;
    if (fviz_retain(p) == NULL) return fviz_last_error_code();
    if (fviz_text_actor_observe_property((FVizObject*)a, p, &tag) != FVIZ_OK)
    {
        fviz_release(p);
        return fviz_last_error_code();
    }
    if (a->property != NULL && a->property_modified_tag != FVIZ_OBSERVER_TAG_INVALID)
        (void)fviz_object_remove_observer((FVizObject*)a->property, a->property_modified_tag);
    fviz_release(a->property);
    a->property = p;
    a->property_modified_tag = tag;
    fviz_object_modified((FVizObject*)a);
    return FVIZ_OK;
}

FVizTextProperty* fviz_billboard_text_actor_3d_text_property(FVizBillboardTextActor3D* a)
{
    return a != NULL ? a->property : NULL;
}

const FVizTextProperty* fviz_billboard_text_actor_3d_const_text_property(const FVizBillboardTextActor3D* a)
{
    return a != NULL ? a->property : NULL;
}

void fviz_billboard_text_actor_3d_set_world_position(FVizBillboardTextActor3D* a, FVizVec3 p)
{
    if (a == NULL) return;
    if (a->world_position.x != p.x || a->world_position.y != p.y || a->world_position.z != p.z)
    {
        a->world_position = p;
        fviz_object_modified((FVizObject*)a);
    }
}

FVizVec3 fviz_billboard_text_actor_3d_world_position(const FVizBillboardTextActor3D* a)
{
    return a != NULL ? a->world_position : fviz_vec3(0, 0, 0);
}

void fviz_billboard_text_actor_3d_set_pixel_offset(FVizBillboardTextActor3D* a, float x, float y)
{
    if (a == NULL || !isfinite(x) || !isfinite(y)) return;
    if (a->pixel_offset[0] != x || a->pixel_offset[1] != y)
    {
        a->pixel_offset[0] = x;
        a->pixel_offset[1] = y;
        fviz_object_modified((FVizObject*)a);
    }
}

void fviz_billboard_text_actor_3d_get_pixel_offset(const FVizBillboardTextActor3D* a, float* x, float* y)
{
    if (a == NULL) return;
    if (x) *x = a->pixel_offset[0];
    if (y) *y = a->pixel_offset[1];
}

void fviz_billboard_text_actor_3d_set_depth_test(FVizBillboardTextActor3D* a, FVizBool e)
{
    if (a == NULL) return;
    e = e != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    if (a->depth_test != e)
    {
        a->depth_test = e;
        fviz_object_modified((FVizObject*)a);
    }
}

FVizBool fviz_billboard_text_actor_3d_depth_test(const FVizBillboardTextActor3D* a)
{
    return a != NULL ? a->depth_test : FVIZ_FALSE;
}

void fviz_billboard_text_actor_3d_set_visible(FVizBillboardTextActor3D* a, FVizBool v)
{
    if (a == NULL) return;
    v = v != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    if (a->visible != v)
    {
        a->visible = v;
        fviz_object_modified((FVizObject*)a);
    }
}

FVizBool fviz_billboard_text_actor_3d_is_visible(const FVizBillboardTextActor3D* a)
{
    return a != NULL ? a->visible : FVIZ_FALSE;
}
