#ifndef FVIZ_RENDERING_RENDER_TARGET_H
#define FVIZ_RENDERING_RENDER_TARGET_H

#include <FViz/Core/FVizApi.h>
#include <FViz/Core/FVizObject.h>
#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>

FVIZ_EXTERN_C_BEGIN

typedef struct FVizRenderTarget FVizRenderTarget;
#define FVIZ_TYPE_RENDER_TARGET UINT64_C(0xA1E132EC69F42A91)

#define FVIZ_RENDER_TARGET_MAX_ATTACHMENTS 8u

typedef enum FVizRenderAttachmentPoint
{
    FVIZ_RENDER_ATTACHMENT_COLOR0 = 0,
    FVIZ_RENDER_ATTACHMENT_COLOR1 = 1,
    FVIZ_RENDER_ATTACHMENT_COLOR2 = 2,
    FVIZ_RENDER_ATTACHMENT_COLOR3 = 3,
    FVIZ_RENDER_ATTACHMENT_DEPTH = 16,
    FVIZ_RENDER_ATTACHMENT_DEPTH_STENCIL = 17
} FVizRenderAttachmentPoint;

typedef enum FVizRenderFormat
{
    FVIZ_RENDER_FORMAT_RGBA8_UNORM = 0,
    FVIZ_RENDER_FORMAT_RGBA8_SRGB = 1,
    FVIZ_RENDER_FORMAT_RGBA16_FLOAT = 2,
    FVIZ_RENDER_FORMAT_R16_FLOAT = 3,
    FVIZ_RENDER_FORMAT_R32_FLOAT = 4,
    FVIZ_RENDER_FORMAT_R32_UINT = 5,
    FVIZ_RENDER_FORMAT_RG32_UINT = 6,
    FVIZ_RENDER_FORMAT_DEPTH24_STENCIL8 = 7,
    FVIZ_RENDER_FORMAT_DEPTH32_FLOAT = 8
} FVizRenderFormat;

typedef struct FVizRenderAttachmentDesc
{
    uint32_t struct_size;
    FVizRenderAttachmentPoint point;
    FVizRenderFormat format;
    FVizBool sampled;
} FVizRenderAttachmentDesc;

typedef struct FVizRenderTargetDesc
{
    uint32_t struct_size;
    uint32_t width;
    uint32_t height;
    uint32_t samples;
    uint32_t attachment_count;
    FVizRenderAttachmentDesc attachments[FVIZ_RENDER_TARGET_MAX_ATTACHMENTS];
} FVizRenderTargetDesc;

FVIZ_API void fviz_render_attachment_desc_initialize(FVizRenderAttachmentDesc* desc);
FVIZ_API void fviz_render_target_desc_initialize(FVizRenderTargetDesc* desc);
FVIZ_API FVizResult fviz_render_target_desc_add_attachment(FVizRenderTargetDesc* desc, FVizRenderAttachmentPoint point,
                                                           FVizRenderFormat format, FVizBool sampled);
FVIZ_API FVizResult fviz_render_target_desc_validate(const FVizRenderTargetDesc* desc);
FVIZ_API FVizResult fviz_render_target_create(const FVizRenderTargetDesc* desc, FVizRenderTarget** out_target);
FVIZ_API FVizResult fviz_render_target_resize(FVizRenderTarget* target, uint32_t width, uint32_t height);
FVIZ_API void fviz_render_target_get_desc(const FVizRenderTarget* target, FVizRenderTargetDesc* out_desc);
FVIZ_API uint64_t fviz_render_target_estimated_bytes(const FVizRenderTarget* target);

FVIZ_EXTERN_C_END

#endif /* FVIZ_RENDERING_RENDER_TARGET_H */
