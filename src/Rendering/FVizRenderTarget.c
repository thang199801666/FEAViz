#include <stddef.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Rendering/FVizRenderTarget.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizRenderTargetPrivate.h>

static void fviz_render_target_destroy(FVizObject* object)
{
    FVIZ_UNUSED(object);
}

static const FVizObjectClass g_fviz_render_target_class = {
    FVIZ_TYPE_RENDER_TARGET,
    "FVizRenderTarget",
    &g_fviz_object_class,
    fviz_render_target_destroy,
    NULL
};

static uint32_t fviz_render_format_bytes(FVizRenderFormat format)
{
    switch (format)
    {
        case FVIZ_RENDER_FORMAT_RGBA16_FLOAT: return 8u;
        case FVIZ_RENDER_FORMAT_RG32_UINT: return 8u;
        case FVIZ_RENDER_FORMAT_RGBA8_UNORM:
        case FVIZ_RENDER_FORMAT_RGBA8_SRGB:
        case FVIZ_RENDER_FORMAT_R32_FLOAT:
        case FVIZ_RENDER_FORMAT_R32_UINT:
        case FVIZ_RENDER_FORMAT_DEPTH24_STENCIL8:
        case FVIZ_RENDER_FORMAT_DEPTH32_FLOAT: return 4u;
        case FVIZ_RENDER_FORMAT_R16_FLOAT: return 2u;
        default: return 0u;
    }
}

static FVizBool fviz_render_attachment_point_is_valid(FVizRenderAttachmentPoint point)
{
    return ((point >= FVIZ_RENDER_ATTACHMENT_COLOR0 && point <= FVIZ_RENDER_ATTACHMENT_COLOR3) ||
        point == FVIZ_RENDER_ATTACHMENT_DEPTH || point == FVIZ_RENDER_ATTACHMENT_DEPTH_STENCIL)
        ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizBool fviz_render_attachment_point_is_depth(FVizRenderAttachmentPoint point)
{
    return point == FVIZ_RENDER_ATTACHMENT_DEPTH ||
        point == FVIZ_RENDER_ATTACHMENT_DEPTH_STENCIL ? FVIZ_TRUE : FVIZ_FALSE;
}

static FVizBool fviz_render_format_is_depth(FVizRenderFormat format)
{
    return format == FVIZ_RENDER_FORMAT_DEPTH24_STENCIL8 ||
        format == FVIZ_RENDER_FORMAT_DEPTH32_FLOAT ? FVIZ_TRUE : FVIZ_FALSE;
}

void fviz_render_attachment_desc_initialize(FVizRenderAttachmentDesc* desc)
{
    if (desc == NULL) return;
    (void)memset(desc, 0, sizeof(*desc));
    desc->struct_size = (uint32_t)sizeof(*desc);
    desc->point = FVIZ_RENDER_ATTACHMENT_COLOR0;
    desc->format = FVIZ_RENDER_FORMAT_RGBA8_UNORM;
    desc->sampled = FVIZ_TRUE;
}

void fviz_render_target_desc_initialize(FVizRenderTargetDesc* desc)
{
    uint32_t i;
    if (desc == NULL) return;
    (void)memset(desc, 0, sizeof(*desc));
    desc->struct_size = (uint32_t)sizeof(*desc);
    desc->width = 1u;
    desc->height = 1u;
    desc->samples = 1u;
    for (i = 0u; i < FVIZ_RENDER_TARGET_MAX_ATTACHMENTS; ++i)
        fviz_render_attachment_desc_initialize(&desc->attachments[i]);
}

FVizResult fviz_render_target_desc_add_attachment(
    FVizRenderTargetDesc* desc,
    FVizRenderAttachmentPoint point,
    FVizRenderFormat format,
    FVizBool sampled)
{
    FVizRenderAttachmentDesc* attachment;
    uint32_t i;
    if (desc == NULL || desc->attachment_count >= FVIZ_RENDER_TARGET_MAX_ATTACHMENTS ||
        fviz_render_attachment_point_is_valid(point) == FVIZ_FALSE ||
        fviz_render_format_bytes(format) == 0u)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    for (i = 0u; i < desc->attachment_count; ++i)
        if (desc->attachments[i].point == point) return FVIZ_ERROR_INVALID_STATE;
    attachment = &desc->attachments[desc->attachment_count++];
    fviz_render_attachment_desc_initialize(attachment);
    attachment->point = point;
    attachment->format = format;
    attachment->sampled = sampled != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    return FVIZ_OK;
}

FVizResult fviz_render_target_desc_validate(const FVizRenderTargetDesc* desc)
{
    uint32_t i;
    uint32_t j;
    uint32_t depth_count = 0u;
    if (desc == NULL || desc->width == 0u || desc->height == 0u ||
        desc->samples == 0u || desc->samples > 32u ||
        desc->attachment_count == 0u ||
        desc->attachment_count > FVIZ_RENDER_TARGET_MAX_ATTACHMENTS)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    for (i = 0u; i < desc->attachment_count; ++i)
    {
        const FVizRenderAttachmentDesc* attachment = &desc->attachments[i];
        const FVizBool point_depth = fviz_render_attachment_point_is_depth(attachment->point);
        const FVizBool format_depth = fviz_render_format_is_depth(attachment->format);
        if (fviz_render_attachment_point_is_valid(attachment->point) == FVIZ_FALSE ||
            fviz_render_format_bytes(attachment->format) == 0u || point_depth != format_depth)
            return FVIZ_ERROR_INVALID_ARGUMENT;
        if (point_depth != FVIZ_FALSE) ++depth_count;
        for (j = i + 1u; j < desc->attachment_count; ++j)
            if (attachment->point == desc->attachments[j].point)
                return FVIZ_ERROR_INVALID_STATE;
    }
    if (depth_count > 1u) return FVIZ_ERROR_INVALID_ARGUMENT;
    return FVIZ_OK;
}

FVizResult fviz_render_target_create(
    const FVizRenderTargetDesc* desc,
    FVizRenderTarget** out_target)
{
    FVizRenderTarget* target;
    FVizResult result;
    if (out_target == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_target = NULL;
    result = fviz_render_target_desc_validate(desc);
    if (result != FVIZ_OK) return result;
    target = (FVizRenderTarget*)fviz_internal_object_allocate(
        sizeof(*target), &g_fviz_render_target_class, NULL);
    if (target == NULL) return fviz_last_error_code();
    target->desc = *desc;
    target->desc.struct_size = (uint32_t)sizeof(target->desc);
    *out_target = target;
    return FVIZ_OK;
}

FVizResult fviz_render_target_resize(
    FVizRenderTarget* target,
    uint32_t width,
    uint32_t height)
{
    if (target == NULL || width == 0u || height == 0u) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (target->desc.width != width || target->desc.height != height)
    {
        target->desc.width = width;
        target->desc.height = height;
        fviz_object_modified((FVizObject*)target);
    }
    return FVIZ_OK;
}

void fviz_render_target_get_desc(
    const FVizRenderTarget* target,
    FVizRenderTargetDesc* out_desc)
{
    if (out_desc == NULL) return;
    fviz_render_target_desc_initialize(out_desc);
    if (target != NULL) *out_desc = target->desc;
}

uint64_t fviz_render_target_estimated_bytes(const FVizRenderTarget* target)
{
    uint64_t bytes_per_pixel = 0u;
    uint64_t pixels;
    uint32_t i;
    if (target == NULL) return 0u;
    for (i = 0u; i < target->desc.attachment_count; ++i)
        bytes_per_pixel += fviz_render_format_bytes(target->desc.attachments[i].format);
    pixels = (uint64_t)target->desc.width * (uint64_t)target->desc.height;
    if (bytes_per_pixel != 0u && pixels > UINT64_MAX / bytes_per_pixel) return UINT64_MAX;
    pixels *= bytes_per_pixel;
    if (target->desc.samples != 0u && pixels > UINT64_MAX / (uint64_t)target->desc.samples)
        return UINT64_MAX;
    return pixels * (uint64_t)target->desc.samples;
}
