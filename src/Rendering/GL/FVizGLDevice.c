#include <limits.h>
#include <stddef.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Data/FVizDataType.h>
#include <FViz/Math/FVizMat3.h>
#include <FViz/Math/FVizMat4.h>
#include <FViz/Math/FVizBounds.h>
#include <FViz/Rendering/FVizActor.h>
#include <FViz/Rendering/FVizCamera.h>
#include <FViz/Rendering/FVizLookupTable.h>
#include <FViz/Rendering/FVizLight.h>
#include <FViz/Rendering/FVizGlyphMapper.h>
#include <FViz/Rendering/FVizVolumeMapper.h>
#include <FViz/Rendering/FVizMapper.h>
#include <FViz/Rendering/FVizOverlayLayout.h>
#include <FViz/Rendering/FVizRenderer.h>
#include <FViz/Rendering/FVizScalarLegend.h>
#include <FViz/Rendering/FVizTextActor.h>
#include <FViz/Rendering/FVizTextProperty.h>
#include <FViz/Rendering/FVizFontAtlas.h>
#include <FViz/Rendering/FVizScene.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Rendering/FVizGL.h>
#include <FViz/Rendering/FVizGLDevice.h>
#include <FViz/Rendering/FVizMapperPrivate.h>
#include <FViz/Rendering/FVizGlyphMapperPrivate.h>
#include <FViz/Rendering/FVizTextLayoutPrivate.h>
#include <FViz/Rendering/FVizLabelSetPrivate.h>

static FVizResult fviz_gl_render_volume(FVizGLDevice* device, FVizRenderer* renderer, const FVizActor* actor,
                                        float aspect_ratio);

#define FVIZ_GL_POSITION_ATTRIBUTE_INDEX 0u
#define FVIZ_GL_NORMAL_ATTRIBUTE_INDEX 1u
#define FVIZ_GL_COLOR_ATTRIBUTE_INDEX 2u
#define FVIZ_GL_INSTANCE_MATRIX0_ATTRIBUTE_INDEX 3u
#define FVIZ_GL_INSTANCE_MATRIX1_ATTRIBUTE_INDEX 4u
#define FVIZ_GL_INSTANCE_MATRIX2_ATTRIBUTE_INDEX 5u
#define FVIZ_GL_INSTANCE_MATRIX3_ATTRIBUTE_INDEX 6u
#define FVIZ_GL_INSTANCE_COLOR_ATTRIBUTE_INDEX 7u
#define FVIZ_GL_MULTISAMPLE 0x809D
#define FVIZ_GL_POINTS 0x0000
#define FVIZ_GL_LINES_ADJACENCY 0x000A
#define FVIZ_GL_SCISSOR_TEST 0x0C11
#define FVIZ_GL_VIEWPORT 0x0BA2
#define FVIZ_GL_CULL_FACE_MODE 0x0B45
#define FVIZ_GL_POLYGON_MODE 0x0B40
#define FVIZ_GL_FRAMEBUFFER_BINDING 0x8CA6
#define FVIZ_GL_CURRENT_PROGRAM 0x8B8D
#define FVIZ_GL_VERTEX_ARRAY_BINDING 0x85B5
#define FVIZ_GL_READ_BUFFER 0x0C02
#define FVIZ_GL_COLOR_WRITEMASK 0x0C23
#define FVIZ_GL_DEPTH_FUNC 0x0B74
#define FVIZ_GL_LINE_WIDTH 0x0B21
#define FVIZ_GL_POINT_SIZE 0x0B11
#define FVIZ_GL_LESS 0x0201
#define FVIZ_GL_LEQUAL 0x0203
#define FVIZ_GL_TIME_ELAPSED 0x88BF
#define FVIZ_GL_QUERY_RESULT 0x8866
#define FVIZ_GL_QUERY_RESULT_AVAILABLE 0x8867

static const char* const k_fviz_gl_vertex_shader_source =
    "#version 330 core\n"
    "layout(location = 0) in vec3 aPosition;\n"
    "layout(location = 1) in vec3 aNormal;\n"
    "layout(location = 2) in vec4 aColor;\n"
    "layout(location = 3) in vec4 aInstance0;\n"
    "layout(location = 4) in vec4 aInstance1;\n"
    "layout(location = 5) in vec4 aInstance2;\n"
    "layout(location = 6) in vec4 aInstance3;\n"
    "layout(location = 7) in vec4 aInstanceColor;\n"
    "uniform mat4 uMvp;\n"
    "uniform mat4 uModel;\n"
    "uniform mat3 uNormalMatrix;\n"
    "uniform int uInstancingEnabled;\n"
    "out vec3 vNormal;\n"
    "out vec3 vWorldPos;\n"
    "out vec4 vColor;\n"
    "void main()\n"
    "{\n"
    "    mat4 instanceMatrix = mat4(aInstance0, aInstance1, aInstance2, aInstance3);\n"
    "    vec4 localPosition = uInstancingEnabled == 1 ? instanceMatrix * vec4(aPosition, 1.0) : vec4(aPosition, 1.0);\n"
    "    gl_Position = uMvp * localPosition;\n"
    "    mat3 instanceNormal = uInstancingEnabled == 1 ? transpose(inverse(mat3(instanceMatrix))) : mat3(1.0);\n"
    "    vNormal = normalize(uNormalMatrix * instanceNormal * aNormal);\n"
    "    vWorldPos = vec3(uModel * localPosition);\n"
    "    vColor = uInstancingEnabled == 1 ? aInstanceColor : aColor;\n"
    "}\n";

static const char* const k_fviz_gl_fragment_shader_source =
    "#version 330 core\n"
    "in vec3 vNormal;\n"
    "in vec3 vWorldPos;\n"
    "in vec4 vColor;\n"
    "uniform vec3 uDiffuse;\n"
    "uniform int uLightCount;\n"
    "uniform vec4 uLightPositionIntensity[4];\n"
    "uniform vec3 uLightColor[4];\n"
    "uniform vec3 uCameraPosition;\n"
    "uniform int uScalarColorEnabled;\n"
    "uniform float uOpacity;\n"
    "uniform float uAmbientFactor;\n"
    "uniform float uDiffuseFactor;\n"
    "uniform float uSpecularFactor;\n"
    "uniform float uSpecularPower;\n"
    "uniform int uFlatShading;\n"
    "uniform int uClipPlaneCount;\n"
    "uniform vec4 uClipPlanes[6];\n"
    "uniform int uOITPass;\n"
    "uniform float uOITWeightScale;\n"
    "uniform float uOITDepthWeight;\n"
    "uniform float uOITMinimumWeight;\n"
    "uniform float uOITAlphaCutoff;\n"
    "uniform int uPeelEnabled;\n"
    "uniform sampler2D uPeelDepthTexture;\n"
    "uniform float uPeelScreenWidthInv;\n"
    "uniform float uPeelScreenHeightInv;\n"
    "out vec4 outColor;\n"
    "void main()\n"
    "{\n"
    "    for (int i = 0; i < uClipPlaneCount; ++i)\n"
    "        if (dot(uClipPlanes[i].xyz, vWorldPos) + uClipPlanes[i].w < 0.0) discard;\n"
    "    if (uPeelEnabled == 1) {\n"
    "        vec2 uv = gl_FragCoord.xy * vec2(uPeelScreenWidthInv, uPeelScreenHeightInv);\n"
    "        float peeledDepth = texture(uPeelDepthTexture, uv).r;\n"
    "        if (gl_FragCoord.z <= peeledDepth + 1e-5) discard;\n"
    "    }\n"
    "    vec3 baseColor = uScalarColorEnabled == 1 ? vColor.rgb : uDiffuse;\n"
    "    vec3 faceNormal = normalize(cross(dFdx(vWorldPos), dFdy(vWorldPos)));\n"
    "    vec3 n = uFlatShading == 1 ? faceNormal : normalize(vNormal);\n"
    "    if (length(n) < 0.5) n = faceNormal;\n"
    "    if (!gl_FrontFacing) n = -n;\n"
    "    vec3 v = normalize(uCameraPosition - vWorldPos);\n"
    "    vec3 color = baseColor * uAmbientFactor;\n"
    "    for (int lightIndex = 0; lightIndex < uLightCount; ++lightIndex)\n"
    "    {\n"
    "        vec3 l = normalize(uLightPositionIntensity[lightIndex].xyz - vWorldPos);\n"
    "        vec3 h = normalize(l + v);\n"
    "        float ndotl = max(dot(n, l), 0.0);\n"
    "        float spec = ndotl > 0.0 ? pow(max(dot(n, h), 0.0), uSpecularPower) : 0.0;\n"
    "        float intensity = uLightPositionIntensity[lightIndex].w;\n"
    "        vec3 lightColor = uLightColor[lightIndex];\n"
    "        color += baseColor * lightColor * (uDiffuseFactor * ndotl * intensity);\n"
    "        color += lightColor * (uSpecularFactor * spec * intensity);\n"
    "    }\n"
    "    color = min(color, vec3(1.0));\n"
    "    float alpha = uOpacity * (uScalarColorEnabled == 1 ? vColor.a : 1.0);\n"
    "    if (alpha <= uOITAlphaCutoff && uOITPass != 0) discard;\n"
    "    if (uOITPass == 1) {\n"
    "        float z = clamp(1.0 - gl_FragCoord.z, 0.0, 1.0);\n"
    "        float w = max(uOITMinimumWeight, alpha * uOITWeightScale * pow(max(z, 1e-4), uOITDepthWeight));\n"
    "        outColor = vec4(color * alpha * w, alpha * w); return; }\n"
    "    if (uOITPass == 2) { outColor = vec4(0.0, 0.0, 0.0, alpha); return; }\n"
    "    outColor = vec4(color, alpha);\n"
    "}\n";

static const char* const k_fviz_gl2d_vertex_shader_source = "#version 330 core\n"
                                                            "layout(location = 0) in vec2 aPosition;\n"
                                                            "layout(location = 1) in vec3 aColor;\n"
                                                            "uniform mat4 uMvp;\n"
                                                            "out vec3 vColor;\n"
                                                            "void main()\n"
                                                            "{\n"
                                                            "    gl_Position = uMvp * vec4(aPosition, 0.0, 1.0);\n"
                                                            "    vColor = aColor;\n"
                                                            "}\n";

static const char* const k_fviz_gl_edge_vertex_shader_source =
    "#version 330 core\n"
    "layout(location = 0) in vec3 aPosition;\n"
    "layout(location = 2) in vec4 aColor;\n"
    "layout(location = 3) in vec4 aInstance0;\n"
    "layout(location = 4) in vec4 aInstance1;\n"
    "layout(location = 5) in vec4 aInstance2;\n"
    "layout(location = 6) in vec4 aInstance3;\n"
    "layout(location = 7) in vec4 aInstanceColor;\n"
    "uniform mat4 uMvp;\n"
    "uniform int uInstancingEnabled;\n"
    "uniform float uLineDepthBias;\n"
    "out vec4 vLineColor;\n"
    "void main() {\n"
    "    mat4 instanceMatrix = mat4(aInstance0, aInstance1, aInstance2, aInstance3);\n"
    "    vec4 p = uInstancingEnabled == 1 ? instanceMatrix * vec4(aPosition, 1.0) : vec4(aPosition, 1.0);\n"
    "    gl_Position = uMvp * p;\n"
    "    gl_Position.z -= uLineDepthBias * gl_Position.w;\n"
    "    vLineColor = uInstancingEnabled == 1 ? aInstanceColor : aColor;\n"
    "}\n";

/* Volume ray-casting: the vertex shader expands the volume's unit cube; the
 * fragment shader computes entry/exit intersections and marches through the
 * 3D scalar texture, compositing front-to-back through the transfer function.
 * The scalar field is uploaded as GL_R32F (component 0) and the transfer
 * function as a 256x1 RGBA8 texture. */
static const char* const k_fviz_gl_volume_vertex_shader_source =
    "#version 330 core\n"
    "layout(location = 0) in vec3 aPosition;\n"
    "uniform mat4 uMvp;\n"
    "uniform mat4 uModel;\n"
    "out vec3 vObjectPos;\n"
    "out vec3 vWorldPos;\n"
    "void main()\n"
    "{\n"
    "    vObjectPos = aPosition;\n"
    "    vWorldPos = vec3(uModel * vec4(aPosition, 1.0));\n"
    "    gl_Position = uMvp * vec4(aPosition, 1.0);\n"
    "}\n";

static const char* const k_fviz_gl_volume_fragment_shader_source =
    "#version 330 core\n"
    "in vec3 vObjectPos;\n"
    "in vec3 vWorldPos;\n"
    "uniform mat4 uInvModel;\n"
    "uniform vec3 uCameraPosition;\n"
    "uniform vec3 uBoundsMin;\n"
    "uniform vec3 uBoundsMax;\n"
    "uniform sampler3D uScalarTexture;\n"
    "uniform sampler2D uTransferTexture;\n"
    "uniform float uStepSize;\n"
    "uniform float uScalarRangeMin;\n"
    "uniform float uScalarRangeMax;\n"
    "uniform int uShading;\n"
    "uniform int uLightCount;\n"
    "uniform vec4 uLightPositionIntensity[4];\n"
    "uniform vec3 uLightColor[4];\n"
    "uniform float uAmbientFactor;\n"
    "uniform float uDiffuseFactor;\n"
    "uniform float uSpecularFactor;\n"
    "uniform float uSpecularPower;\n"
    "out vec4 outColor;\n"
    "vec2 intersect_box(vec3 origin, vec3 direction, vec3 box_min, vec3 box_max)\n"
    "{\n"
    "    vec3 inv_dir = 1.0 / direction;\n"
    "    vec3 t0 = (box_min - origin) * inv_dir;\n"
    "    vec3 t1 = (box_max - origin) * inv_dir;\n"
    "    vec3 tmin = min(t0, t1);\n"
    "    vec3 tmax = max(t0, t1);\n"
    "    float t_near = max(max(tmin.x, tmin.y), tmin.z);\n"
    "    float t_far = min(min(tmax.x, tmax.y), tmax.z);\n"
    "    return vec2(t_near, t_far);\n"
    "}\n"
    "float scalar_at(vec3 object_pos)\n"
    "{\n"
    "    return texture(uScalarTexture, object_pos).r;\n"
    "}\n"
    "vec3 gradient_at(vec3 object_pos)\n"
    "{\n"
    "    vec3 tex_size = vec3(textureSize(uScalarTexture, 0));\n"
    "    vec3 e = 0.5 / max(tex_size, vec3(1.0));\n"
    "    float dx = scalar_at(object_pos + vec3(e.x, 0.0, 0.0)) - scalar_at(object_pos - vec3(e.x, 0.0, 0.0));\n"
    "    float dy = scalar_at(object_pos + vec3(0.0, e.y, 0.0)) - scalar_at(object_pos - vec3(0.0, e.y, 0.0));\n"
    "    float dz = scalar_at(object_pos + vec3(0.0, 0.0, e.z)) - scalar_at(object_pos - vec3(0.0, 0.0, e.z));\n"
    "    vec3 g = vec3(dx, dy, dz);\n"
    "    return length(g) > 1e-6 ? normalize(g) : vec3(0.0, 0.0, -1.0);\n"
    "}\n"
    "void main()\n"
    "{\n"
    "    vec3 ray_origin = uCameraPosition;\n"
    "    vec3 ray_direction = normalize(vWorldPos - uCameraPosition);\n"
    "    vec2 hits = intersect_box(ray_origin, ray_direction, uBoundsMin, uBoundsMax);\n"
    "    if (hits.y < hits.x) discard;\n"
    "    float t_start = max(hits.x, 0.0);\n"
    "    float t_end = hits.y;\n"
    "    float distance = t_end - t_start;\n"
    "    int steps = max(2, int(ceil(distance / max(uStepSize, 1e-5))));\n"
    "    float dt = distance / float(steps);\n"
    "    vec4 accumulated = vec4(0.0);\n"
    "    float transmittance = 1.0;\n"
    "    vec3 forward = normalize(-ray_direction);\n"
    "    for (int step = 0; step < steps; ++step)\n"
    "    {\n"
    "        float t = t_start + (float(step) + 0.5) * dt;\n"
    "        vec3 world_pos = ray_origin + ray_direction * t;\n"
    "        vec3 object_pos = vec3(uInvModel * vec4(world_pos, 1.0));\n"
    "        if (any(lessThan(object_pos, vec3(0.0))) || any(greaterThan(object_pos, vec3(1.0)))) continue;\n"
    "        float scalar = scalar_at(object_pos);\n"
    "        float s = clamp((scalar - uScalarRangeMin) / max(uScalarRangeMax - uScalarRangeMin, 1e-6), 0.0, 1.0);\n"
    "        vec4 tf = texture(uTransferTexture, vec2(s, 0.5));\n"
    "        if (tf.a <= 0.003) continue;\n"
    "        vec3 color = tf.rgb;\n"
    "        if (uShading == 1)\n"
    "        {\n"
    "            vec3 n = gradient_at(object_pos);\n"
    "            if (dot(n, forward) < 0.0) n = -n;\n"
    "            vec3 shaded = color * uAmbientFactor;\n"
    "            for (int i = 0; i < 4; ++i)\n"
    "            {\n"
    "                if (i >= uLightCount) break;\n"
    "                vec3 light_pos = uLightPositionIntensity[i].xyz;\n"
    "                vec3 l = normalize(light_pos - world_pos);\n"
    "                vec3 h = normalize(l + forward);\n"
    "                float ndotl = max(dot(n, l), 0.0);\n"
    "                float spec = ndotl > 0.0 ? pow(max(dot(n, h), 0.0), uSpecularPower) : 0.0;\n"
    "                float intensity = uLightPositionIntensity[i].w;\n"
    "                vec3 light_color = uLightColor[i];\n"
    "                shaded += color * light_color * (uDiffuseFactor * ndotl * intensity);\n"
    "                shaded += light_color * (uSpecularFactor * spec * intensity);\n"
    "            }\n"
    "            color = min(shaded, vec3(1.0));\n"
    "        }\n"
    "        float alpha = 1.0 - pow(1.0 - tf.a, dt / max(uStepSize, 1e-5));\n"
    "        accumulated.rgb += transmittance * alpha * color;\n"
    "        accumulated.a += transmittance * alpha;\n"
    "        transmittance *= (1.0 - alpha);\n"
    "        if (transmittance < 0.01) break;\n"
    "    }\n"
    "    outColor = vec4(accumulated.rgb, accumulated.a);\n"
    "}\n";

static const char* const k_fviz_gl_edge_geometry_shader_source =
    "#version 330 core\n"
    "layout(lines_adjacency) in;\n"
    "layout(triangle_strip, max_vertices = 4) out;\n"
    "in vec4 vLineColor[];\n"
    "uniform vec3 uViewportSize;\n"
    "uniform float uLineWidth;\n"
    "uniform int uLineCap;\n"
    "uniform int uLineJoin;\n"
    "uniform float uMiterLimit;\n"
    "noperspective out float gDistance;\n"
    "noperspective out float gAlong;\n"
    "noperspective out float gRoundEnd;\n"
    "flat out float gSegmentLength;\n"
    "out vec4 gColor;\n"
    "void emitPoint(vec4 p, vec2 offsetNdc, float d, float alongPx, float roundEnd, vec4 color, float lenPx) {\n"
    "    gl_Position = p + vec4(offsetNdc * p.w, 0.0, 0.0);\n"
    "    gDistance = d; gAlong = alongPx; gRoundEnd = roundEnd; gSegmentLength = lenPx; gColor = color; EmitVertex();\n"
    "}\n"
    "vec2 toPx(vec4 p) { return (p.xy / p.w) * uViewportSize.xy * 0.5; }\n"
    "vec2 safeDir(vec2 d, vec2 fallbackDir) { float l=length(d); return l>1e-5 ? d/l : fallbackDir; }\n"
    "void main() {\n"
    "    vec4 pp = gl_in[0].gl_Position; vec4 p0 = gl_in[1].gl_Position;\n"
    "    vec4 p1 = gl_in[2].gl_Position; vec4 pn = gl_in[3].gl_Position;\n"
    "    if (abs(p0.w)<1e-6 || abs(p1.w)<1e-6) return;\n"
    "    vec2 qPrev=toPx(pp), q0=toPx(p0), q1=toPx(p1), qNext=toPx(pn);\n"
    "    vec2 delta=q1-q0; float lenPx=length(delta); if(lenPx<1e-4)return;\n"
    "    vec2 dir=delta/lenPx; vec2 n=vec2(-dir.y,dir.x);\n"
    "    bool hasPrev=length(q0-qPrev)>1e-4; bool hasNext=length(qNext-q1)>1e-4;\n"
    "    vec2 dirPrev=safeDir(q0-qPrev,dir); vec2 dirNext=safeDir(qNext-q1,dir);\n"
    "    vec2 nPrev=vec2(-dirPrev.y,dirPrev.x); vec2 nNext=vec2(-dirNext.y,dirNext.x);\n"
    "    float halfCore=max(0.5,uLineWidth*0.5); float extent=halfCore+1.0;\n"
    "    vec2 startN=n; vec2 endN=n; float startScale=extent; float endScale=extent;\n"
    "    if(hasPrev && uLineJoin==0){ vec2 m=normalize(nPrev+n); float d=abs(dot(m,n));\n"
    "      float ml=d>1e-4?extent/d:extent*uMiterLimit; if(ml<=extent*uMiterLimit){startN=m;startScale=ml;} }\n"
    "    if(hasNext && uLineJoin==0){ vec2 m=normalize(n+nNext); float d=abs(dot(m,n));\n"
    "      float ml=d>1e-4?extent/d:extent*uMiterLimit; if(ml<=extent*uMiterLimit){endN=m;endScale=ml;} }\n"
    "    float startCap=!hasPrev ? (uLineCap==0?0.0:extent) : (uLineJoin==2?extent:0.0);\n"
    "    float endCap=!hasNext ? (uLineCap==0?0.0:extent) : (uLineJoin==2?extent:0.0);\n"
    "    float startRound=(!hasPrev && uLineCap==2) || (hasPrev && uLineJoin==2) ? 1.0 : 0.0;\n"
    "    float endRound=(!hasNext && uLineCap==2) || (hasNext && uLineJoin==2) ? 1.0 : 0.0;\n"
    "    vec2 startOff=startN*startScale*2.0/uViewportSize.xy;\n"
    "    vec2 endOff=endN*endScale*2.0/uViewportSize.xy;\n"
    "    vec2 startAlong=dir*startCap*2.0/uViewportSize.xy; vec2 endAlong=dir*endCap*2.0/uViewportSize.xy;\n"
    "    emitPoint(p0,-startOff-startAlong,-extent,-startCap,startRound,vLineColor[1],lenPx);\n"
    "    emitPoint(p0, startOff-startAlong, extent,-startCap,startRound,vLineColor[1],lenPx);\n"
    "    emitPoint(p1,-endOff+endAlong,-extent,lenPx+endCap,endRound,vLineColor[2],lenPx);\n"
    "    emitPoint(p1, endOff+endAlong, extent,lenPx+endCap,endRound,vLineColor[2],lenPx); EndPrimitive();\n"
    "}\n";

static const char* const k_fviz_gl_edge_fragment_shader_source =
    "#version 330 core\n"
    "noperspective in float gDistance;\n"
    "noperspective in float gAlong;\n"
    "noperspective in float gRoundEnd;\n"
    "flat in float gSegmentLength;\n"
    "in vec4 gColor;\n"
    "uniform vec4 uLineColor;\n"
    "uniform float uLineWidth;\n"
    "uniform float uLineDepthBias;\n"
    "uniform int uLineCap;\n"
    "uniform vec3 uLineDash;\n"
    "uniform int uScalarColorEnabled;\n"
    "out vec4 outColor;\n"
    "void main() {\n"
    "    float halfCore = max(0.5, uLineWidth * 0.5);\n"
    "    float lateral = abs(gDistance);\n"
    "    float longitudinal = 0.0;\n"
    "    if (gAlong < 0.0) longitudinal = -gAlong;\n"
    "    else if (gAlong > gSegmentLength) longitudinal = gAlong - gSegmentLength;\n"
    "    float signedDistance = lateral - halfCore;\n"
    "    if (gRoundEnd > 0.5 && longitudinal > 0.0)\n"
    "        signedDistance = length(vec2(lateral, longitudinal)) - halfCore;\n"
    "    else if (uLineCap == 0 && longitudinal > 0.0) discard;\n"
    "    else if (uLineCap == 1 && longitudinal > halfCore) discard;\n"
    "    float coverage = 1.0 - smoothstep(-0.5, 0.5, signedDistance);\n"
    "    float period = uLineDash.x + uLineDash.y;\n"
    "    if (uLineDash.x > 0.0 && uLineDash.y > 0.0 && period > 0.0) {\n"
    "        float d = mod(max(gAlong, 0.0) + uLineDash.z, period);\n"
    "        float edge = min(1.0, max(0.5, halfCore * 0.35));\n"
    "        float dashCoverage = 1.0 - smoothstep(uLineDash.x - edge, uLineDash.x + edge, d);\n"
    "        coverage *= dashCoverage;\n"
    "    }\n"
    "    if (coverage <= 0.001) discard;\n"
    "    /* The geometry shader expands a line into a quad.  At a sharp vertex\n"
    "       the expanded corners can otherwise interpolate behind the original\n"
    "       triangle even when the line endpoints have coincident depth. Apply\n"
    "       the same small clip-space bias at fragment depth as well. */\n"
    "    gl_FragDepth = max(0.0, gl_FragCoord.z - 0.5 * uLineDepthBias);\n"
    "    vec4 color = uScalarColorEnabled == 1 ? gColor : uLineColor;\n"
    "    outColor = vec4(color.rgb, color.a * coverage);\n"
    "}\n";

static const char* const k_fviz_gl_point_vertex_shader_source =
    "#version 330 core\n"
    "layout(location = 0) in vec3 aPosition;\n"
    "layout(location = 2) in vec4 aColor;\n"
    "layout(location = 3) in vec4 aInstance0;\n"
    "layout(location = 4) in vec4 aInstance1;\n"
    "layout(location = 5) in vec4 aInstance2;\n"
    "layout(location = 6) in vec4 aInstance3;\n"
    "layout(location = 7) in vec4 aInstanceColor;\n"
    "uniform mat4 uMvp;\n"
    "uniform float uPointSize;\n"
    "uniform int uInstancingEnabled;\n"
    "out vec4 vPointColor;\n"
    "void main() {\n"
    "    mat4 instanceMatrix = mat4(aInstance0, aInstance1, aInstance2, aInstance3);\n"
    "    vec4 p = uInstancingEnabled == 1 ? instanceMatrix * vec4(aPosition, 1.0) : vec4(aPosition, 1.0);\n"
    "    gl_Position = uMvp * p; gl_PointSize = uPointSize;\n"
    "    vPointColor = uInstancingEnabled == 1 ? aInstanceColor : aColor;\n"
    "}\n";

static const char* const k_fviz_gl_point_fragment_shader_source =
    "#version 330 core\n"
    "in vec4 vPointColor;\n"
    "uniform vec4 uPointColor;\n"
    "uniform int uPointShape;\n"
    "uniform int uScalarColorEnabled;\n"
    "out vec4 outColor;\n"
    "void main() {\n"
    "    vec2 p = gl_PointCoord * 2.0 - 1.0;\n"
    "    float r2 = dot(p, p);\n"
    "    float coverage = 1.0;\n"
    "    if (uPointShape != 0) {\n"
    "        float radius = sqrt(max(r2, 0.0));\n"
    "        coverage = 1.0 - smoothstep(0.92, 1.0, radius);\n"
    "        if (coverage <= 0.001) discard;\n"
    "    }\n"
    "    vec4 color = uScalarColorEnabled == 1 ? vPointColor : uPointColor;\n"
    "    if (uPointShape == 2) {\n"
    "        float nz = sqrt(max(0.0, 1.0 - r2));\n"
    "        float diffuse = 0.35 + 0.65 * max(nz, 0.0);\n"
    "        float highlight = pow(max(nz, 0.0), 24.0) * 0.22;\n"
    "        color.rgb = min(vec3(1.0), color.rgb * diffuse + vec3(highlight));\n"
    "    }\n"
    "    outColor = vec4(color.rgb, color.a * coverage);\n"
    "}\n";

static const char* const k_fviz_gl_selection_vertex_shader_source =
    "#version 330 core\n"
    "layout(location = 0) in vec3 aPosition;\n"
    "layout(location = 3) in vec4 aInstance0;\n"
    "layout(location = 4) in vec4 aInstance1;\n"
    "layout(location = 5) in vec4 aInstance2;\n"
    "layout(location = 6) in vec4 aInstance3;\n"
    "uniform mat4 uMvp;\n"
    "uniform mat4 uModel;\n"
    "uniform int uInstancingEnabled;\n"
    "out vec3 vWorldPos;\n"
    "flat out uint vInstanceId;\n"
    "void main() {\n"
    "    mat4 instanceMatrix = mat4(aInstance0, aInstance1, aInstance2, aInstance3);\n"
    "    vec4 local = uInstancingEnabled == 1 ? instanceMatrix * vec4(aPosition, 1.0) : vec4(aPosition, 1.0);\n"
    "    gl_Position = uMvp * local;\n"
    "    vWorldPos = vec3(uModel * local);\n"
    "    vInstanceId = uint(gl_InstanceID);\n"
    "}\n";

static const char* const k_fviz_gl_selection_fragment_shader_source =
    "#version 330 core\n"
    "uniform uint uActorId;\n"
    "uniform uint uAssociation;\n"
    "uniform int uClipPlaneCount;\n"
    "uniform vec4 uClipPlanes[6];\n"
    "in vec3 vWorldPos;\n"
    "flat in uint vInstanceId;\n"
    "layout(location = 0) out uvec4 outPick;\n"
    "void main()\n"
    "{\n"
    "    for (int i = 0; i < uClipPlaneCount; ++i)\n"
    "        if (dot(uClipPlanes[i].xyz, vWorldPos) + uClipPlanes[i].w < 0.0) discard;\n"
    "    uint item = uAssociation == 5u ? vInstanceId : uint(gl_PrimitiveID);\n"
    "    outPick = uvec4(uActorId + 1u, uAssociation, item + 1u, 0u);\n"
    "}\n";

static const char* const k_fviz_gl2d_fragment_shader_source = "#version 330 core\n"
                                                              "in vec3 vColor;\n"
                                                              "out vec4 outColor;\n"
                                                              "void main()\n"
                                                              "{\n"
                                                              "    outColor = vec4(vColor, 1.0);\n"
                                                              "}\n";

static const char* const k_fviz_gl_text_vertex_shader_source =
    "#version 330 core\n"
    "layout(location = 0) in vec3 aPosition;\n"
    "layout(location = 1) in vec2 aUv;\n"
    "layout(location = 2) in vec4 aColor;\n"
    "out vec2 vUv;\n"
    "out vec4 vColor;\n"
    "void main() { gl_Position = vec4(aPosition, 1.0); vUv = aUv; vColor = aColor; }\n";

static const char* const k_fviz_gl_text_fragment_shader_source =
    "#version 330 core\n"
    "in vec2 vUv;\n"
    "in vec4 vColor;\n"
    "uniform sampler2D uAtlas;\n"
    "uniform int uSolid;\n"
    "out vec4 outColor;\n"
    "void main() {\n"
    "  float mask = uSolid == 1 ? 1.0 : texture(uAtlas, vUv).r;\n"
    "  float coverage = uSolid == 1 ? 1.0 : smoothstep(0.08, 0.92, mask);\n"
    "  float alpha = vColor.a * coverage; if (alpha <= 0.001) discard;\n"
    "  outColor = vec4(vColor.rgb, alpha);\n"
    "}\n";

static const char* const k_fviz_gl_fxaa_vertex_shader_source =
    "#version 330 core\n"
    "out vec2 vUv;\n"
    "void main()\n"
    "{\n"
    "    vec2 p = gl_VertexID == 0 ? vec2(-1.0, -1.0) :\n"
    "             (gl_VertexID == 1 ? vec2(3.0, -1.0) : vec2(-1.0, 3.0));\n"
    "    vUv = p * 0.5 + 0.5;\n"
    "    gl_Position = vec4(p, 0.0, 1.0);\n"
    "}\n";

static const char* const k_fviz_gl_fxaa_fragment_shader_source =
    "#version 330 core\n"
    "in vec2 vUv;\n"
    "uniform sampler2D uColor;\n"
    "uniform vec4 uInvScreen;\n"
    "uniform float uEdgeThreshold;\n"
    "uniform float uEdgeThresholdMin;\n"
    "uniform float uSpanMax;\n"
    "out vec4 outColor;\n"
    "float luma(vec3 c) { return dot(c, vec3(0.299, 0.587, 0.114)); }\n"
    "void main()\n"
    "{\n"
    "    vec2 px = uInvScreen.xy;\n"
    "    vec4 center = texture(uColor, vUv);\n"
    "    float m = luma(center.rgb);\n"
    "    float nw = luma(texture(uColor, vUv + vec2(-px.x,  px.y)).rgb);\n"
    "    float ne = luma(texture(uColor, vUv + vec2( px.x,  px.y)).rgb);\n"
    "    float sw = luma(texture(uColor, vUv + vec2(-px.x, -px.y)).rgb);\n"
    "    float se = luma(texture(uColor, vUv + vec2( px.x, -px.y)).rgb);\n"
    "    float lo = min(m, min(min(nw, ne), min(sw, se)));\n"
    "    float hi = max(m, max(max(nw, ne), max(sw, se)));\n"
    "    if (hi - lo < max(uEdgeThresholdMin, hi * uEdgeThreshold)) { outColor = center; return; }\n"
    "    vec2 dir;\n"
    "    dir.x = -((nw + ne) - (sw + se));\n"
    "    dir.y =  ((nw + sw) - (ne + se));\n"
    "    float reduce = max((nw + ne + sw + se) * (0.25 * 0.125), 1.0 / 128.0);\n"
    "    float invMin = 1.0 / (min(abs(dir.x), abs(dir.y)) + reduce);\n"
    "    dir = clamp(dir * invMin, vec2(-uSpanMax), vec2(uSpanMax)) * px;\n"
    "    vec3 a = 0.5 * (texture(uColor, vUv + dir * (1.0/3.0 - 0.5)).rgb +\n"
    "                    texture(uColor, vUv + dir * (2.0/3.0 - 0.5)).rgb);\n"
    "    vec3 b = a * 0.5 + 0.25 * (texture(uColor, vUv + dir * -0.5).rgb +\n"
    "                                texture(uColor, vUv + dir *  0.5).rgb);\n"
    "    float lb = luma(b);\n"
    "    outColor = vec4((lb < lo || lb > hi) ? a : b, center.a);\n"
    "}\n";

static const char* const k_fviz_gl_oit_composite_fragment_shader_source =
    "#version 330 core\n"
    "in vec2 vUv;\n"
    "uniform sampler2D uAccum;\n"
    "uniform sampler2D uReveal;\n"
    "out vec4 outColor;\n"
    "void main()\n"
    "{\n"
    "    vec4 accum = texture(uAccum, vUv);\n"
    "    float reveal = clamp(texture(uReveal, vUv).r, 0.0, 1.0);\n"
    "    float alpha = 1.0 - reveal;\n"
    "    if (alpha <= 1e-5) discard;\n"
    "    vec3 color = accum.rgb / max(accum.a, 1e-5);\n"
    "    outColor = vec4(color, alpha);\n"
    "}\n";

typedef struct FVizGLActorResource
{
    /* GPU mesh resources are mapper-owned rather than actor-owned. Multiple
       actors may share one mapper while keeping independent transforms and
       material uniforms. */
    const FVizMapper* mapper;
    const FVizPolyData* poly_data;
    const FVizGlyphMapper* glyph_mapper;
    GLuint vao;
    GLuint position_buffer;
    GLuint normal_buffer;
    GLuint index_buffer;
    GLuint color_buffer;
    GLuint line_index_buffer;
    GLuint line_adjacency_index_buffer;
    GLuint triangle_edge_index_buffer;
    GLuint triangle_edge_adjacency_index_buffer;
    GLuint point_index_buffer;
    GLuint instance_buffer;
    FVizSize instance_buffer_bytes;
    FVizBool has_color;
    GLsizei index_count;
    GLsizei line_index_count;
    GLsizei line_adjacency_index_count;
    GLsizei source_line_index_count;
    GLsizei triangle_edge_index_count;
    GLsizei triangle_edge_adjacency_index_count;
    GLsizei point_index_count;
    GLsizei instance_count;
    FVizMTime poly_data_mtime;
    FVizMTime render_data_mtime;
    FVizMTime geometry_mtime;
    FVizMTime topology_mtime;
    FVizMTime attribute_mtime;
    FVizMTime color_data_mtime;
    const FVizDataArray* color_array;
    FVizMTime color_array_mtime;
    FVizMTime glyph_mtime;
    FVizSize point_count;
    uint64_t last_seen_frame;
} FVizGLActorResource;

static uint64_t fviz_gl_actor_resource_resident_bytes(const FVizGLActorResource* resource)
{
    uint64_t bytes = 0u;
    if (resource == NULL) return 0u;
#define FVIZ_ADD_RESIDENT_BUFFER(Buffer, Count, Stride)                                                                \
    do                                                                                                                 \
    {                                                                                                                  \
        if ((Buffer) != 0u && (Count) > 0) bytes += (uint64_t)(Count) * (uint64_t)(Stride);                            \
    } while (0)
    FVIZ_ADD_RESIDENT_BUFFER(resource->position_buffer, resource->point_count, sizeof(FVizVec3));
    FVIZ_ADD_RESIDENT_BUFFER(resource->normal_buffer, resource->point_count, sizeof(FVizVec3));
    FVIZ_ADD_RESIDENT_BUFFER(resource->index_buffer, resource->index_count, sizeof(uint32_t));
    FVIZ_ADD_RESIDENT_BUFFER(resource->color_buffer, resource->point_count, sizeof(float) * 4u);
    FVIZ_ADD_RESIDENT_BUFFER(resource->line_index_buffer, resource->line_index_count, sizeof(uint32_t));
    FVIZ_ADD_RESIDENT_BUFFER(resource->line_adjacency_index_buffer, resource->line_adjacency_index_count,
                             sizeof(uint32_t));
    FVIZ_ADD_RESIDENT_BUFFER(resource->triangle_edge_index_buffer, resource->triangle_edge_index_count,
                             sizeof(uint32_t));
    FVIZ_ADD_RESIDENT_BUFFER(resource->triangle_edge_adjacency_index_buffer,
                             resource->triangle_edge_adjacency_index_count, sizeof(uint32_t));
    FVIZ_ADD_RESIDENT_BUFFER(resource->point_index_buffer, resource->point_index_count, sizeof(uint32_t));
#undef FVIZ_ADD_RESIDENT_BUFFER
    if (resource->instance_buffer != 0u) bytes += (uint64_t)resource->instance_buffer_bytes;
    return bytes;
}

static FVizBool fviz_gl_actor_resource_pinned(const FVizGLActorResource* resource)
{
    if (resource == NULL) return FVIZ_FALSE;
    if (resource->mapper != NULL) return fviz_mapper_gpu_residency_pinned(resource->mapper);
    return resource->glyph_mapper != NULL ? fviz_glyph_mapper_gpu_residency_pinned(resource->glyph_mapper) : FVIZ_FALSE;
}

static void fviz_gl_actor_resource_resident_classes(const FVizGLActorResource* resource, uint64_t* geometry,
                                                    uint64_t* attributes, uint64_t* instances)
{
    uint64_t total;
    uint64_t attribute_bytes = 0u;
    uint64_t instance_bytes = 0u;
    if (resource == NULL) return;
    total = fviz_gl_actor_resource_resident_bytes(resource);
    if (resource->color_buffer != 0u) attribute_bytes = (uint64_t)resource->point_count * sizeof(float) * 4u;
    if (resource->instance_buffer != 0u) instance_bytes = (uint64_t)resource->instance_buffer_bytes;
    *attributes += attribute_bytes;
    *instances += instance_bytes;
    *geometry += total >= attribute_bytes + instance_bytes ? total - attribute_bytes - instance_bytes : 0u;
}

typedef struct FVizGLColor
{
    float r;
    float g;
    float b;
    float a;
} FVizGLColor;

typedef struct FVizGLGlyphInstance
{
    float matrix[16];
    float color[4];
} FVizGLGlyphInstance;

typedef struct FVizGLDrawItem
{
    const FVizActor* actor;
    float distance_squared;
    FVizSize source_index;
} FVizGLDrawItem;

struct FVizGLDevice
{
    FVizGLFunctions gl;
    GLuint program;
    GLint mvp_location;
    GLint model_location;
    GLint normal_matrix_location;
    GLint instancing_location;
    GLint diffuse_location;
    GLint light_count_location;
    GLint light_position_intensity_location;
    GLint light_color_location;
    GLint camera_position_location;
    GLint ambient_factor_location;
    GLint diffuse_factor_location;
    GLint specular_factor_location;
    GLint specular_power_location;
    GLint flat_shading_location;
    GLint scalar_color_location;
    GLint opacity_location;
    GLint clip_plane_count_location;
    GLint clip_planes_location;
    GLint oit_pass_location;
    GLint oit_weight_scale_location;
    GLint oit_depth_weight_location;
    GLint oit_minimum_weight_location;
    GLint oit_alpha_cutoff_location;
    GLint peel_enabled_location;
    GLint peel_depth_texture_location;
    GLint peel_screen_width_inv_location;
    GLint peel_screen_height_inv_location;
    GLuint program_2d;
    GLint mvp_location_2d;
    GLuint edge_program;
    GLint edge_mvp_location;
    GLint edge_viewport_location;
    GLint edge_width_location;
    GLint edge_depth_bias_location;
    GLint edge_color_location;
    GLint edge_cap_location;
    GLint edge_join_location;
    GLint edge_miter_limit_location;
    GLint edge_dash_location;
    GLint edge_scalar_color_location;
    GLint edge_instancing_location;
    GLuint point_program;
    GLint point_mvp_location;
    GLint point_size_location;
    GLint point_color_location;
    GLint point_shape_location;
    GLint point_scalar_color_location;
    GLint point_instancing_location;
    GLuint selection_program;
    GLint selection_mvp_location;
    GLint selection_model_location;
    GLint selection_actor_id_location;
    GLint selection_association_location;
    GLint selection_instancing_location;
    GLint selection_clip_plane_count_location;
    GLint selection_clip_planes_location;
    GLuint selection_framebuffer;
    GLuint selection_texture;
    GLuint selection_depth_renderbuffer;
    int selection_width;
    int selection_height;
    GLuint oit_composite_program;
    GLint oit_accum_location;
    GLint oit_reveal_location;
    GLuint oit_vao;
    GLuint oit_framebuffer;
    GLuint oit_resolve_framebuffer;
    GLuint oit_color_renderbuffer;
    GLuint oit_depth_renderbuffer;
    GLuint oit_accum_texture;
    GLuint oit_reveal_texture;
    int oit_width;
    int oit_height;
    uint32_t oit_samples;
    int oit_pass;
    /* Dual depth peeling state. */
    GLuint peel_front_depth_texture;
    GLuint peel_back_depth_texture;
    GLuint peel_color_texture;
    GLuint peel_framebuffer;
    GLuint peel_depth_framebuffer;
    GLuint peel_depth_renderbuffer;
    GLint peel_depth_location;
    GLint peel_color_attached;
    int peel_pass;
    int peel_width;
    int peel_height;
    FVizBool peel_supported;
    /* Volume ray-casting state. */
    GLuint volume_program;
    GLint volume_model_location;
    GLint volume_inv_model_location;
    GLint volume_mvp_location;
    GLint volume_camera_position_location;
    GLint volume_scalar_texture_location;
    GLint volume_transfer_texture_location;
    GLint volume_step_size_location;
    GLint volume_scalar_range_location;
    GLint volume_scalar_range_max_location;
    GLint volume_bounds_min_location;
    GLint volume_bounds_max_location;
    GLint volume_shading_location;
    GLint volume_light_count_location;
    GLint volume_light_position_intensity_location;
    GLint volume_light_color_location;
    GLint volume_ambient_location;
    GLint volume_diffuse_location;
    GLint volume_specular_location;
    GLint volume_specular_power_location;
    GLuint volume_vao;
    GLuint volume_vbo;
    GLuint volume_ibo;
    GLuint volume_scalar_texture;
    GLuint volume_transfer_texture;
    int volume_scalar_width;
    int volume_scalar_height;
    int volume_scalar_depth;
    const FVizVolumeMapper* volume_uploaded_mapper;
    FVizMTime volume_texture_mtime;
    FVizMTime volume_transfer_mtime;
    FVizBool volume_program_ready;
    FVizBool volume_texture_ready;
    GLuint fxaa_program;
    GLint fxaa_color_location;
    GLint fxaa_inv_screen_location;
    GLint fxaa_edge_threshold_location;
    GLint fxaa_edge_threshold_min_location;
    GLint fxaa_span_max_location;
    GLuint fxaa_vao;
    GLuint fxaa_texture;
    GLuint fxaa_framebuffer;
    int fxaa_width;
    int fxaa_height;
    FVizBool fxaa_srgb;
    GLuint overlay_vao;
    GLuint overlay_vbo;
    GLsizeiptr overlay_capacity_bytes;
    GLuint text_program;
    GLint text_atlas_location;
    GLint text_solid_location;
    GLuint text_vao;
    GLuint text_vbo;
    GLuint text_texture;
    GLsizeiptr text_capacity_bytes;
    const FVizFontAtlas* text_atlas;
    void* text_staging;
    FVizSize text_staging_capacity_vertices;
    uint64_t frame_serial;
    FVizGLFrameStatistics frame_statistics;
    GLuint gpu_time_queries[2];
    FVizBool gpu_time_query_issued[2];
    uint32_t gpu_time_write_index;
    FVizBool gpu_time_query_active;
    FVizGLDrawItem* draw_items;
    FVizSize draw_item_capacity;
    FVizGLActorResource* actors;
    FVizSize actor_count;
    FVizSize actor_capacity;
    uint64_t mesh_byte_budget;
    uint32_t unused_resource_retention_frames;
};

static const FVizPolyData* fviz_gl_actor_render_poly_data(const FVizActor* actor)
{
    const FVizGlyphMapper* glyph_mapper;
    if (actor == NULL) return NULL;
    glyph_mapper = fviz_actor_const_glyph_mapper(actor);
    return glyph_mapper != NULL ? fviz_glyph_mapper_const_source_poly_data(glyph_mapper)
                                : fviz_actor_const_poly_data(actor);
}

static void fviz_gl_glyph_instance_to_gpu(const FVizGlyphInstance* source, FVizGLGlyphInstance* destination)
{
    FVizMat3 rotation;
    if (source == NULL || destination == NULL) return;
    rotation = fviz_mat3_from_quaternion(source->orientation);
    (void)memset(destination, 0, sizeof(*destination));
    destination->matrix[0] = rotation.m[0] * source->scale.x;
    destination->matrix[1] = rotation.m[1] * source->scale.x;
    destination->matrix[2] = rotation.m[2] * source->scale.x;
    destination->matrix[4] = rotation.m[3] * source->scale.y;
    destination->matrix[5] = rotation.m[4] * source->scale.y;
    destination->matrix[6] = rotation.m[5] * source->scale.y;
    destination->matrix[8] = rotation.m[6] * source->scale.z;
    destination->matrix[9] = rotation.m[7] * source->scale.z;
    destination->matrix[10] = rotation.m[8] * source->scale.z;
    destination->matrix[12] = source->position.x;
    destination->matrix[13] = source->position.y;
    destination->matrix[14] = source->position.z;
    destination->matrix[15] = 1.0f;
    destination->color[0] = source->color[0];
    destination->color[1] = source->color[1];
    destination->color[2] = source->color[2];
    destination->color[3] = source->color[3];
}

static FVizVec3 fviz_gl_transform_point(FVizMat4 matrix, FVizVec3 point)
{
    return fviz_vec3(matrix.m[0] * point.x + matrix.m[4] * point.y + matrix.m[8] * point.z + matrix.m[12],
                     matrix.m[1] * point.x + matrix.m[5] * point.y + matrix.m[9] * point.z + matrix.m[13],
                     matrix.m[2] * point.x + matrix.m[6] * point.y + matrix.m[10] * point.z + matrix.m[14]);
}

static FVizBool fviz_gl_actor_is_translucent(const FVizActor* actor)
{
    const FVizGlyphMapper* glyph_mapper;
    if (actor == NULL) return FVIZ_FALSE;
    if (fviz_actor_const_volume_mapper(actor) != NULL) return FVIZ_TRUE;
    if (fviz_actor_opacity(actor) < 0.999999f) return FVIZ_TRUE;
    glyph_mapper = fviz_actor_const_glyph_mapper(actor);
    return glyph_mapper != NULL && fviz_glyph_mapper_has_translucent_instances(glyph_mapper) != FVIZ_FALSE ? FVIZ_TRUE
                                                                                                           : FVIZ_FALSE;
}

static int fviz_gl_draw_item_compare_back_to_front(const void* lhs, const void* rhs)
{
    const FVizGLDrawItem* a = (const FVizGLDrawItem*)lhs;
    const FVizGLDrawItem* b = (const FVizGLDrawItem*)rhs;
    if (a->distance_squared > b->distance_squared) return -1;
    if (a->distance_squared < b->distance_squared) return 1;
    if (a->source_index < b->source_index) return -1;
    if (a->source_index > b->source_index) return 1;
    return 0;
}

static FVizResult fviz_gl_prepare_translucent_order(FVizGLDevice* device, FVizScene* scene, FVizCamera* camera,
                                                    FVizSize* out_count)
{
    const FVizSize scene_count = fviz_scene_actor_count(scene);
    const FVizVec3 camera_position = fviz_camera_position(camera);
    FVizSize i;
    FVizSize count = 0u;
    if (out_count == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    *out_count = 0u;
    if (scene_count > device->draw_item_capacity)
    {
        FVizSize capacity = device->draw_item_capacity == 0u ? 16u : device->draw_item_capacity;
        FVizGLDrawItem* items;
        while (capacity < scene_count)
        {
            if (capacity > SIZE_MAX / 2u) return FVIZ_ERROR_OVERFLOW;
            capacity *= 2u;
        }
        {
            FVizSize bytes;
            if (fviz_size_multiply(capacity, (FVizSize)sizeof(*items), &bytes) != FVIZ_OK)
                return fviz_last_error_code();
            items = (FVizGLDrawItem*)fviz_realloc(device->draw_items, bytes);
        }
        if (items == NULL) return fviz_last_error_code();
        device->draw_items = items;
        device->draw_item_capacity = capacity;
    }
    for (i = 0u; i < scene_count; ++i)
    {
        const FVizActor* actor = fviz_scene_const_actor(scene, i);
        const FVizPolyData* poly_data;
        FVizVec3 center = fviz_vec3(0.0f, 0.0f, 0.0f);
        FVizVec3 offset;
        FVizBounds bounds = fviz_bounds_empty();
        if (actor == NULL || fviz_actor_is_visible(actor) == FVIZ_FALSE ||
            fviz_gl_actor_is_translucent(actor) == FVIZ_FALSE)
            continue;
        if (fviz_actor_const_glyph_mapper(actor) != NULL)
            bounds = fviz_glyph_mapper_bounds(fviz_actor_const_glyph_mapper(actor));
        else
        {
            poly_data = fviz_actor_const_poly_data(actor);
            if (poly_data != NULL) bounds = fviz_poly_data_bounds(poly_data);
        }
        if (bounds.valid != FVIZ_FALSE) center = fviz_bounds_center(&bounds);
        center = fviz_gl_transform_point(fviz_actor_transform_matrix(actor), center);
        offset = fviz_vec3_sub(center, camera_position);
        device->draw_items[count].actor = actor;
        device->draw_items[count].distance_squared = fviz_vec3_dot(offset, offset);
        device->draw_items[count].source_index = i;
        ++count;
    }
    if (count > 1u)
        qsort(device->draw_items, count, sizeof(*device->draw_items), fviz_gl_draw_item_compare_back_to_front);
    *out_count = count;
    return FVIZ_OK;
}

static FVizGLActorResource* fviz_gl_find_actor_resource(FVizGLDevice* device, const FVizActor* actor)
{
    const FVizGlyphMapper* glyph_mapper;
    const FVizMapper* mapper;
    FVizSize i;
    if (device == NULL || actor == NULL) return NULL;
    glyph_mapper = fviz_actor_const_glyph_mapper(actor);
    mapper = glyph_mapper == NULL ? fviz_actor_mapper((FVizActor*)actor) : NULL;
    for (i = 0u; i < device->actor_count; ++i)
    {
        FVizGLActorResource* resource = &device->actors[i];
        if (glyph_mapper != NULL)
        {
            if (resource->glyph_mapper == glyph_mapper) return resource;
        }
        else if (resource->glyph_mapper == NULL && resource->mapper == mapper)
        {
            return resource;
        }
    }
    return NULL;
}

static void fviz_gl_actor_resource_destroy(FVizGLDevice* device, FVizGLActorResource* resource)
{
    const FVizGLFunctions* gl = &device->gl;
    if (resource->instance_buffer != 0u)
    {
        gl->glDeleteBuffers(1, &resource->instance_buffer);
        resource->instance_buffer = 0u;
    }
    if (resource->point_index_buffer != 0u)
    {
        gl->glDeleteBuffers(1, &resource->point_index_buffer);
        resource->point_index_buffer = 0u;
    }
    if (resource->triangle_edge_index_buffer != 0u)
    {
        gl->glDeleteBuffers(1, &resource->triangle_edge_index_buffer);
        resource->triangle_edge_index_buffer = 0u;
    }
    if (resource->triangle_edge_adjacency_index_buffer != 0u)
    {
        gl->glDeleteBuffers(1, &resource->triangle_edge_adjacency_index_buffer);
        resource->triangle_edge_adjacency_index_buffer = 0u;
    }
    if (resource->line_index_buffer != 0u)
    {
        gl->glDeleteBuffers(1, &resource->line_index_buffer);
        resource->line_index_buffer = 0u;
    }
    if (resource->line_adjacency_index_buffer != 0u)
    {
        gl->glDeleteBuffers(1, &resource->line_adjacency_index_buffer);
        resource->line_adjacency_index_buffer = 0u;
    }
    if (resource->color_buffer != 0u)
    {
        gl->glDeleteBuffers(1, &resource->color_buffer);
        resource->color_buffer = 0u;
    }
    if (resource->index_buffer != 0u)
    {
        gl->glDeleteBuffers(1, &resource->index_buffer);
        resource->index_buffer = 0u;
    }
    if (resource->normal_buffer != 0u)
    {
        gl->glDeleteBuffers(1, &resource->normal_buffer);
        resource->normal_buffer = 0u;
    }
    if (resource->position_buffer != 0u)
    {
        gl->glDeleteBuffers(1, &resource->position_buffer);
        resource->position_buffer = 0u;
    }
    if (resource->vao != 0u)
    {
        gl->glDeleteVertexArrays(1, &resource->vao);
        resource->vao = 0u;
    }
    fviz_release((FVizMapper*)resource->mapper);
    fviz_release((FVizGlyphMapper*)resource->glyph_mapper);
    resource->has_color = FVIZ_FALSE;
    resource->mapper = NULL;
    resource->poly_data = NULL;
    resource->glyph_mapper = NULL;
    resource->index_count = 0;
    resource->line_index_count = 0;
    resource->line_adjacency_index_count = 0;
    resource->source_line_index_count = 0;
    resource->triangle_edge_index_count = 0;
    resource->triangle_edge_adjacency_index_count = 0;
    resource->point_index_count = 0;
    resource->instance_count = 0;
    resource->instance_buffer_bytes = 0u;
    resource->poly_data_mtime = 0u;
    resource->render_data_mtime = 0u;
    resource->geometry_mtime = 0u;
    resource->topology_mtime = 0u;
    resource->attribute_mtime = 0u;
    resource->color_data_mtime = 0u;
    resource->glyph_mtime = 0u;
    resource->point_count = 0u;
}

static void fviz_gl_upload_buffer(FVizGLDevice* device, GLuint* buffer, GLenum target, const void* data,
                                  GLsizeiptr size)
{
    const FVizGLFunctions* gl = &device->gl;
    if (*buffer == 0u) gl->glGenBuffers(1, buffer);
    gl->glBindBuffer(target, *buffer);
    gl->glBufferData(target, size, data, FVIZ_GL_STATIC_DRAW);
    ++device->frame_statistics.gpu_uploads;
    if (size > 0) device->frame_statistics.gpu_upload_bytes += (uint64_t)size;
}

static void fviz_gl_update_buffer(FVizGLDevice* device, GLuint buffer, GLenum target, const void* data, GLsizeiptr size)
{
    const FVizGLFunctions* gl = &device->gl;
    if (buffer == 0u || data == NULL || size <= 0) return;
    gl->glBindBuffer(target, buffer);
    gl->glBufferSubData(target, 0, size, data);
    ++device->frame_statistics.gpu_uploads;
    device->frame_statistics.gpu_upload_bytes += (uint64_t)size;
}

static void fviz_gl_update_buffer_range(FVizGLDevice* device, GLuint buffer, GLenum target, const void* data,
                                        GLsizeiptr offset, GLsizeiptr size)
{
    const FVizGLFunctions* gl = &device->gl;
    if (buffer == 0u || data == NULL || size <= 0) return;
    gl->glBindBuffer(target, buffer);
    gl->glBufferSubData(target, (GLintptr)offset, size, data);
    ++device->frame_statistics.gpu_uploads;
    device->frame_statistics.gpu_upload_bytes += (uint64_t)size;
}

static FVizResult fviz_gl_upload_glyph_instances(FVizGLDevice* device, FVizGLActorResource* resource,
                                                 const FVizGlyphMapper* glyph_mapper)
{
    const FVizGLFunctions* gl = &device->gl;
    const FVizGlyphInstance* source_instances;
    FVizGLGlyphInstance* gpu_instances;
    FVizSize count;
    FVizSize bytes;
    FVizSize i;
    FVizDirtyRange dirty = {0u, 0u, FVIZ_FALSE};
    FVizBool partial = FVIZ_FALSE;
    if (device == NULL || resource == NULL || glyph_mapper == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    count = fviz_glyph_mapper_instance_count(glyph_mapper);
    source_instances = fviz_glyph_mapper_instances(glyph_mapper);
    if (count == 0u)
    {
        resource->instance_count = 0;
        resource->glyph_mtime = fviz_internal_glyph_mapper_instances_mtime(glyph_mapper);
        return FVIZ_OK;
    }
    if (count > (FVizSize)INT_MAX || source_instances == NULL) return FVIZ_ERROR_OVERFLOW;
    if (resource->instance_buffer != 0u && resource->glyph_mtime != 0u &&
        fviz_internal_glyph_mapper_dirty_range_since(glyph_mapper, resource->glyph_mtime, &dirty) == FVIZ_OK &&
        dirty.full == FVIZ_FALSE && dirty.count != 0u && dirty.first <= count && dirty.count <= count - dirty.first &&
        resource->instance_buffer_bytes >= count * sizeof(*gpu_instances))
        partial = FVIZ_TRUE;
    if (partial != FVIZ_FALSE)
    {
        FVizSize dirty_bytes;
        if (fviz_size_multiply(dirty.count, (FVizSize)sizeof(*gpu_instances), &dirty_bytes) != FVIZ_OK)
            return fviz_last_error_code();
        gpu_instances = (FVizGLGlyphInstance*)fviz_alloc(dirty_bytes);
        if (gpu_instances == NULL) return fviz_last_error_code();
        for (i = 0u; i < dirty.count; ++i)
            fviz_gl_glyph_instance_to_gpu(&source_instances[dirty.first + i], &gpu_instances[i]);
        fviz_gl_update_buffer_range(device, resource->instance_buffer, FVIZ_GL_ARRAY_BUFFER, gpu_instances,
                                    (GLsizeiptr)(dirty.first * sizeof(*gpu_instances)), (GLsizeiptr)dirty_bytes);
        fviz_free(gpu_instances);
        resource->instance_count = (GLsizei)count;
        resource->glyph_mtime = fviz_internal_glyph_mapper_instances_mtime(glyph_mapper);
        return FVIZ_OK;
    }
    if (fviz_size_multiply(count, (FVizSize)sizeof(*gpu_instances), &bytes) != FVIZ_OK) return fviz_last_error_code();
    gpu_instances = (FVizGLGlyphInstance*)fviz_alloc(bytes);
    if (gpu_instances == NULL) return fviz_last_error_code();
    for (i = 0u; i < count; ++i)
        fviz_gl_glyph_instance_to_gpu(&source_instances[i], &gpu_instances[i]);
    if (resource->instance_buffer == 0u) gl->glGenBuffers(1, &resource->instance_buffer);
    gl->glBindBuffer(FVIZ_GL_ARRAY_BUFFER, resource->instance_buffer);
    if (resource->instance_buffer_bytes >= bytes && resource->instance_buffer_bytes != 0u)
        gl->glBufferSubData(FVIZ_GL_ARRAY_BUFFER, 0, (GLsizeiptr)bytes, gpu_instances);
    else
    {
        gl->glBufferData(FVIZ_GL_ARRAY_BUFFER, (GLsizeiptr)bytes, gpu_instances, FVIZ_GL_DYNAMIC_DRAW);
        resource->instance_buffer_bytes = bytes;
    }
    ++device->frame_statistics.gpu_uploads;
    device->frame_statistics.gpu_upload_bytes += (uint64_t)bytes;
    fviz_free(gpu_instances);

    gl->glBindVertexArray(resource->vao);
    gl->glBindBuffer(FVIZ_GL_ARRAY_BUFFER, resource->instance_buffer);
    gl->glVertexAttribPointer(FVIZ_GL_INSTANCE_MATRIX0_ATTRIBUTE_INDEX, 4, GL_FLOAT, GL_FALSE,
                              (GLsizei)sizeof(FVizGLGlyphInstance), (const void*)offsetof(FVizGLGlyphInstance, matrix));
    gl->glVertexAttribPointer(FVIZ_GL_INSTANCE_MATRIX1_ATTRIBUTE_INDEX, 4, GL_FLOAT, GL_FALSE,
                              (GLsizei)sizeof(FVizGLGlyphInstance),
                              (const void*)(offsetof(FVizGLGlyphInstance, matrix) + 4u * sizeof(float)));
    gl->glVertexAttribPointer(FVIZ_GL_INSTANCE_MATRIX2_ATTRIBUTE_INDEX, 4, GL_FLOAT, GL_FALSE,
                              (GLsizei)sizeof(FVizGLGlyphInstance),
                              (const void*)(offsetof(FVizGLGlyphInstance, matrix) + 8u * sizeof(float)));
    gl->glVertexAttribPointer(FVIZ_GL_INSTANCE_MATRIX3_ATTRIBUTE_INDEX, 4, GL_FLOAT, GL_FALSE,
                              (GLsizei)sizeof(FVizGLGlyphInstance),
                              (const void*)(offsetof(FVizGLGlyphInstance, matrix) + 12u * sizeof(float)));
    gl->glVertexAttribPointer(FVIZ_GL_INSTANCE_COLOR_ATTRIBUTE_INDEX, 4, GL_FLOAT, GL_FALSE,
                              (GLsizei)sizeof(FVizGLGlyphInstance), (const void*)offsetof(FVizGLGlyphInstance, color));
    gl->glEnableVertexAttribArray(FVIZ_GL_INSTANCE_MATRIX0_ATTRIBUTE_INDEX);
    gl->glEnableVertexAttribArray(FVIZ_GL_INSTANCE_MATRIX1_ATTRIBUTE_INDEX);
    gl->glEnableVertexAttribArray(FVIZ_GL_INSTANCE_MATRIX2_ATTRIBUTE_INDEX);
    gl->glEnableVertexAttribArray(FVIZ_GL_INSTANCE_MATRIX3_ATTRIBUTE_INDEX);
    gl->glEnableVertexAttribArray(FVIZ_GL_INSTANCE_COLOR_ATTRIBUTE_INDEX);
    gl->glVertexAttribDivisor(FVIZ_GL_INSTANCE_MATRIX0_ATTRIBUTE_INDEX, 1u);
    gl->glVertexAttribDivisor(FVIZ_GL_INSTANCE_MATRIX1_ATTRIBUTE_INDEX, 1u);
    gl->glVertexAttribDivisor(FVIZ_GL_INSTANCE_MATRIX2_ATTRIBUTE_INDEX, 1u);
    gl->glVertexAttribDivisor(FVIZ_GL_INSTANCE_MATRIX3_ATTRIBUTE_INDEX, 1u);
    gl->glVertexAttribDivisor(FVIZ_GL_INSTANCE_COLOR_ATTRIBUTE_INDEX, 1u);
    gl->glBindVertexArray(0u);
    resource->instance_count = (GLsizei)count;
    resource->glyph_mtime = fviz_internal_glyph_mapper_instances_mtime(glyph_mapper);
    return FVIZ_OK;
}

static double fviz_gl_array_component(const FVizDataArray* array, FVizSize tuple_index, uint32_t component)
{
    const unsigned char* tuple = (const unsigned char*)fviz_data_array_const_tuple(array, tuple_index);
    if (tuple == NULL || component >= fviz_data_array_components(array)) return 0.0;
    switch (fviz_data_array_type(array))
    {
        case FVIZ_DATA_INT8:
            return ((const int8_t*)tuple)[component];
        case FVIZ_DATA_UINT8:
            return ((const uint8_t*)tuple)[component];
        case FVIZ_DATA_INT16:
            return ((const int16_t*)tuple)[component];
        case FVIZ_DATA_UINT16:
            return ((const uint16_t*)tuple)[component];
        case FVIZ_DATA_INT32:
            return ((const int32_t*)tuple)[component];
        case FVIZ_DATA_UINT32:
            return ((const uint32_t*)tuple)[component];
        case FVIZ_DATA_INT64:
            return (double)((const int64_t*)tuple)[component];
        case FVIZ_DATA_UINT64:
            return (double)((const uint64_t*)tuple)[component];
        case FVIZ_DATA_FLOAT32:
            return ((const float*)tuple)[component];
        case FVIZ_DATA_FLOAT64:
            return ((const double*)tuple)[component];
        default:
            return 0.0;
    }
}

static double fviz_gl_array_scalar(const FVizDataArray* array, FVizSize tuple_index, FVizComponentMode mode,
                                   uint32_t component)
{
    if (mode == FVIZ_COMPONENT_MAGNITUDE)
    {
        double squared = 0.0;
        uint32_t i;
        for (i = 0u; i < fviz_data_array_components(array); ++i)
        {
            const double value = fviz_gl_array_component(array, tuple_index, i);
            squared += value * value;
        }
        return sqrt(squared);
    }
    return fviz_gl_array_component(array, tuple_index, component);
}

static float fviz_gl_direct_color_component(const FVizDataArray* array, FVizSize tuple_index, uint32_t component)
{
    double value = fviz_gl_array_component(array, tuple_index, component);
    if (fviz_data_array_type(array) == FVIZ_DATA_UINT8) value /= 255.0;
    else if (fviz_data_array_type(array) == FVIZ_DATA_UINT16)
        value /= 65535.0;
    if (value < 0.0) value = 0.0;
    if (value > 1.0) value = 1.0;
    return (float)value;
}

static FVizGLColor fviz_gl_map_tuple_color(FVizMapper* mapper, const FVizDataArray* array, FVizSize tuple_index,
                                           FVizComponentMode mode, uint32_t component)
{
    FVizGLColor color = {1.0f, 1.0f, 1.0f, 1.0f};
    if (mode == FVIZ_COMPONENT_COLOR && fviz_data_array_components(array) >= 3u)
    {
        color.r = fviz_gl_direct_color_component(array, tuple_index, 0u);
        color.g = fviz_gl_direct_color_component(array, tuple_index, 1u);
        color.b = fviz_gl_direct_color_component(array, tuple_index, 2u);
        if (fviz_data_array_components(array) >= 4u) color.a = fviz_gl_direct_color_component(array, tuple_index, 3u);
    }
    else
    {
        const double value = fviz_gl_array_scalar(array, tuple_index, mode, component);
        fviz_lookup_table_map_scalar(fviz_mapper_lookup_table(mapper), (float)value, &color.r, &color.g, &color.b);
    }
    return color;
}

static FVizResult fviz_gl_build_color_buffer(FVizGLDevice* device, FVizGLActorResource* resource,
                                             const FVizActor* actor, const FVizPolyData* poly_data,
                                             FVizSize point_count)
{
    const FVizGLFunctions* gl = &device->gl;
    FVizMapper* mapper = fviz_actor_mapper((FVizActor*)actor);
    const FVizDataArray* scalars;
    const FVizDataArray* opacity_array = NULL;
    FVizArraySelection selection;
    FVizGLColor* colors;
    uint32_t* contributions = NULL;
    FVizSize tuple_count;
    FVizSize i;
    resource->color_array = NULL;
    resource->color_array_mtime = 0u;
    if (mapper == NULL || fviz_mapper_scalar_visibility(mapper) == FVIZ_FALSE) return FVIZ_OK;
    fviz_array_selection_initialize(&selection);
    (void)fviz_mapper_get_array_selection(mapper, &selection);
    scalars = fviz_mapper_selected_array(mapper);
    if (scalars == NULL)
    {
        scalars = fviz_poly_data_const_scalars(poly_data);
        selection.association = FVIZ_ASSOCIATION_POINTS;
        selection.component_mode = FVIZ_COMPONENT_DIRECT;
        selection.component = 0u;
    }
    if (scalars == NULL || point_count == 0u) return FVIZ_OK;
    tuple_count = fviz_data_array_tuple_count(scalars);
    if (selection.component_mode != FVIZ_COMPONENT_COLOR && fviz_mapper_lookup_table(mapper) == NULL) return FVIZ_OK;
    if (selection.component_mode == FVIZ_COMPONENT_DIRECT && selection.component >= fviz_data_array_components(scalars))
        return FVIZ_ERROR_INVALID_ARGUMENT;

    if (selection.component_mode != FVIZ_COMPONENT_COLOR && fviz_mapper_scalar_range_valid(mapper) == FVIZ_FALSE &&
        tuple_count > 0u)
    {
        double minimum = fviz_gl_array_scalar(scalars, 0u, selection.component_mode, selection.component);
        double maximum = minimum;
        for (i = 1u; i < tuple_count; ++i)
        {
            const double value = fviz_gl_array_scalar(scalars, i, selection.component_mode, selection.component);
            if (value < minimum) minimum = value;
            if (value > maximum) maximum = value;
        }
        if (maximum <= minimum) maximum = minimum + 1.0f;
        fviz_mapper_set_scalar_range(mapper, (float)minimum, (float)maximum);
    }

    colors = (FVizGLColor*)fviz_alloc(point_count * sizeof(*colors));
    if (colors == NULL) return fviz_last_error_code();
    if (selection.association == FVIZ_ASSOCIATION_POINTS && tuple_count == point_count)
    {
        for (i = 0u; i < point_count; ++i)
            colors[i] = fviz_gl_map_tuple_color(mapper, scalars, i, selection.component_mode, selection.component);
    }
    else if (selection.association == FVIZ_ASSOCIATION_CELLS && tuple_count == fviz_poly_data_triangle_count(poly_data))
    {
        const uint32_t* indices = fviz_poly_data_triangle_indices(poly_data);
        contributions = (uint32_t*)fviz_alloc(point_count * sizeof(*contributions));
        if (contributions == NULL)
        {
            fviz_free(colors);
            return fviz_last_error_code();
        }
        (void)memset(colors, 0, point_count * sizeof(*colors));
        (void)memset(contributions, 0, point_count * sizeof(*contributions));
        for (i = 0u; i < tuple_count; ++i)
        {
            FVizGLColor cell =
                fviz_gl_map_tuple_color(mapper, scalars, i, selection.component_mode, selection.component);
            uint32_t corner;
            for (corner = 0u; corner < 3u; ++corner)
            {
                const uint32_t point_id = indices[i * 3u + corner];
                colors[point_id].r += cell.r;
                colors[point_id].g += cell.g;
                colors[point_id].b += cell.b;
                colors[point_id].a += cell.a;
                ++contributions[point_id];
            }
        }
        for (i = 0u; i < point_count; ++i)
        {
            if (contributions[i] != 0u)
            {
                const float inverse = 1.0f / (float)contributions[i];
                colors[i].r *= inverse;
                colors[i].g *= inverse;
                colors[i].b *= inverse;
                colors[i].a *= inverse;
            }
        }
    }
    else if (selection.association == FVIZ_ASSOCIATION_FIELD && tuple_count > 0u)
    {
        const FVizGLColor field =
            fviz_gl_map_tuple_color(mapper, scalars, 0u, selection.component_mode, selection.component);
        for (i = 0u; i < point_count; ++i)
            colors[i] = field;
    }
    else
    {
        fviz_free(colors);
        return FVIZ_OK;
    }
    if (fviz_mapper_opacity_array(mapper) != NULL)
    {
        opacity_array =
            fviz_attribute_set_const_get(fviz_poly_data_const_point_data(poly_data), fviz_mapper_opacity_array(mapper));
        if (opacity_array != NULL && fviz_data_array_tuple_count(opacity_array) == point_count)
        {
            for (i = 0u; i < point_count; ++i)
            {
                double opacity = fviz_gl_array_component(opacity_array, i, 0u);
                if (opacity < 0.0) opacity = 0.0;
                if (opacity > 1.0) opacity = 1.0;
                colors[i].a *= (float)opacity;
            }
        }
    }
    fviz_gl_upload_buffer(device, &resource->color_buffer, FVIZ_GL_ARRAY_BUFFER, colors,
                          (GLsizeiptr)(point_count * sizeof(*colors)));
    fviz_free(contributions);
    fviz_free(colors);
    gl->glVertexAttribPointer(FVIZ_GL_COLOR_ATTRIBUTE_INDEX, 4, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(FVizGLColor),
                              (const void*)0);
    gl->glEnableVertexAttribArray(FVIZ_GL_COLOR_ATTRIBUTE_INDEX);
    resource->has_color = FVIZ_TRUE;
    resource->color_array = scalars;
    resource->color_array_mtime = fviz_object_mtime((const FVizObject*)scalars);
    return FVIZ_OK;
}

static FVizResult fviz_gl_update_point_color_range(FVizGLDevice* device, FVizGLActorResource* resource,
                                                   FVizMapper* mapper, const FVizDataArray* scalars,
                                                   const FVizArraySelection* selection, const FVizDirtyRange* range)
{
    const FVizGLFunctions* gl = &device->gl;
    FVizGLColor* colors;
    FVizSize i;
    FVizSize bytes;
    FVizSize offset;
    if (range->count == 0u) return FVIZ_OK;
    if (fviz_size_multiply(range->count, sizeof(*colors), &bytes) != FVIZ_OK ||
        fviz_size_multiply(range->first, sizeof(*colors), &offset) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    colors = (FVizGLColor*)fviz_alloc(bytes);
    if (colors == NULL) return fviz_last_error_code();
    for (i = 0u; i < range->count; ++i)
        colors[i] =
            fviz_gl_map_tuple_color(mapper, scalars, range->first + i, selection->component_mode, selection->component);
    gl->glBindBuffer(FVIZ_GL_ARRAY_BUFFER, resource->color_buffer);
    gl->glBufferSubData(FVIZ_GL_ARRAY_BUFFER, (GLintptr)offset, (GLsizeiptr)bytes, colors);
    ++device->frame_statistics.gpu_uploads;
    device->frame_statistics.gpu_upload_bytes += (uint64_t)bytes;
    fviz_free(colors);
    resource->color_array_mtime = fviz_object_mtime((const FVizObject*)scalars);
    return FVIZ_OK;
}

static FVizResult fviz_gl_polyline_index_count(const FVizPolyData* poly_data, FVizSize* out_index_count)
{
    const FVizCellArray* lines;
    FVizSize count = 0u;
    FVizSize cell;
    if (poly_data == NULL || out_index_count == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    lines = fviz_poly_data_lines(poly_data);
    for (cell = 0u; lines != NULL && cell < fviz_cell_array_count(lines); ++cell)
    {
        const FVizSize point_count = fviz_cell_array_point_count(lines, cell);
        FVizSize added;
        if (point_count < 2u) continue;
        if (fviz_size_multiply(point_count - 1u, 2u, &added) != FVIZ_OK || added > (FVizSize)-1 - count)
            return FVIZ_ERROR_OVERFLOW;
        count += added;
    }
    *out_index_count = count;
    return FVIZ_OK;
}

static FVizSize fviz_gl_append_polyline_indices(const FVizPolyData* poly_data, uint32_t* destination,
                                                FVizSize destination_index)
{
    const FVizCellArray* lines = fviz_poly_data_lines(poly_data);
    FVizSize cell;
    for (cell = 0u; lines != NULL && cell < fviz_cell_array_count(lines); ++cell)
    {
        const FVizSize point_count = fviz_cell_array_point_count(lines, cell);
        const uint32_t* ids = fviz_cell_array_point_ids(lines, cell);
        FVizSize point;
        if (ids == NULL || point_count < 2u) continue;
        for (point = 0u; point + 1u < point_count; ++point)
        {
            destination[destination_index++] = ids[point];
            destination[destination_index++] = ids[point + 1u];
        }
    }
    return destination_index;
}

static FVizSize fviz_gl_append_polyline_adjacency_indices(const FVizPolyData* poly_data, uint32_t* destination,
                                                          FVizSize destination_index)
{
    const FVizCellArray* lines = fviz_poly_data_lines(poly_data);
    FVizSize cell;
    for (cell = 0u; lines != NULL && cell < fviz_cell_array_count(lines); ++cell)
    {
        const FVizSize point_count = fviz_cell_array_point_count(lines, cell);
        const uint32_t* ids = fviz_cell_array_point_ids(lines, cell);
        FVizSize point;
        if (ids == NULL || point_count < 2u) continue;
        for (point = 0u; point + 1u < point_count; ++point)
        {
            destination[destination_index++] = point == 0u ? ids[point] : ids[point - 1u];
            destination[destination_index++] = ids[point];
            destination[destination_index++] = ids[point + 1u];
            destination[destination_index++] = point + 2u < point_count ? ids[point + 2u] : ids[point + 1u];
        }
    }
    return destination_index;
}

static FVizResult fviz_gl_upload_triangle_edges(FVizGLDevice* device, FVizGLActorResource* resource,
                                                const FVizPolyData* poly_data)
{
    const uint32_t* indices;
    FVizSize triangle_count;
    FVizSize edge_index_count;
    FVizSize adjacency_index_count;
    uint32_t* edges;
    uint32_t* adjacency;
    FVizSize triangle;
    FVizSize out = 0u;
    FVizSize adjacency_out = 0u;
    if (device == NULL || resource == NULL || poly_data == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (resource->triangle_edge_index_buffer != 0u && resource->triangle_edge_adjacency_index_buffer != 0u)
        return FVIZ_OK;
    triangle_count = fviz_poly_data_triangle_count(poly_data);
    if (triangle_count == 0u) return FVIZ_OK;
    if (fviz_size_multiply(triangle_count, 6u, &edge_index_count) != FVIZ_OK ||
        fviz_size_multiply(triangle_count, 12u, &adjacency_index_count) != FVIZ_OK ||
        adjacency_index_count > (FVizSize)INT_MAX)
        return FVIZ_ERROR_OVERFLOW;
    indices = fviz_poly_data_triangle_indices(poly_data);
    if (indices == NULL) return FVIZ_OK;
    edges = (uint32_t*)fviz_alloc(edge_index_count * sizeof(*edges));
    adjacency = (uint32_t*)fviz_alloc(adjacency_index_count * sizeof(*adjacency));
    if (edges == NULL || adjacency == NULL)
    {
        fviz_free(edges);
        fviz_free(adjacency);
        return fviz_last_error_code();
    }
    for (triangle = 0u; triangle < triangle_count; ++triangle)
    {
        const uint32_t a = indices[triangle * 3u + 0u];
        const uint32_t b = indices[triangle * 3u + 1u];
        const uint32_t c = indices[triangle * 3u + 2u];
        edges[out++] = a;
        edges[out++] = b;
        edges[out++] = b;
        edges[out++] = c;
        edges[out++] = c;
        edges[out++] = a;
        adjacency[adjacency_out++] = a;
        adjacency[adjacency_out++] = a;
        adjacency[adjacency_out++] = b;
        adjacency[adjacency_out++] = b;
        adjacency[adjacency_out++] = b;
        adjacency[adjacency_out++] = b;
        adjacency[adjacency_out++] = c;
        adjacency[adjacency_out++] = c;
        adjacency[adjacency_out++] = c;
        adjacency[adjacency_out++] = c;
        adjacency[adjacency_out++] = a;
        adjacency[adjacency_out++] = a;
    }
    fviz_gl_upload_buffer(device, &resource->triangle_edge_index_buffer, FVIZ_GL_ELEMENT_ARRAY_BUFFER, edges,
                          (GLsizeiptr)(edge_index_count * sizeof(*edges)));
    fviz_gl_upload_buffer(device, &resource->triangle_edge_adjacency_index_buffer, FVIZ_GL_ELEMENT_ARRAY_BUFFER,
                          adjacency, (GLsizeiptr)(adjacency_index_count * sizeof(*adjacency)));
    fviz_free(edges);
    fviz_free(adjacency);
    resource->triangle_edge_index_count = (GLsizei)edge_index_count;
    resource->triangle_edge_adjacency_index_count = (GLsizei)adjacency_index_count;
    return FVIZ_OK;
}

static FVizResult fviz_gl_ensure_actor_resource(FVizGLDevice* device, const FVizActor* actor)
{
    const FVizGlyphMapper* glyph_mapper;
    const FVizPolyData* poly_data;
    const FVizVec3* points;
    const FVizVec3* normals;
    const uint32_t* indices;
    const uint32_t* point_indices = NULL;
    const FVizGLFunctions* gl = &device->gl;
    FVizGLActorResource* resource;
    FVizSize point_count;
    FVizSize index_count;
    FVizSize line_index_count;
    FVizSize source_line_index_count = 0u;
    FVizSize triangle_edge_index_count = 0u;
    FVizSize point_index_count = 0u;
    FVizSize glyph_instance_count = 0u;
    FVizMTime poly_data_mtime;
    FVizMTime render_data_mtime;
    FVizMTime geometry_mtime;
    FVizMTime topology_mtime;
    FVizMTime attribute_mtime;
    FVizMTime color_data_mtime;
    FVizMTime glyph_mtime = 0u;
    FVizMapper* mapper;

    if (device == NULL || actor == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    glyph_mapper = fviz_actor_const_glyph_mapper(actor);
    poly_data = fviz_gl_actor_render_poly_data(actor);
    if (poly_data == NULL) return FVIZ_OK;
    if (glyph_mapper != NULL)
    {
        glyph_instance_count = fviz_glyph_mapper_instance_count(glyph_mapper);
        if (glyph_instance_count == 0u) return FVIZ_OK;
        glyph_mtime = fviz_internal_glyph_mapper_instances_mtime(glyph_mapper);
    }
    point_count = fviz_poly_data_point_count(poly_data);
    if (fviz_size_multiply(fviz_poly_data_triangle_count(poly_data), 3u, &index_count) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    if (fviz_gl_polyline_index_count(poly_data, &source_line_index_count) != FVIZ_OK) return fviz_last_error_code();
    if (fviz_size_multiply(fviz_poly_data_triangle_count(poly_data), 6u, &triangle_edge_index_count) != FVIZ_OK)
        return FVIZ_ERROR_OVERFLOW;
    line_index_count = source_line_index_count;
    {
        const FVizCellArray* verts = fviz_poly_data_verts(poly_data);
        if (verts != NULL)
        {
            point_index_count = fviz_cell_array_connectivity_size(verts);
            point_indices = fviz_cell_array_connectivity(verts);
        }
    }
    mapper = fviz_actor_mapper((FVizActor*)actor);
    geometry_mtime = fviz_poly_data_geometry_mtime(poly_data);
    topology_mtime = fviz_poly_data_topology_mtime(poly_data);
    attribute_mtime = fviz_poly_data_attribute_mtime(poly_data);
    color_data_mtime = glyph_mapper == NULL ? fviz_internal_mapper_color_data_mtime(mapper) : 0u;
    /* Non-glyph rendering uses fine-grained PolyData revisions below and does
       not need to walk every child object to compute aggregate MTime. */
    if (glyph_mapper == NULL)
    {
        poly_data_mtime = geometry_mtime;
        if (topology_mtime > poly_data_mtime) poly_data_mtime = topology_mtime;
        if (attribute_mtime > poly_data_mtime) poly_data_mtime = attribute_mtime;
    }
    else
    {
        poly_data_mtime = fviz_object_mtime((const FVizObject*)poly_data);
    }
    render_data_mtime = glyph_mapper == NULL ? fviz_internal_mapper_render_data_mtime(mapper) : 0u;
    if (point_count == 0u || (index_count == 0u && line_index_count == 0u && point_index_count == 0u)) return FVIZ_OK;
    if (point_count > (FVizSize)INT_MAX || index_count > (FVizSize)INT_MAX || line_index_count > (FVizSize)INT_MAX ||
        triangle_edge_index_count > (FVizSize)INT_MAX || point_index_count > (FVizSize)INT_MAX ||
        glyph_instance_count > (FVizSize)INT_MAX)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "poly_data draw count exceeds OpenGL limits");
        return FVIZ_ERROR_OVERFLOW;
    }

    resource = fviz_gl_find_actor_resource(device, actor);
    if (resource != NULL && resource->poly_data == poly_data && resource->glyph_mapper == glyph_mapper &&
        (glyph_mapper != NULL || resource->mapper == mapper))
    {
        if (glyph_mapper == NULL && resource->topology_mtime == topology_mtime &&
            resource->point_count == point_count && resource->index_count == (GLsizei)index_count &&
            resource->line_index_count == (GLsizei)line_index_count &&
            resource->point_index_count == (GLsizei)point_index_count)
        {
            const FVizBool geometry_changed = resource->geometry_mtime != geometry_mtime ? FVIZ_TRUE : FVIZ_FALSE;
            const FVizBool color_changed =
                (resource->attribute_mtime != attribute_mtime || resource->color_data_mtime != color_data_mtime)
                    ? FVIZ_TRUE
                    : FVIZ_FALSE;
            if (geometry_changed != FVIZ_FALSE)
            {
                FVizDirtyRange dirty;
                const FVizBool has_partial_range =
                    fviz_poly_data_geometry_dirty_range_since(poly_data, resource->geometry_mtime, &dirty) == FVIZ_OK &&
                            dirty.full == FVIZ_FALSE && dirty.first <= point_count &&
                            dirty.count <= point_count - dirty.first
                        ? FVIZ_TRUE
                        : FVIZ_FALSE;
                points = fviz_poly_data_points(poly_data);
                normals = fviz_poly_data_normals(poly_data);
                if (points != NULL)
                {
                    if (has_partial_range != FVIZ_FALSE)
                        fviz_gl_update_buffer_range(device, resource->position_buffer, FVIZ_GL_ARRAY_BUFFER,
                                                    points + dirty.first, (GLsizeiptr)(dirty.first * sizeof(FVizVec3)),
                                                    (GLsizeiptr)(dirty.count * sizeof(FVizVec3)));
                    else
                        fviz_gl_update_buffer(device, resource->position_buffer, FVIZ_GL_ARRAY_BUFFER, points,
                                              (GLsizeiptr)(point_count * sizeof(FVizVec3)));
                }
                if (normals != NULL)
                {
                    if (has_partial_range != FVIZ_FALSE)
                        fviz_gl_update_buffer_range(device, resource->normal_buffer, FVIZ_GL_ARRAY_BUFFER,
                                                    normals + dirty.first, (GLsizeiptr)(dirty.first * sizeof(FVizVec3)),
                                                    (GLsizeiptr)(dirty.count * sizeof(FVizVec3)));
                    else
                        fviz_gl_update_buffer(device, resource->normal_buffer, FVIZ_GL_ARRAY_BUFFER, normals,
                                              (GLsizeiptr)(point_count * sizeof(FVizVec3)));
                }
            }
            if (color_changed != FVIZ_FALSE)
            {
                FVizResult color_result = FVIZ_OK;
                FVizBool partial_update = FVIZ_FALSE;
                FVizArraySelection selection;
                const FVizDataArray* selected_array;
                fviz_array_selection_initialize(&selection);
                (void)fviz_mapper_get_array_selection(mapper, &selection);
                selected_array = fviz_mapper_selected_array(mapper);
                if (selected_array == NULL)
                {
                    selected_array = fviz_poly_data_const_scalars(poly_data);
                    selection.association = FVIZ_ASSOCIATION_POINTS;
                    selection.component_mode = FVIZ_COMPONENT_DIRECT;
                    selection.component = 0u;
                }
                if (resource->color_buffer != 0u && resource->has_color != FVIZ_FALSE &&
                    resource->color_data_mtime == color_data_mtime && resource->color_array == selected_array &&
                    selected_array != NULL && selection.association == FVIZ_ASSOCIATION_POINTS &&
                    fviz_data_array_tuple_count(selected_array) == point_count &&
                    fviz_mapper_opacity_array(mapper) == NULL)
                {
                    FVizDirtyRange dirty;
                    if (fviz_data_array_dirty_range_since(selected_array, resource->color_array_mtime, &dirty) ==
                            FVIZ_OK &&
                        dirty.full == FVIZ_FALSE && dirty.first <= point_count &&
                        dirty.count <= point_count - dirty.first)
                    {
                        color_result = fviz_gl_update_point_color_range(device, resource, mapper, selected_array,
                                                                        &selection, &dirty);
                        if (color_result != FVIZ_OK) return color_result;
                        partial_update = FVIZ_TRUE;
                    }
                }
                if (partial_update == FVIZ_FALSE)
                {
                    gl->glBindVertexArray(resource->vao);
                    gl->glDisableVertexAttribArray(FVIZ_GL_COLOR_ATTRIBUTE_INDEX);
                    resource->has_color = FVIZ_FALSE;
                    color_result = fviz_gl_build_color_buffer(device, resource, actor, poly_data, point_count);
                    gl->glBindVertexArray(0u);
                }
                if (color_result != FVIZ_OK) return color_result;
                /* Automatic scalar-range discovery can modify the mapper while
                   the color buffer is being rebuilt, so sample the final key. */
                color_data_mtime = fviz_internal_mapper_color_data_mtime(mapper);
                render_data_mtime = fviz_internal_mapper_render_data_mtime(mapper);
            }
            if ((fviz_actor_edge_visibility(actor) != FVIZ_FALSE || fviz_actor_wireframe(actor) != FVIZ_FALSE) &&
                resource->triangle_edge_index_buffer == 0u)
            {
                FVizResult edge_result = fviz_gl_upload_triangle_edges(device, resource, poly_data);
                if (edge_result != FVIZ_OK) return edge_result;
            }
            resource->poly_data_mtime = poly_data_mtime;
            resource->render_data_mtime = render_data_mtime;
            resource->geometry_mtime = geometry_mtime;
            resource->topology_mtime = topology_mtime;
            resource->attribute_mtime = attribute_mtime;
            resource->color_data_mtime = color_data_mtime;
            resource->last_seen_frame = device->frame_serial;
            return FVIZ_OK;
        }
        if (glyph_mapper != NULL && resource->poly_data_mtime == poly_data_mtime &&
            resource->render_data_mtime == render_data_mtime)
        {
            if (resource->glyph_mtime != glyph_mtime || resource->instance_count != (GLsizei)glyph_instance_count)
            {
                FVizResult instance_result = fviz_gl_upload_glyph_instances(device, resource, glyph_mapper);
                if (instance_result != FVIZ_OK) return instance_result;
            }
            if ((fviz_actor_edge_visibility(actor) != FVIZ_FALSE || fviz_actor_wireframe(actor) != FVIZ_FALSE) &&
                resource->triangle_edge_index_buffer == 0u)
            {
                FVizResult edge_result = fviz_gl_upload_triangle_edges(device, resource, poly_data);
                if (edge_result != FVIZ_OK) return edge_result;
            }
            resource->last_seen_frame = device->frame_serial;
            return FVIZ_OK;
        }
    }

    points = fviz_poly_data_points(poly_data);
    normals = fviz_poly_data_normals(poly_data);
    indices = fviz_poly_data_triangle_indices(poly_data);
    if (points == NULL) return FVIZ_OK;

    if (resource == NULL)
    {
        if (device->actor_count == device->actor_capacity)
        {
            FVizSize new_capacity;
            FVizGLActorResource* new_actors;
            if (device->actor_capacity == 0u) new_capacity = 4u;
            else
            {
                if (device->actor_capacity > SIZE_MAX / 2u) return FVIZ_ERROR_OVERFLOW;
                new_capacity = device->actor_capacity * 2u;
            }
            new_actors = (FVizGLActorResource*)fviz_realloc(device->actors, new_capacity * sizeof(FVizGLActorResource));
            if (new_actors == NULL) return fviz_last_error_code();
            device->actors = new_actors;
            device->actor_capacity = new_capacity;
        }
        resource = &device->actors[device->actor_count++];
        (void)memset(resource, 0, sizeof(*resource));
    }
    else
    {
        fviz_gl_actor_resource_destroy(device, resource);
    }
    resource->mapper = glyph_mapper == NULL ? (const FVizMapper*)fviz_retain(mapper) : NULL;
    resource->glyph_mapper =
        glyph_mapper != NULL ? (const FVizGlyphMapper*)fviz_retain((FVizGlyphMapper*)glyph_mapper) : NULL;

    gl->glGenVertexArrays(1, &resource->vao);
    gl->glBindVertexArray(resource->vao);

    fviz_gl_upload_buffer(device, &resource->position_buffer, FVIZ_GL_ARRAY_BUFFER, points,
                          (GLsizeiptr)(point_count * sizeof(FVizVec3)));
    gl->glVertexAttribPointer(FVIZ_GL_POSITION_ATTRIBUTE_INDEX, 3, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(FVizVec3),
                              (const void*)0);
    gl->glEnableVertexAttribArray(FVIZ_GL_POSITION_ATTRIBUTE_INDEX);

    if (normals != NULL)
    {
        fviz_gl_upload_buffer(device, &resource->normal_buffer, FVIZ_GL_ARRAY_BUFFER, normals,
                              (GLsizeiptr)(point_count * sizeof(FVizVec3)));
    }
    else
    {
        fviz_gl_upload_buffer(device, &resource->normal_buffer, FVIZ_GL_ARRAY_BUFFER, NULL,
                              (GLsizeiptr)(point_count * sizeof(FVizVec3)));
    }
    gl->glVertexAttribPointer(FVIZ_GL_NORMAL_ATTRIBUTE_INDEX, 3, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(FVizVec3),
                              (const void*)0);
    gl->glEnableVertexAttribArray(FVIZ_GL_NORMAL_ATTRIBUTE_INDEX);

    if (glyph_mapper == NULL) (void)fviz_gl_build_color_buffer(device, resource, actor, poly_data, point_count);

    if (indices != NULL && index_count > 0u)
    {
        fviz_gl_upload_buffer(device, &resource->index_buffer, FVIZ_GL_ELEMENT_ARRAY_BUFFER, indices,
                              (GLsizeiptr)(index_count * sizeof(uint32_t)));
    }
    if (point_index_count > 0u && point_indices != NULL)
    {
        fviz_gl_upload_buffer(device, &resource->point_index_buffer, FVIZ_GL_ELEMENT_ARRAY_BUFFER, point_indices,
                              (GLsizeiptr)(point_index_count * sizeof(uint32_t)));
    }
    if (line_index_count > 0u)
    {
        uint32_t* render_lines;
        uint32_t* adjacency_lines;
        FVizSize line_bytes;
        FVizSize adjacency_count;
        FVizSize adjacency_bytes;
        FVizSize render_index = 0u;
        FVizSize adjacency_index = 0u;
        if (fviz_size_multiply(line_index_count, sizeof(*render_lines), &line_bytes) != FVIZ_OK ||
            fviz_size_multiply(line_index_count, 2u, &adjacency_count) != FVIZ_OK ||
            fviz_size_multiply(adjacency_count, sizeof(*adjacency_lines), &adjacency_bytes) != FVIZ_OK)
            return FVIZ_ERROR_OVERFLOW;
        render_lines = (uint32_t*)fviz_alloc(line_bytes);
        adjacency_lines = (uint32_t*)fviz_alloc(adjacency_bytes);
        if (render_lines == NULL || adjacency_lines == NULL)
        {
            fviz_free(render_lines);
            fviz_free(adjacency_lines);
            return fviz_last_error_code();
        }
        render_index = fviz_gl_append_polyline_indices(poly_data, render_lines, render_index);
        adjacency_index = fviz_gl_append_polyline_adjacency_indices(poly_data, adjacency_lines, adjacency_index);
        if (render_index != line_index_count || adjacency_index != adjacency_count)
        {
            fviz_free(render_lines);
            fviz_free(adjacency_lines);
            fviz_internal_set_error(FVIZ_ERROR_INTERNAL, "line topology expansion count mismatch");
            return FVIZ_ERROR_INTERNAL;
        }
        fviz_gl_upload_buffer(device, &resource->line_index_buffer, FVIZ_GL_ELEMENT_ARRAY_BUFFER, render_lines,
                              (GLsizeiptr)line_bytes);
        fviz_gl_upload_buffer(device, &resource->line_adjacency_index_buffer, FVIZ_GL_ELEMENT_ARRAY_BUFFER,
                              adjacency_lines, (GLsizeiptr)adjacency_bytes);
        resource->line_adjacency_index_count = (GLsizei)adjacency_count;
        fviz_free(render_lines);
        fviz_free(adjacency_lines);
    }
    gl->glBindVertexArray(0u);

    resource->index_count = (GLsizei)index_count;
    resource->line_index_count = (GLsizei)line_index_count;
    resource->source_line_index_count = (GLsizei)source_line_index_count;
    resource->triangle_edge_index_count = 0;
    resource->triangle_edge_adjacency_index_count = 0;
    resource->point_index_count = (GLsizei)point_index_count;
    resource->point_count = point_count;
    resource->poly_data = poly_data;
    resource->poly_data_mtime = poly_data_mtime;
    resource->geometry_mtime = geometry_mtime;
    resource->topology_mtime = topology_mtime;
    resource->attribute_mtime = attribute_mtime;
    if (glyph_mapper == NULL)
    {
        resource->color_data_mtime = fviz_internal_mapper_color_data_mtime(mapper);
        resource->render_data_mtime = fviz_internal_mapper_render_data_mtime(mapper);
    }
    else
    {
        resource->color_data_mtime = 0u;
        resource->render_data_mtime = render_data_mtime;
    }
    if ((fviz_actor_edge_visibility(actor) != FVIZ_FALSE || fviz_actor_wireframe(actor) != FVIZ_FALSE) &&
        triangle_edge_index_count > 0u)
    {
        FVizResult edge_result = fviz_gl_upload_triangle_edges(device, resource, poly_data);
        if (edge_result != FVIZ_OK) return edge_result;
    }
    if (glyph_mapper != NULL)
    {
        FVizResult instance_result = fviz_gl_upload_glyph_instances(device, resource, glyph_mapper);
        if (instance_result != FVIZ_OK) return instance_result;
    }
    resource->last_seen_frame = device->frame_serial;
    return FVIZ_OK;
}

static GLuint fviz_gl_compile_shader(const FVizGLFunctions* gl, GLenum shader_type, const char* source, char* info_log,
                                     FVizSize info_log_size)
{
    GLuint shader;
    GLint status = GL_FALSE;
    shader = gl->glCreateShader(shader_type);
    if (shader == 0u) return 0u;
    gl->glShaderSource(shader, 1, &source, NULL);
    gl->glCompileShader(shader);
    gl->glGetShaderiv(shader, FVIZ_GL_COMPILE_STATUS, &status);
    if (status != GL_TRUE)
    {
        if (info_log != NULL && info_log_size > 0u)
        {
            gl->glGetShaderInfoLog(shader, (GLsizei)info_log_size, NULL, info_log);
        }
        gl->glDeleteShader(shader);
        return 0u;
    }
    return shader;
}

static FVizResult fviz_gl_create_program(FVizGLDevice* device)
{
    const FVizGLFunctions* gl = &device->gl;
    GLuint vertex_shader;
    GLuint fragment_shader;
    char info_log[2048];
    GLint status = GL_FALSE;

    vertex_shader =
        fviz_gl_compile_shader(gl, FVIZ_GL_VERTEX_SHADER, k_fviz_gl_vertex_shader_source, info_log, sizeof(info_log));
    if (vertex_shader == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "FEAViz vertex shader failed to compile");
        return FVIZ_ERROR_GRAPHICS;
    }
    fragment_shader = fviz_gl_compile_shader(gl, FVIZ_GL_FRAGMENT_SHADER, k_fviz_gl_fragment_shader_source, info_log,
                                             sizeof(info_log));
    if (fragment_shader == 0u)
    {
        gl->glDeleteShader(vertex_shader);
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "FEAViz fragment shader failed to compile");
        return FVIZ_ERROR_GRAPHICS;
    }

    device->program = gl->glCreateProgram();
    if (device->program == 0u)
    {
        gl->glDeleteShader(vertex_shader);
        gl->glDeleteShader(fragment_shader);
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "failed to create FEAViz shader program");
        return FVIZ_ERROR_GRAPHICS;
    }
    gl->glAttachShader(device->program, vertex_shader);
    gl->glAttachShader(device->program, fragment_shader);
    gl->glLinkProgram(device->program);
    gl->glGetProgramiv(device->program, FVIZ_GL_LINK_STATUS, &status);
    gl->glDeleteShader(vertex_shader);
    gl->glDeleteShader(fragment_shader);
    if (status != GL_TRUE)
    {
        gl->glGetProgramInfoLog(device->program, (GLsizei)sizeof(info_log), NULL, info_log);
        gl->glDeleteProgram(device->program);
        device->program = 0u;
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "FEAViz shader program failed to link");
        return FVIZ_ERROR_GRAPHICS;
    }

    device->mvp_location = gl->glGetUniformLocation(device->program, "uMvp");
    device->model_location = gl->glGetUniformLocation(device->program, "uModel");
    device->normal_matrix_location = gl->glGetUniformLocation(device->program, "uNormalMatrix");
    device->instancing_location = gl->glGetUniformLocation(device->program, "uInstancingEnabled");
    device->diffuse_location = gl->glGetUniformLocation(device->program, "uDiffuse");
    device->light_count_location = gl->glGetUniformLocation(device->program, "uLightCount");
    device->light_position_intensity_location = gl->glGetUniformLocation(device->program, "uLightPositionIntensity[0]");
    device->light_color_location = gl->glGetUniformLocation(device->program, "uLightColor[0]");
    device->camera_position_location = gl->glGetUniformLocation(device->program, "uCameraPosition");
    device->ambient_factor_location = gl->glGetUniformLocation(device->program, "uAmbientFactor");
    device->diffuse_factor_location = gl->glGetUniformLocation(device->program, "uDiffuseFactor");
    device->specular_factor_location = gl->glGetUniformLocation(device->program, "uSpecularFactor");
    device->specular_power_location = gl->glGetUniformLocation(device->program, "uSpecularPower");
    device->flat_shading_location = gl->glGetUniformLocation(device->program, "uFlatShading");
    device->scalar_color_location = gl->glGetUniformLocation(device->program, "uScalarColorEnabled");
    device->opacity_location = gl->glGetUniformLocation(device->program, "uOpacity");
    device->clip_plane_count_location = gl->glGetUniformLocation(device->program, "uClipPlaneCount");
    device->clip_planes_location = gl->glGetUniformLocation(device->program, "uClipPlanes");
    device->oit_pass_location = gl->glGetUniformLocation(device->program, "uOITPass");
    device->oit_weight_scale_location = gl->glGetUniformLocation(device->program, "uOITWeightScale");
    device->oit_depth_weight_location = gl->glGetUniformLocation(device->program, "uOITDepthWeight");
    device->oit_minimum_weight_location = gl->glGetUniformLocation(device->program, "uOITMinimumWeight");
    device->oit_alpha_cutoff_location = gl->glGetUniformLocation(device->program, "uOITAlphaCutoff");
    device->peel_enabled_location = gl->glGetUniformLocation(device->program, "uPeelEnabled");
    device->peel_depth_texture_location = gl->glGetUniformLocation(device->program, "uPeelDepthTexture");
    device->peel_screen_width_inv_location = gl->glGetUniformLocation(device->program, "uPeelScreenWidthInv");
    device->peel_screen_height_inv_location = gl->glGetUniformLocation(device->program, "uPeelScreenHeightInv");
    device->peel_supported = FVIZ_TRUE;
    return FVIZ_OK;
}

static FVizResult fviz_gl_create_2d_program(FVizGLDevice* device)
{
    const FVizGLFunctions* gl = &device->gl;
    GLuint vertex_shader;
    GLuint fragment_shader;
    char info_log[2048];
    GLint status = GL_FALSE;
    vertex_shader =
        fviz_gl_compile_shader(gl, FVIZ_GL_VERTEX_SHADER, k_fviz_gl2d_vertex_shader_source, info_log, sizeof(info_log));
    if (vertex_shader == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "FEAViz 2D vertex shader failed to compile");
        return FVIZ_ERROR_GRAPHICS;
    }
    fragment_shader = fviz_gl_compile_shader(gl, FVIZ_GL_FRAGMENT_SHADER, k_fviz_gl2d_fragment_shader_source, info_log,
                                             sizeof(info_log));
    if (fragment_shader == 0u)
    {
        gl->glDeleteShader(vertex_shader);
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "FEAViz 2D fragment shader failed to compile");
        return FVIZ_ERROR_GRAPHICS;
    }
    device->program_2d = gl->glCreateProgram();
    if (device->program_2d == 0u)
    {
        gl->glDeleteShader(vertex_shader);
        gl->glDeleteShader(fragment_shader);
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "failed to create FEAViz 2D shader program");
        return FVIZ_ERROR_GRAPHICS;
    }
    gl->glAttachShader(device->program_2d, vertex_shader);
    gl->glAttachShader(device->program_2d, fragment_shader);
    gl->glLinkProgram(device->program_2d);
    gl->glGetProgramiv(device->program_2d, FVIZ_GL_LINK_STATUS, &status);
    gl->glDeleteShader(vertex_shader);
    gl->glDeleteShader(fragment_shader);
    if (status != GL_TRUE)
    {
        gl->glGetProgramInfoLog(device->program_2d, (GLsizei)sizeof(info_log), NULL, info_log);
        gl->glDeleteProgram(device->program_2d);
        device->program_2d = 0u;
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "FEAViz 2D shader program failed to link");
        return FVIZ_ERROR_GRAPHICS;
    }
    device->mvp_location_2d = gl->glGetUniformLocation(device->program_2d, "uMvp");
    return FVIZ_OK;
}

static FVizResult fviz_gl_create_volume_program(FVizGLDevice* device)
{
    const FVizGLFunctions* gl = &device->gl;
    GLuint vertex_shader;
    GLuint fragment_shader;
    char info_log[2048];
    GLint status = GL_FALSE;
    if (gl->glTexImage3D == NULL || gl->glTexSubImage3D == NULL)
    {
        return FVIZ_ERROR_NOT_SUPPORTED;
    }
    vertex_shader = fviz_gl_compile_shader(gl, FVIZ_GL_VERTEX_SHADER, k_fviz_gl_volume_vertex_shader_source, info_log,
                                           sizeof(info_log));
    if (vertex_shader == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "FEAViz volume vertex shader failed to compile");
        return FVIZ_ERROR_GRAPHICS;
    }
    fragment_shader = fviz_gl_compile_shader(gl, FVIZ_GL_FRAGMENT_SHADER, k_fviz_gl_volume_fragment_shader_source,
                                             info_log, sizeof(info_log));
    if (fragment_shader == 0u)
    {
        gl->glDeleteShader(vertex_shader);
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "FEAViz volume fragment shader failed to compile");
        return FVIZ_ERROR_GRAPHICS;
    }
    device->volume_program = gl->glCreateProgram();
    if (device->volume_program == 0u)
    {
        gl->glDeleteShader(vertex_shader);
        gl->glDeleteShader(fragment_shader);
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "failed to create FEAViz volume shader program");
        return FVIZ_ERROR_GRAPHICS;
    }
    gl->glAttachShader(device->volume_program, vertex_shader);
    gl->glAttachShader(device->volume_program, fragment_shader);
    gl->glLinkProgram(device->volume_program);
    gl->glGetProgramiv(device->volume_program, FVIZ_GL_LINK_STATUS, &status);
    gl->glDeleteShader(vertex_shader);
    gl->glDeleteShader(fragment_shader);
    if (status != GL_TRUE)
    {
        gl->glGetProgramInfoLog(device->volume_program, (GLsizei)sizeof(info_log), NULL, info_log);
        gl->glDeleteProgram(device->volume_program);
        device->volume_program = 0u;
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "FEAViz volume shader program failed to link");
        return FVIZ_ERROR_GRAPHICS;
    }
    device->volume_model_location = gl->glGetUniformLocation(device->volume_program, "uModel");
    device->volume_inv_model_location = gl->glGetUniformLocation(device->volume_program, "uInvModel");
    device->volume_mvp_location = gl->glGetUniformLocation(device->volume_program, "uMvp");
    device->volume_camera_position_location = gl->glGetUniformLocation(device->volume_program, "uCameraPosition");
    device->volume_scalar_texture_location = gl->glGetUniformLocation(device->volume_program, "uScalarTexture");
    device->volume_transfer_texture_location = gl->glGetUniformLocation(device->volume_program, "uTransferTexture");
    device->volume_step_size_location = gl->glGetUniformLocation(device->volume_program, "uStepSize");
    device->volume_scalar_range_location = gl->glGetUniformLocation(device->volume_program, "uScalarRangeMin");
    device->volume_scalar_range_max_location = gl->glGetUniformLocation(device->volume_program, "uScalarRangeMax");
    device->volume_bounds_min_location = gl->glGetUniformLocation(device->volume_program, "uBoundsMin");
    device->volume_bounds_max_location = gl->glGetUniformLocation(device->volume_program, "uBoundsMax");
    device->volume_shading_location = gl->glGetUniformLocation(device->volume_program, "uShading");
    device->volume_light_count_location = gl->glGetUniformLocation(device->volume_program, "uLightCount");
    device->volume_light_position_intensity_location =
        gl->glGetUniformLocation(device->volume_program, "uLightPositionIntensity[0]");
    device->volume_light_color_location = gl->glGetUniformLocation(device->volume_program, "uLightColor[0]");
    device->volume_ambient_location = gl->glGetUniformLocation(device->volume_program, "uAmbientFactor");
    device->volume_diffuse_location = gl->glGetUniformLocation(device->volume_program, "uDiffuseFactor");
    device->volume_specular_location = gl->glGetUniformLocation(device->volume_program, "uSpecularFactor");
    device->volume_specular_power_location = gl->glGetUniformLocation(device->volume_program, "uSpecularPower");
    device->volume_program_ready = FVIZ_TRUE;
    return FVIZ_OK;
}

static FVizResult fviz_gl_create_text_program(FVizGLDevice* device)
{
    const FVizGLFunctions* gl = &device->gl;
    GLuint vs;
    GLuint fs;
    GLint status = GL_FALSE;
    char info_log[2048];
    vs = fviz_gl_compile_shader(gl, FVIZ_GL_VERTEX_SHADER, k_fviz_gl_text_vertex_shader_source, info_log,
                                sizeof(info_log));
    fs = fviz_gl_compile_shader(gl, FVIZ_GL_FRAGMENT_SHADER, k_fviz_gl_text_fragment_shader_source, info_log,
                                sizeof(info_log));
    if (vs == 0u || fs == 0u)
    {
        if (vs != 0u) gl->glDeleteShader(vs);
        if (fs != 0u) gl->glDeleteShader(fs);
        return FVIZ_ERROR_GRAPHICS;
    }
    device->text_program = gl->glCreateProgram();
    if (device->text_program == 0u)
    {
        gl->glDeleteShader(vs);
        gl->glDeleteShader(fs);
        return FVIZ_ERROR_GRAPHICS;
    }
    gl->glAttachShader(device->text_program, vs);
    gl->glAttachShader(device->text_program, fs);
    gl->glLinkProgram(device->text_program);
    gl->glGetProgramiv(device->text_program, FVIZ_GL_LINK_STATUS, &status);
    gl->glDeleteShader(vs);
    gl->glDeleteShader(fs);
    if (status != GL_TRUE)
    {
        gl->glDeleteProgram(device->text_program);
        device->text_program = 0u;
        return FVIZ_ERROR_GRAPHICS;
    }
    device->text_atlas_location = gl->glGetUniformLocation(device->text_program, "uAtlas");
    device->text_solid_location = gl->glGetUniformLocation(device->text_program, "uSolid");
    gl->glGenVertexArrays(1, &device->text_vao);
    gl->glGenBuffers(1, &device->text_vbo);
    glGenTextures(1, &device->text_texture);
    if (device->text_vao == 0u || device->text_vbo == 0u || device->text_texture == 0u) return FVIZ_ERROR_GRAPHICS;
    glBindTexture(GL_TEXTURE_2D, device->text_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    glBindTexture(GL_TEXTURE_2D, 0u);
    return FVIZ_OK;
}

static FVizResult fviz_gl_create_edge_program(FVizGLDevice* device)
{
    const FVizGLFunctions* gl = &device->gl;
    GLuint vs;
    GLuint gs;
    GLuint fs;
    GLint status = GL_FALSE;
    char info_log[2048];
    vs = fviz_gl_compile_shader(gl, FVIZ_GL_VERTEX_SHADER, k_fviz_gl_edge_vertex_shader_source, info_log,
                                sizeof(info_log));
    gs = fviz_gl_compile_shader(gl, FVIZ_GL_GEOMETRY_SHADER, k_fviz_gl_edge_geometry_shader_source, info_log,
                                sizeof(info_log));
    fs = fviz_gl_compile_shader(gl, FVIZ_GL_FRAGMENT_SHADER, k_fviz_gl_edge_fragment_shader_source, info_log,
                                sizeof(info_log));
    if (vs == 0u || gs == 0u || fs == 0u)
    {
        if (vs != 0u) gl->glDeleteShader(vs);
        if (gs != 0u) gl->glDeleteShader(gs);
        if (fs != 0u) gl->glDeleteShader(fs);
        return FVIZ_ERROR_GRAPHICS;
    }
    device->edge_program = gl->glCreateProgram();
    if (device->edge_program == 0u)
    {
        gl->glDeleteShader(vs);
        gl->glDeleteShader(gs);
        gl->glDeleteShader(fs);
        return FVIZ_ERROR_GRAPHICS;
    }
    gl->glAttachShader(device->edge_program, vs);
    gl->glAttachShader(device->edge_program, gs);
    gl->glAttachShader(device->edge_program, fs);
    gl->glLinkProgram(device->edge_program);
    gl->glGetProgramiv(device->edge_program, FVIZ_GL_LINK_STATUS, &status);
    gl->glDeleteShader(vs);
    gl->glDeleteShader(gs);
    gl->glDeleteShader(fs);
    if (status != GL_TRUE)
    {
        gl->glDeleteProgram(device->edge_program);
        device->edge_program = 0u;
        return FVIZ_ERROR_GRAPHICS;
    }
    device->edge_mvp_location = gl->glGetUniformLocation(device->edge_program, "uMvp");
    device->edge_viewport_location = gl->glGetUniformLocation(device->edge_program, "uViewportSize");
    device->edge_width_location = gl->glGetUniformLocation(device->edge_program, "uLineWidth");
    device->edge_depth_bias_location = gl->glGetUniformLocation(device->edge_program, "uLineDepthBias");
    device->edge_color_location = gl->glGetUniformLocation(device->edge_program, "uLineColor");
    device->edge_cap_location = gl->glGetUniformLocation(device->edge_program, "uLineCap");
    device->edge_join_location = gl->glGetUniformLocation(device->edge_program, "uLineJoin");
    device->edge_miter_limit_location = gl->glGetUniformLocation(device->edge_program, "uMiterLimit");
    device->edge_dash_location = gl->glGetUniformLocation(device->edge_program, "uLineDash");
    device->edge_scalar_color_location = gl->glGetUniformLocation(device->edge_program, "uScalarColorEnabled");
    device->edge_instancing_location = gl->glGetUniformLocation(device->edge_program, "uInstancingEnabled");
    return FVIZ_OK;
}

static FVizResult fviz_gl_create_point_program(FVizGLDevice* device)
{
    const FVizGLFunctions* gl = &device->gl;
    GLuint vs;
    GLuint fs;
    GLint status = GL_FALSE;
    char info_log[2048];
    vs = fviz_gl_compile_shader(gl, FVIZ_GL_VERTEX_SHADER, k_fviz_gl_point_vertex_shader_source, info_log,
                                sizeof(info_log));
    fs = fviz_gl_compile_shader(gl, FVIZ_GL_FRAGMENT_SHADER, k_fviz_gl_point_fragment_shader_source, info_log,
                                sizeof(info_log));
    if (vs == 0u || fs == 0u)
    {
        if (vs != 0u) gl->glDeleteShader(vs);
        if (fs != 0u) gl->glDeleteShader(fs);
        return FVIZ_ERROR_GRAPHICS;
    }
    device->point_program = gl->glCreateProgram();
    if (device->point_program == 0u)
    {
        gl->glDeleteShader(vs);
        gl->glDeleteShader(fs);
        return FVIZ_ERROR_GRAPHICS;
    }
    gl->glAttachShader(device->point_program, vs);
    gl->glAttachShader(device->point_program, fs);
    gl->glLinkProgram(device->point_program);
    gl->glGetProgramiv(device->point_program, FVIZ_GL_LINK_STATUS, &status);
    gl->glDeleteShader(vs);
    gl->glDeleteShader(fs);
    if (status != GL_TRUE)
    {
        gl->glDeleteProgram(device->point_program);
        device->point_program = 0u;
        return FVIZ_ERROR_GRAPHICS;
    }
    device->point_mvp_location = gl->glGetUniformLocation(device->point_program, "uMvp");
    device->point_size_location = gl->glGetUniformLocation(device->point_program, "uPointSize");
    device->point_color_location = gl->glGetUniformLocation(device->point_program, "uPointColor");
    device->point_shape_location = gl->glGetUniformLocation(device->point_program, "uPointShape");
    device->point_scalar_color_location = gl->glGetUniformLocation(device->point_program, "uScalarColorEnabled");
    device->point_instancing_location = gl->glGetUniformLocation(device->point_program, "uInstancingEnabled");
    return FVIZ_OK;
}

static FVizResult fviz_gl_create_selection_program(FVizGLDevice* device)
{
    const FVizGLFunctions* gl = &device->gl;
    GLuint vertex_shader;
    GLuint fragment_shader;
    GLint status = GL_FALSE;
    char info_log[2048];
    vertex_shader = fviz_gl_compile_shader(gl, FVIZ_GL_VERTEX_SHADER, k_fviz_gl_selection_vertex_shader_source,
                                           info_log, sizeof(info_log));
    fragment_shader = fviz_gl_compile_shader(gl, FVIZ_GL_FRAGMENT_SHADER, k_fviz_gl_selection_fragment_shader_source,
                                             info_log, sizeof(info_log));
    if (vertex_shader == 0u || fragment_shader == 0u)
    {
        if (vertex_shader != 0u) gl->glDeleteShader(vertex_shader);
        if (fragment_shader != 0u) gl->glDeleteShader(fragment_shader);
        return FVIZ_ERROR_GRAPHICS;
    }
    device->selection_program = gl->glCreateProgram();
    if (device->selection_program == 0u)
    {
        gl->glDeleteShader(vertex_shader);
        gl->glDeleteShader(fragment_shader);
        return FVIZ_ERROR_GRAPHICS;
    }
    gl->glAttachShader(device->selection_program, vertex_shader);
    gl->glAttachShader(device->selection_program, fragment_shader);
    gl->glLinkProgram(device->selection_program);
    gl->glGetProgramiv(device->selection_program, FVIZ_GL_LINK_STATUS, &status);
    gl->glDeleteShader(vertex_shader);
    gl->glDeleteShader(fragment_shader);
    if (status != GL_TRUE) return FVIZ_ERROR_GRAPHICS;
    device->selection_mvp_location = gl->glGetUniformLocation(device->selection_program, "uMvp");
    device->selection_model_location = gl->glGetUniformLocation(device->selection_program, "uModel");
    device->selection_actor_id_location = gl->glGetUniformLocation(device->selection_program, "uActorId");
    device->selection_association_location = gl->glGetUniformLocation(device->selection_program, "uAssociation");
    device->selection_instancing_location = gl->glGetUniformLocation(device->selection_program, "uInstancingEnabled");
    device->selection_clip_plane_count_location =
        gl->glGetUniformLocation(device->selection_program, "uClipPlaneCount");
    device->selection_clip_planes_location = gl->glGetUniformLocation(device->selection_program, "uClipPlanes");
    return FVIZ_OK;
}

static FVizResult fviz_gl_create_oit_program(FVizGLDevice* device)
{
    const FVizGLFunctions* gl = &device->gl;
    GLuint vs;
    GLuint fs;
    GLint status = GL_FALSE;
    char info_log[2048];
    vs = fviz_gl_compile_shader(gl, FVIZ_GL_VERTEX_SHADER, k_fviz_gl_fxaa_vertex_shader_source, info_log,
                                sizeof(info_log));
    fs = fviz_gl_compile_shader(gl, FVIZ_GL_FRAGMENT_SHADER, k_fviz_gl_oit_composite_fragment_shader_source, info_log,
                                sizeof(info_log));
    if (vs == 0u || fs == 0u)
    {
        if (vs != 0u) gl->glDeleteShader(vs);
        if (fs != 0u) gl->glDeleteShader(fs);
        return FVIZ_ERROR_GRAPHICS;
    }
    device->oit_composite_program = gl->glCreateProgram();
    if (device->oit_composite_program == 0u)
    {
        gl->glDeleteShader(vs);
        gl->glDeleteShader(fs);
        return FVIZ_ERROR_GRAPHICS;
    }
    gl->glAttachShader(device->oit_composite_program, vs);
    gl->glAttachShader(device->oit_composite_program, fs);
    gl->glLinkProgram(device->oit_composite_program);
    gl->glGetProgramiv(device->oit_composite_program, FVIZ_GL_LINK_STATUS, &status);
    gl->glDeleteShader(vs);
    gl->glDeleteShader(fs);
    if (status != GL_TRUE)
    {
        gl->glDeleteProgram(device->oit_composite_program);
        device->oit_composite_program = 0u;
        return FVIZ_ERROR_GRAPHICS;
    }
    device->oit_accum_location = gl->glGetUniformLocation(device->oit_composite_program, "uAccum");
    device->oit_reveal_location = gl->glGetUniformLocation(device->oit_composite_program, "uReveal");
    gl->glGenVertexArrays(1, &device->oit_vao);
    if (device->oit_vao == 0u) return FVIZ_ERROR_GRAPHICS;
    gl->glGenFramebuffers(1, &device->oit_framebuffer);
    gl->glGenFramebuffers(1, &device->oit_resolve_framebuffer);
    gl->glGenRenderbuffers(1, &device->oit_color_renderbuffer);
    gl->glGenRenderbuffers(1, &device->oit_depth_renderbuffer);
    glGenTextures(1, &device->oit_accum_texture);
    glGenTextures(1, &device->oit_reveal_texture);
    if (device->oit_framebuffer == 0u || device->oit_resolve_framebuffer == 0u ||
        device->oit_color_renderbuffer == 0u || device->oit_depth_renderbuffer == 0u ||
        device->oit_accum_texture == 0u || device->oit_reveal_texture == 0u)
        return FVIZ_ERROR_GRAPHICS;
    return FVIZ_OK;
}

static FVizResult fviz_gl_create_fxaa_program(FVizGLDevice* device)
{
    const FVizGLFunctions* gl = &device->gl;
    GLuint vs;
    GLuint fs;
    GLint status = GL_FALSE;
    char info_log[2048];
    vs = fviz_gl_compile_shader(gl, FVIZ_GL_VERTEX_SHADER, k_fviz_gl_fxaa_vertex_shader_source, info_log,
                                sizeof(info_log));
    fs = fviz_gl_compile_shader(gl, FVIZ_GL_FRAGMENT_SHADER, k_fviz_gl_fxaa_fragment_shader_source, info_log,
                                sizeof(info_log));
    if (vs == 0u || fs == 0u)
    {
        if (vs != 0u) gl->glDeleteShader(vs);
        if (fs != 0u) gl->glDeleteShader(fs);
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "FEAViz FXAA shader failed to compile");
        return FVIZ_ERROR_GRAPHICS;
    }
    device->fxaa_program = gl->glCreateProgram();
    if (device->fxaa_program == 0u)
    {
        gl->glDeleteShader(vs);
        gl->glDeleteShader(fs);
        return FVIZ_ERROR_GRAPHICS;
    }
    gl->glAttachShader(device->fxaa_program, vs);
    gl->glAttachShader(device->fxaa_program, fs);
    gl->glLinkProgram(device->fxaa_program);
    gl->glGetProgramiv(device->fxaa_program, FVIZ_GL_LINK_STATUS, &status);
    gl->glDeleteShader(vs);
    gl->glDeleteShader(fs);
    if (status != GL_TRUE)
    {
        gl->glDeleteProgram(device->fxaa_program);
        device->fxaa_program = 0u;
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "FEAViz FXAA program failed to link");
        return FVIZ_ERROR_GRAPHICS;
    }
    device->fxaa_color_location = gl->glGetUniformLocation(device->fxaa_program, "uColor");
    device->fxaa_inv_screen_location = gl->glGetUniformLocation(device->fxaa_program, "uInvScreen");
    device->fxaa_edge_threshold_location = gl->glGetUniformLocation(device->fxaa_program, "uEdgeThreshold");
    device->fxaa_edge_threshold_min_location = gl->glGetUniformLocation(device->fxaa_program, "uEdgeThresholdMin");
    device->fxaa_span_max_location = gl->glGetUniformLocation(device->fxaa_program, "uSpanMax");
    gl->glGenVertexArrays(1, &device->fxaa_vao);
    if (device->fxaa_vao == 0u) return FVIZ_ERROR_GRAPHICS;
    glGenTextures(1, &device->fxaa_texture);
    if (device->fxaa_texture == 0u) return FVIZ_ERROR_GRAPHICS;
    glBindTexture(GL_TEXTURE_2D, device->fxaa_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    glBindTexture(GL_TEXTURE_2D, 0u);
    gl->glGenFramebuffers(1, &device->fxaa_framebuffer);
    if (device->fxaa_framebuffer == 0u) return FVIZ_ERROR_GRAPHICS;
    return FVIZ_OK;
}

FVizGLDevice* fviz_internal_gl_device_create(const FVizGLFunctions* functions)
{
    FVizGLDevice* device;
    if (functions == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "functions must not be NULL");
        return NULL;
    }
    device = (FVizGLDevice*)fviz_alloc(sizeof(FVizGLDevice));
    if (device == NULL)
    {
        return NULL;
    }
    (void)memset(device, 0, sizeof(*device));
    device->gl = *functions;
    if (fviz_gl_create_program(device) != FVIZ_OK || fviz_gl_create_2d_program(device) != FVIZ_OK ||
        fviz_gl_create_selection_program(device) != FVIZ_OK)
    {
        fviz_internal_gl_device_destroy(device);
        return NULL;
    }
    if (fviz_gl_create_text_program(device) != FVIZ_OK)
    {
        /* Text annotations are optional; geometry rendering remains available. */
        fviz_clear_last_error();
    }
    if (fviz_gl_create_edge_program(device) != FVIZ_OK)
    {
        /* Shader-expanded lines are optional; legacy GL lines remain fallback. */
        fviz_clear_last_error();
    }
    if (fviz_gl_create_point_program(device) != FVIZ_OK)
    {
        /* Shader point sprites are optional; surface rendering remains usable. */
        fviz_clear_last_error();
    }
    if (device->edge_program == 0u && device->point_program != 0u)
    {
        /* The EDGE stage falls back as a unit.  Do not let an isolated point
           shader intercept that stage and suppress the legacy line path when
           the geometry-shader line program is unavailable. */
        device->gl.glDeleteProgram(device->point_program);
        device->point_program = 0u;
    }
    if (fviz_gl_create_oit_program(device) != FVIZ_OK)
    {
        /* Weighted OIT is optional; sorted alpha remains the portable fallback. */
        fviz_clear_last_error();
    }
    if (fviz_gl_create_fxaa_program(device) != FVIZ_OK)
    {
        /* FXAA is an optional post-process. Keep the modern renderer usable
           even if a driver rejects this shader or framebuffer path. */
        fviz_clear_last_error();
    }
    if (fviz_gl_create_volume_program(device) != FVIZ_OK)
    {
        /* GPU volume ray-casting is optional; the rest of the pipeline stays
           available when a driver rejects the 3D-texture shader. */
        fviz_clear_last_error();
    }
    if (device->gl.glGenQueries != NULL && device->gl.glDeleteQueries != NULL && device->gl.glBeginQuery != NULL &&
        device->gl.glEndQuery != NULL && device->gl.glGetQueryObjectiv != NULL &&
        device->gl.glGetQueryObjectui64v != NULL)
    {
        device->gl.glGenQueries(2, device->gpu_time_queries);
        if (device->gpu_time_queries[0] == 0u || device->gpu_time_queries[1] == 0u)
        {
            if (device->gpu_time_queries[0] != 0u || device->gpu_time_queries[1] != 0u)
                device->gl.glDeleteQueries(2, device->gpu_time_queries);
            device->gpu_time_queries[0] = 0u;
            device->gpu_time_queries[1] = 0u;
        }
    }
    return device;
}

void fviz_internal_gl_device_bind_framebuffer(FVizGLDevice* device, uint32_t framebuffer)
{
    if (device != NULL && device->gl.glBindFramebuffer != NULL)
        device->gl.glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, (GLuint)framebuffer);
}

void fviz_internal_gl_device_capture_state(FVizGLDevice* device, FVizGLStateSnapshot* out_snapshot)
{
    GLboolean depth_write = GL_TRUE;
    GLboolean color_write[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    if (out_snapshot == NULL) return;
    (void)memset(out_snapshot, 0, sizeof(*out_snapshot));
    if (device == NULL) return;
    glGetIntegerv(FVIZ_GL_VIEWPORT, out_snapshot->viewport);
    glGetIntegerv(0x0C10, out_snapshot->scissor_box); /* GL_SCISSOR_BOX */
    glGetIntegerv(FVIZ_GL_FRAMEBUFFER_BINDING, &out_snapshot->framebuffer);
    glGetIntegerv(FVIZ_GL_CURRENT_PROGRAM, &out_snapshot->program);
    glGetIntegerv(FVIZ_GL_VERTEX_ARRAY_BINDING, &out_snapshot->vertex_array);
    glGetIntegerv(GL_DEPTH_FUNC, &out_snapshot->depth_function);
    glGetIntegerv(FVIZ_GL_CULL_FACE_MODE, &out_snapshot->cull_face_mode);
    glGetIntegerv(FVIZ_GL_POLYGON_MODE, out_snapshot->polygon_mode);
    glGetFloatv(FVIZ_GL_LINE_WIDTH, &out_snapshot->line_width);
    glGetFloatv(FVIZ_GL_POINT_SIZE, &out_snapshot->point_size);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
    glGetBooleanv(FVIZ_GL_COLOR_WRITEMASK, color_write);
    out_snapshot->blend_enabled = glIsEnabled(GL_BLEND) != GL_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    out_snapshot->depth_test_enabled = glIsEnabled(GL_DEPTH_TEST) != GL_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    out_snapshot->cull_face_enabled = glIsEnabled(GL_CULL_FACE) != GL_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    out_snapshot->scissor_enabled = glIsEnabled(FVIZ_GL_SCISSOR_TEST) != GL_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    out_snapshot->multisample_enabled = glIsEnabled(FVIZ_GL_MULTISAMPLE) != GL_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    out_snapshot->dither_enabled = glIsEnabled(GL_DITHER) != GL_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    out_snapshot->depth_write = depth_write != GL_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    out_snapshot->color_write[0] = color_write[0] != GL_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    out_snapshot->color_write[1] = color_write[1] != GL_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    out_snapshot->color_write[2] = color_write[2] != GL_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
    out_snapshot->color_write[3] = color_write[3] != GL_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
}

static void fviz_gl_restore_enable(GLenum capability, FVizBool enabled)
{
    if (enabled != FVIZ_FALSE) glEnable(capability);
    else
        glDisable(capability);
}

FVizResult fviz_internal_gl_device_restore_state(FVizGLDevice* device, const FVizGLStateSnapshot* snapshot)
{
    if (device == NULL || snapshot == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    device->gl.glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, (GLuint)snapshot->framebuffer);
    device->gl.glBindVertexArray((GLuint)snapshot->vertex_array);
    device->gl.glUseProgram((GLuint)snapshot->program);
    glViewport(snapshot->viewport[0], snapshot->viewport[1], snapshot->viewport[2], snapshot->viewport[3]);
    glScissor(snapshot->scissor_box[0], snapshot->scissor_box[1], snapshot->scissor_box[2], snapshot->scissor_box[3]);
    glDepthFunc((GLenum)snapshot->depth_function);
    glCullFace((GLenum)snapshot->cull_face_mode);
    glPolygonMode(GL_FRONT_AND_BACK, (GLenum)snapshot->polygon_mode[0]);
    glLineWidth(snapshot->line_width);
    glPointSize(snapshot->point_size);
    glDepthMask(snapshot->depth_write != FVIZ_FALSE ? GL_TRUE : GL_FALSE);
    glColorMask(snapshot->color_write[0] != FVIZ_FALSE ? GL_TRUE : GL_FALSE,
                snapshot->color_write[1] != FVIZ_FALSE ? GL_TRUE : GL_FALSE,
                snapshot->color_write[2] != FVIZ_FALSE ? GL_TRUE : GL_FALSE,
                snapshot->color_write[3] != FVIZ_FALSE ? GL_TRUE : GL_FALSE);
    fviz_gl_restore_enable(GL_BLEND, snapshot->blend_enabled);
    fviz_gl_restore_enable(GL_DEPTH_TEST, snapshot->depth_test_enabled);
    fviz_gl_restore_enable(GL_CULL_FACE, snapshot->cull_face_enabled);
    fviz_gl_restore_enable(FVIZ_GL_SCISSOR_TEST, snapshot->scissor_enabled);
    fviz_gl_restore_enable(FVIZ_GL_MULTISAMPLE, snapshot->multisample_enabled);
    fviz_gl_restore_enable(GL_DITHER, snapshot->dither_enabled);
    if (device->frame_statistics.custom_pass_state_restorations != UINT64_MAX)
        ++device->frame_statistics.custom_pass_state_restorations;
    return glGetError() == GL_NO_ERROR ? FVIZ_OK : FVIZ_ERROR_GRAPHICS;
}

FVizBool fviz_internal_gl_device_fxaa_supported(const FVizGLDevice* device)
{
    return device != NULL && device->fxaa_program != 0u && device->fxaa_texture != 0u && device->fxaa_vao != 0u &&
                   device->fxaa_framebuffer != 0u
               ? FVIZ_TRUE
               : FVIZ_FALSE;
}

FVizBool fviz_internal_gl_device_weighted_oit_supported(const FVizGLDevice* device)
{
    return device != NULL && device->oit_composite_program != 0u && device->oit_framebuffer != 0u &&
                   device->oit_resolve_framebuffer != 0u && device->oit_color_renderbuffer != 0u &&
                   device->oit_depth_renderbuffer != 0u && device->oit_accum_texture != 0u &&
                   device->oit_reveal_texture != 0u && device->oit_vao != 0u
               ? FVIZ_TRUE
               : FVIZ_FALSE;
}

FVizBool fviz_internal_gl_device_depth_peeling_supported(const FVizGLDevice* device)
{
    return device != NULL && device->peel_supported != FVIZ_FALSE && device->peel_enabled_location >= 0 ? FVIZ_TRUE
                                                                                                        : FVIZ_FALSE;
}

FVizBool fviz_internal_gl_device_text_supported(const FVizGLDevice* device)
{
    return device != NULL && device->text_program != 0u && device->text_vao != 0u && device->text_vbo != 0u &&
                   device->text_texture != 0u
               ? FVIZ_TRUE
               : FVIZ_FALSE;
}

FVizBool fviz_internal_gl_device_integer_selection_supported(const FVizGLDevice* device)
{
    return device != NULL && device->selection_program != 0u && device->gl.glClearBufferuiv != NULL ? FVIZ_TRUE
                                                                                                    : FVIZ_FALSE;
}

FVizBool fviz_internal_gl_device_gpu_timing_supported(const FVizGLDevice* device)
{
    return device != NULL && device->gpu_time_queries[0] != 0u && device->gpu_time_queries[1] != 0u ? FVIZ_TRUE
                                                                                                    : FVIZ_FALSE;
}

FVizBool fviz_internal_gl_device_shader_lines_supported(const FVizGLDevice* device)
{
    return device != NULL && device->edge_program != 0u ? FVIZ_TRUE : FVIZ_FALSE;
}

void fviz_internal_gl_device_destroy(FVizGLDevice* device)
{
    FVizSize i;
    if (device == NULL) return;
    if (device->gpu_time_queries[0] != 0u || device->gpu_time_queries[1] != 0u)
    {
        if (device->gl.glDeleteQueries != NULL) device->gl.glDeleteQueries(2, device->gpu_time_queries);
        device->gpu_time_queries[0] = 0u;
        device->gpu_time_queries[1] = 0u;
    }
    for (i = 0u; i < device->actor_count; ++i)
    {
        fviz_gl_actor_resource_destroy(device, &device->actors[i]);
    }
    if (device->program != 0u)
    {
        device->gl.glDeleteProgram(device->program);
        device->program = 0u;
    }
    if (device->program_2d != 0u)
    {
        device->gl.glDeleteProgram(device->program_2d);
        device->program_2d = 0u;
    }
    if (device->edge_program != 0u)
    {
        device->gl.glDeleteProgram(device->edge_program);
        device->edge_program = 0u;
    }
    if (device->point_program != 0u)
    {
        device->gl.glDeleteProgram(device->point_program);
        device->point_program = 0u;
    }
    if (device->selection_program != 0u)
    {
        device->gl.glDeleteProgram(device->selection_program);
        device->selection_program = 0u;
    }
    if (device->selection_texture != 0u)
    {
        glDeleteTextures(1, &device->selection_texture);
        device->selection_texture = 0u;
    }
    if (device->selection_depth_renderbuffer != 0u)
    {
        device->gl.glDeleteRenderbuffers(1, &device->selection_depth_renderbuffer);
        device->selection_depth_renderbuffer = 0u;
    }
    if (device->selection_framebuffer != 0u)
    {
        device->gl.glDeleteFramebuffers(1, &device->selection_framebuffer);
        device->selection_framebuffer = 0u;
    }
    if (device->text_texture != 0u)
    {
        glDeleteTextures(1, &device->text_texture);
        device->text_texture = 0u;
    }
    if (device->text_vbo != 0u)
    {
        device->gl.glDeleteBuffers(1, &device->text_vbo);
        device->text_vbo = 0u;
    }
    if (device->text_vao != 0u)
    {
        device->gl.glDeleteVertexArrays(1, &device->text_vao);
        device->text_vao = 0u;
    }
    if (device->text_program != 0u)
    {
        device->gl.glDeleteProgram(device->text_program);
        device->text_program = 0u;
    }
    if (device->text_atlas != NULL)
    {
        fviz_release((void*)device->text_atlas);
        device->text_atlas = NULL;
    }
    fviz_free(device->text_staging);
    device->text_staging = NULL;
    device->text_staging_capacity_vertices = 0u;
    if (device->overlay_vbo != 0u)
    {
        device->gl.glDeleteBuffers(1, &device->overlay_vbo);
        device->overlay_vbo = 0u;
    }
    if (device->overlay_vao != 0u)
    {
        device->gl.glDeleteVertexArrays(1, &device->overlay_vao);
        device->overlay_vao = 0u;
    }
    if (device->oit_accum_texture != 0u) glDeleteTextures(1, &device->oit_accum_texture);
    if (device->oit_reveal_texture != 0u) glDeleteTextures(1, &device->oit_reveal_texture);
    if (device->peel_front_depth_texture != 0u) glDeleteTextures(1, &device->peel_front_depth_texture);
    if (device->peel_back_depth_texture != 0u) glDeleteTextures(1, &device->peel_back_depth_texture);
    if (device->peel_color_texture != 0u) glDeleteTextures(1, &device->peel_color_texture);
    if (device->peel_depth_renderbuffer != 0u) device->gl.glDeleteRenderbuffers(1, &device->peel_depth_renderbuffer);
    if (device->peel_framebuffer != 0u) device->gl.glDeleteFramebuffers(1, &device->peel_framebuffer);
    if (device->peel_depth_framebuffer != 0u) device->gl.glDeleteFramebuffers(1, &device->peel_depth_framebuffer);
    if (device->oit_color_renderbuffer != 0u) device->gl.glDeleteRenderbuffers(1, &device->oit_color_renderbuffer);
    if (device->oit_depth_renderbuffer != 0u) device->gl.glDeleteRenderbuffers(1, &device->oit_depth_renderbuffer);
    if (device->oit_framebuffer != 0u) device->gl.glDeleteFramebuffers(1, &device->oit_framebuffer);
    if (device->oit_resolve_framebuffer != 0u) device->gl.glDeleteFramebuffers(1, &device->oit_resolve_framebuffer);
    if (device->oit_vao != 0u) device->gl.glDeleteVertexArrays(1, &device->oit_vao);
    if (device->oit_composite_program != 0u) device->gl.glDeleteProgram(device->oit_composite_program);
    if (device->fxaa_framebuffer != 0u)
    {
        device->gl.glDeleteFramebuffers(1, &device->fxaa_framebuffer);
        device->fxaa_framebuffer = 0u;
    }
    if (device->fxaa_texture != 0u)
    {
        glDeleteTextures(1, &device->fxaa_texture);
        device->fxaa_texture = 0u;
    }
    if (device->fxaa_vao != 0u)
    {
        device->gl.glDeleteVertexArrays(1, &device->fxaa_vao);
        device->fxaa_vao = 0u;
    }
    if (device->fxaa_program != 0u)
    {
        device->gl.glDeleteProgram(device->fxaa_program);
        device->fxaa_program = 0u;
    }
    if (device->volume_ibo != 0u) device->gl.glDeleteBuffers(1, &device->volume_ibo);
    if (device->volume_vbo != 0u) device->gl.glDeleteBuffers(1, &device->volume_vbo);
    if (device->volume_vao != 0u) device->gl.glDeleteVertexArrays(1, &device->volume_vao);
    if (device->volume_transfer_texture != 0u) glDeleteTextures(1, &device->volume_transfer_texture);
    if (device->volume_scalar_texture != 0u) glDeleteTextures(1, &device->volume_scalar_texture);
    if (device->volume_program != 0u) device->gl.glDeleteProgram(device->volume_program);
    fviz_free(device->draw_items);
    device->draw_items = NULL;
    device->draw_item_capacity = 0u;
    fviz_free(device->actors);
    device->actors = NULL;
    device->actor_count = 0u;
    device->actor_capacity = 0u;
    fviz_free(device);
}

void fviz_internal_gl_device_begin_frame(FVizGLDevice* device)
{
    if (device == NULL) return;
    (void)memset(&device->frame_statistics, 0, sizeof(device->frame_statistics));
    ++device->frame_serial;
    if (device->frame_serial == 0u)
    {
        FVizSize i;
        device->frame_serial = 1u;
        for (i = 0u; i < device->actor_count; ++i)
            device->actors[i].last_seen_frame = 0u;
    }
    device->gpu_time_query_active = FVIZ_FALSE;
    if (fviz_internal_gl_device_gpu_timing_supported(device) != FVIZ_FALSE)
    {
        uint32_t candidate;
        for (candidate = 0u; candidate < 2u; ++candidate)
        {
            if (device->gpu_time_query_issued[candidate] != FVIZ_FALSE)
            {
                GLint available = GL_FALSE;
                device->gl.glGetQueryObjectiv(device->gpu_time_queries[candidate], FVIZ_GL_QUERY_RESULT_AVAILABLE,
                                              &available);
                if (available != GL_FALSE)
                {
                    uint64_t nanoseconds = 0u;
                    device->gl.glGetQueryObjectui64v(device->gpu_time_queries[candidate], FVIZ_GL_QUERY_RESULT,
                                                     &nanoseconds);
                    device->gpu_time_query_issued[candidate] = FVIZ_FALSE;
                    device->frame_statistics.gpu_frame_nanoseconds = nanoseconds;
                    device->frame_statistics.gpu_timing_valid = FVIZ_TRUE;
                }
            }
        }
        if (device->gpu_time_query_issued[device->gpu_time_write_index] == FVIZ_FALSE)
        {
            device->gl.glBeginQuery(FVIZ_GL_TIME_ELAPSED, device->gpu_time_queries[device->gpu_time_write_index]);
            device->gpu_time_query_active = FVIZ_TRUE;
        }
    }
}

void fviz_internal_gl_device_end_frame(FVizGLDevice* device)
{
    FVizSize read_index;
    FVizSize write_index = 0u;
    uint64_t resident_mesh_gpu_bytes = 0u;
    if (device == NULL || device->frame_serial == 0u) return;
    if (device->gpu_time_query_active != FVIZ_FALSE)
    {
        device->gl.glEndQuery(FVIZ_GL_TIME_ELAPSED);
        device->gpu_time_query_issued[device->gpu_time_write_index] = FVIZ_TRUE;
        device->gpu_time_write_index = (device->gpu_time_write_index + 1u) % 2u;
        device->gpu_time_query_active = FVIZ_FALSE;
    }
    for (read_index = 0u; read_index < device->actor_count; ++read_index)
    {
        FVizGLActorResource* resource = &device->actors[read_index];
        if (fviz_gl_actor_resource_pinned(resource) == FVIZ_FALSE &&
            resource->last_seen_frame != device->frame_serial &&
            device->frame_serial - resource->last_seen_frame > (uint64_t)device->unused_resource_retention_frames)
        {
            fviz_gl_actor_resource_destroy(device, resource);
            if (device->frame_statistics.gpu_resource_evictions != UINT64_MAX)
                ++device->frame_statistics.gpu_resource_evictions;
            continue;
        }
        resident_mesh_gpu_bytes += fviz_gl_actor_resource_resident_bytes(resource);
        if (write_index != read_index) device->actors[write_index] = device->actors[read_index];
        ++write_index;
    }
    device->actor_count = write_index;
    if (device->mesh_byte_budget != 0u && resident_mesh_gpu_bytes > device->mesh_byte_budget)
    {
        for (;;)
        {
            FVizSize oldest_index = SIZE_MAX;
            uint64_t oldest_frame = UINT64_MAX;
            for (read_index = 0u; read_index < device->actor_count; ++read_index)
            {
                const FVizGLActorResource* resource = &device->actors[read_index];
                if (fviz_gl_actor_resource_pinned(resource) == FVIZ_FALSE &&
                    resource->last_seen_frame != device->frame_serial && resource->last_seen_frame < oldest_frame)
                {
                    oldest_frame = resource->last_seen_frame;
                    oldest_index = read_index;
                }
            }
            if (oldest_index == SIZE_MAX || resident_mesh_gpu_bytes <= device->mesh_byte_budget) break;
            {
                const uint64_t bytes = fviz_gl_actor_resource_resident_bytes(&device->actors[oldest_index]);
                fviz_gl_actor_resource_destroy(device, &device->actors[oldest_index]);
                if (oldest_index + 1u < device->actor_count)
                    (void)memmove(&device->actors[oldest_index], &device->actors[oldest_index + 1u],
                                  (size_t)(device->actor_count - oldest_index - 1u) * sizeof(*device->actors));
                --device->actor_count;
                resident_mesh_gpu_bytes = bytes <= resident_mesh_gpu_bytes ? resident_mesh_gpu_bytes - bytes : 0u;
                if (device->frame_statistics.gpu_resource_evictions != UINT64_MAX)
                    ++device->frame_statistics.gpu_resource_evictions;
            }
        }
    }
    device->frame_statistics.resident_actor_resources = (uint64_t)device->actor_count;
    device->frame_statistics.resident_mesh_gpu_bytes = resident_mesh_gpu_bytes;
    device->frame_statistics.resident_geometry_gpu_bytes = 0u;
    device->frame_statistics.resident_attribute_gpu_bytes = 0u;
    device->frame_statistics.resident_instance_gpu_bytes = 0u;
    device->frame_statistics.pinned_gpu_resources = 0u;
    for (read_index = 0u; read_index < device->actor_count; ++read_index)
    {
        fviz_gl_actor_resource_resident_classes(&device->actors[read_index],
                                                &device->frame_statistics.resident_geometry_gpu_bytes,
                                                &device->frame_statistics.resident_attribute_gpu_bytes,
                                                &device->frame_statistics.resident_instance_gpu_bytes);
        if (fviz_gl_actor_resource_pinned(&device->actors[read_index]) != FVIZ_FALSE)
            ++device->frame_statistics.pinned_gpu_resources;
    }
    device->frame_statistics.resident_render_target_gpu_bytes = 0u;
    if (device->selection_texture != 0u && device->selection_width > 0 && device->selection_height > 0)
        device->frame_statistics.resident_render_target_gpu_bytes +=
            (uint64_t)device->selection_width * (uint64_t)device->selection_height * 20u;
    if (device->oit_accum_texture != 0u && device->oit_width > 0 && device->oit_height > 0)
        device->frame_statistics.resident_render_target_gpu_bytes +=
            (uint64_t)device->oit_width * (uint64_t)device->oit_height * 18u;
    if (device->fxaa_texture != 0u && device->fxaa_width > 0 && device->fxaa_height > 0)
        device->frame_statistics.resident_render_target_gpu_bytes +=
            (uint64_t)device->fxaa_width * (uint64_t)device->fxaa_height * 4u;
    device->frame_statistics.gpu_mesh_byte_budget = device->mesh_byte_budget;
    device->frame_statistics.gpu_mesh_budget_exceeded =
        device->mesh_byte_budget != 0u && resident_mesh_gpu_bytes > device->mesh_byte_budget ? FVIZ_TRUE : FVIZ_FALSE;
}

void fviz_internal_gl_device_get_frame_statistics(const FVizGLDevice* device, FVizGLFrameStatistics* out_statistics)
{
    if (out_statistics == NULL) return;
    if (device == NULL)
    {
        (void)memset(out_statistics, 0, sizeof(*out_statistics));
        return;
    }
    *out_statistics = device->frame_statistics;
    out_statistics->resident_actor_resources = (uint64_t)device->actor_count;
    {
        FVizSize i;
        uint64_t bytes = 0u;
        for (i = 0u; i < device->actor_count; ++i)
            bytes += fviz_gl_actor_resource_resident_bytes(&device->actors[i]);
        out_statistics->resident_mesh_gpu_bytes = bytes;
    }
    out_statistics->gpu_mesh_byte_budget = device->mesh_byte_budget;
    out_statistics->gpu_mesh_budget_exceeded =
        device->mesh_byte_budget != 0u && out_statistics->resident_mesh_gpu_bytes > device->mesh_byte_budget
            ? FVIZ_TRUE
            : FVIZ_FALSE;
}

void fviz_internal_gl_device_set_memory_options(FVizGLDevice* device, const FVizGPUMemoryOptions* options)
{
    if (device == NULL || options == NULL) return;
    device->mesh_byte_budget = options->mesh_byte_budget;
    device->unused_resource_retention_frames = options->unused_resource_retention_frames;
}

void fviz_internal_gl_device_release_mesh_resources(FVizGLDevice* device)
{
    FVizSize index;
    if (device == NULL) return;
    for (index = 0u; index < device->actor_count; ++index)
        fviz_gl_actor_resource_destroy(device, &device->actors[index]);
    device->actor_count = 0u;
    device->frame_statistics.resident_actor_resources = 0u;
    device->frame_statistics.resident_mesh_gpu_bytes = 0u;
    device->frame_statistics.gpu_mesh_budget_exceeded = FVIZ_FALSE;
}

static FVizResult fviz_gl_render_shader_edges(FVizGLDevice* device, FVizRenderer* renderer, float aspect_ratio,
                                              int viewport_width, int viewport_height)
{
    const FVizGLFunctions* gl = &device->gl;
    FVizScene* scene = fviz_renderer_scene(renderer);
    FVizCamera* camera = fviz_renderer_camera(renderer);
    FVizMat4 projection;
    FVizMat4 view;
    FVizMat4 view_projection;
    GLboolean blend_enabled;
    GLboolean depth_write = GL_TRUE;
    GLint depth_function = GL_LESS;
    GLboolean cull_enabled;
    FVizFrustum culling_frustum;
    FVizBool use_frustum = FVIZ_FALSE;
    FVizBool use_small_object_culling;
    float small_object_threshold;
    FVizSize actor_count;
    FVizSize i;
    if (scene == NULL || camera == NULL || (device->edge_program == 0u && device->point_program == 0u) ||
        viewport_width <= 0 || viewport_height <= 0)
        return FVIZ_ERROR_NOT_SUPPORTED;
    projection = fviz_camera_projection_matrix(camera, aspect_ratio);
    view = fviz_camera_view_matrix(camera);
    view_projection = fviz_mat4_multiply(projection, view);
    actor_count = fviz_scene_actor_count(scene);
    if (fviz_renderer_frustum_culling(renderer) != FVIZ_FALSE &&
        fviz_renderer_get_frustum(renderer, aspect_ratio, &culling_frustum) == FVIZ_OK)
        use_frustum = FVIZ_TRUE;
    use_small_object_culling = fviz_renderer_small_object_culling(renderer);
    small_object_threshold = fviz_renderer_small_object_threshold_pixels(renderer);
    blend_enabled = glIsEnabled(GL_BLEND);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
    glGetIntegerv(GL_DEPTH_FUNC, &depth_function);
    cull_enabled = glIsEnabled(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    /* Depth peeling needs the depth writes so each layer advances the
     * peeled depth; regular sorted-alpha rendering leaves writes off. */
    if (device->peel_pass != 0) glDepthMask(GL_TRUE);
    else
        glDepthMask(GL_FALSE);
    glDepthFunc(GL_LEQUAL);
    /* The expanded line quads generated by the geometry shader must not be
     * backface-culled; the surface pass leaves GL_CULL_FACE enabled. */
    glDisable(GL_CULL_FACE);

    if (device->edge_program != 0u)
    {
        gl->glUseProgram(device->edge_program);
        {
            const GLfloat viewport[3] = {(GLfloat)viewport_width, (GLfloat)viewport_height, 0.0f};
            gl->glUniform3fv(device->edge_viewport_location, 1, viewport);
        }
        for (i = 0u; i < actor_count; ++i)
        {
            const FVizActor* actor = fviz_scene_const_actor(scene, i);
            FVizGLActorResource* resource;
            FVizMat4 mvp;
            float r;
            float g;
            float b;
            float dash_length = 0.0f;
            float gap_length = 0.0f;
            float dash_phase = 0.0f;
            GLint scalar_coloring;
            if (actor == NULL || fviz_actor_is_visible(actor) == FVIZ_FALSE) continue;
            if (use_frustum != FVIZ_FALSE &&
                fviz_frustum_intersects_bounds(&culling_frustum, fviz_actor_bounds(actor)) == FVIZ_FALSE)
                continue;
            if (use_small_object_culling != FVIZ_FALSE && small_object_threshold > 0.0f &&
                fviz_renderer_actor_projected_diameter_pixels(renderer, actor, aspect_ratio, viewport_height) <
                    small_object_threshold)
                continue;
            if (fviz_gl_ensure_actor_resource(device, actor) != FVIZ_OK) continue;
            resource = fviz_gl_find_actor_resource(device, actor);
            if (resource == NULL ||
                (resource->line_index_count <= 0 &&
                 (fviz_actor_edge_visibility(actor) == FVIZ_FALSE && fviz_actor_wireframe(actor) == FVIZ_FALSE)))
                continue;
            mvp = fviz_mat4_multiply(view_projection, fviz_actor_transform_matrix(actor));
            gl->glUniformMatrix4fv(device->edge_mvp_location, 1, GL_FALSE, mvp.m);
            gl->glUniform1f(device->edge_width_location, fviz_actor_line_width(actor));
            gl->glUniform1f(device->edge_depth_bias_location, fviz_actor_line_depth_bias(actor));
            gl->glUniform1i(device->edge_cap_location, (GLint)fviz_actor_line_cap(actor));
            gl->glUniform1i(device->edge_join_location, (GLint)fviz_actor_line_join(actor));
            gl->glUniform1f(device->edge_miter_limit_location, fviz_actor_line_miter_limit(actor));
            fviz_actor_get_line_dash(actor, &dash_length, &gap_length, &dash_phase);
            {
                const GLfloat dash[3] = {dash_length, gap_length, dash_phase};
                gl->glUniform3fv(device->edge_dash_location, 1, dash);
            }
            /* In wireframe mode use the per-vertex (scalar-mapped) colors so the
             * mesh stays clearly visible instead of a thin dark outline. */
            scalar_coloring = (resource->has_color != FVIZ_FALSE || resource->instance_count > 0) &&
                                      (fviz_actor_wireframe(actor) != FVIZ_FALSE ||
                                       fviz_actor_line_scalar_coloring(actor) != FVIZ_FALSE)
                                  ? 1
                                  : 0;
            gl->glUniform1i(device->edge_scalar_color_location, scalar_coloring);
            gl->glUniform1i(device->edge_instancing_location, resource->instance_count > 0 ? 1 : 0);
            fviz_actor_get_edge_color(actor, &r, &g, &b);
            gl->glUniform4fv(device->edge_color_location, 1, (const GLfloat[]){r, g, b, fviz_actor_opacity(actor)});
            gl->glBindVertexArray(resource->vao);
            if (resource->line_adjacency_index_count > 0 && resource->line_adjacency_index_buffer != 0u)
            {
                gl->glBindBuffer(FVIZ_GL_ELEMENT_ARRAY_BUFFER, resource->line_adjacency_index_buffer);
                if (resource->instance_count > 0)
                    gl->glDrawElementsInstanced(FVIZ_GL_LINES_ADJACENCY, resource->line_adjacency_index_count,
                                                GL_UNSIGNED_INT, (const void*)0, resource->instance_count);
                else
                    glDrawElements(FVIZ_GL_LINES_ADJACENCY, resource->line_adjacency_index_count, GL_UNSIGNED_INT,
                                   (const void*)0);
                ++device->frame_statistics.draw_calls;
                device->frame_statistics.lines +=
                    ((uint64_t)resource->line_adjacency_index_count / 4u) *
                    (uint64_t)(resource->instance_count > 0 ? resource->instance_count : 1);
            }
            if ((fviz_actor_edge_visibility(actor) != FVIZ_FALSE || fviz_actor_wireframe(actor) != FVIZ_FALSE) &&
                resource->triangle_edge_adjacency_index_count > 0 &&
                resource->triangle_edge_adjacency_index_buffer != 0u)
            {
                gl->glBindBuffer(FVIZ_GL_ELEMENT_ARRAY_BUFFER, resource->triangle_edge_adjacency_index_buffer);
                if (resource->instance_count > 0)
                    gl->glDrawElementsInstanced(FVIZ_GL_LINES_ADJACENCY, resource->triangle_edge_adjacency_index_count,
                                                GL_UNSIGNED_INT, (const void*)0, resource->instance_count);
                else
                    glDrawElements(FVIZ_GL_LINES_ADJACENCY, resource->triangle_edge_adjacency_index_count,
                                   GL_UNSIGNED_INT, (const void*)0);
                ++device->frame_statistics.draw_calls;
                device->frame_statistics.lines +=
                    ((uint64_t)resource->triangle_edge_adjacency_index_count / 4u) *
                    (uint64_t)(resource->instance_count > 0 ? resource->instance_count : 1);
            }
        }
        gl->glBindVertexArray(0u);
        gl->glUseProgram(0u);
    }

    if (device->point_program != 0u)
    {
        glEnable(0x8642 /* GL_PROGRAM_POINT_SIZE */);
        gl->glUseProgram(device->point_program);
        for (i = 0u; i < actor_count; ++i)
        {
            const FVizActor* actor = fviz_scene_const_actor(scene, i);
            FVizGLActorResource* resource;
            FVizMat4 mvp;
            float r;
            float g;
            float b;
            GLint scalar_coloring;
            if (actor == NULL || fviz_actor_is_visible(actor) == FVIZ_FALSE ||
                fviz_actor_point_visibility(actor) == FVIZ_FALSE)
                continue;
            if (use_frustum != FVIZ_FALSE &&
                fviz_frustum_intersects_bounds(&culling_frustum, fviz_actor_bounds(actor)) == FVIZ_FALSE)
                continue;
            if (use_small_object_culling != FVIZ_FALSE && small_object_threshold > 0.0f &&
                fviz_renderer_actor_projected_diameter_pixels(renderer, actor, aspect_ratio, viewport_height) <
                    small_object_threshold)
                continue;
            if (fviz_gl_ensure_actor_resource(device, actor) != FVIZ_OK) continue;
            resource = fviz_gl_find_actor_resource(device, actor);
            if (resource == NULL || resource->point_index_count <= 0) continue;
            mvp = fviz_mat4_multiply(view_projection, fviz_actor_transform_matrix(actor));
            gl->glUniformMatrix4fv(device->point_mvp_location, 1, GL_FALSE, mvp.m);
            gl->glUniform1f(device->point_size_location, fviz_actor_point_size(actor));
            gl->glUniform1i(device->point_shape_location, (GLint)fviz_actor_point_shape(actor));
            scalar_coloring = (resource->has_color != FVIZ_FALSE || resource->instance_count > 0) &&
                                      fviz_actor_point_scalar_coloring(actor) != FVIZ_FALSE
                                  ? 1
                                  : 0;
            gl->glUniform1i(device->point_scalar_color_location, scalar_coloring);
            gl->glUniform1i(device->point_instancing_location, resource->instance_count > 0 ? 1 : 0);
            fviz_actor_get_point_color(actor, &r, &g, &b);
            gl->glUniform4fv(device->point_color_location, 1, (const GLfloat[]){r, g, b, fviz_actor_opacity(actor)});
            glDepthMask(fviz_gl_actor_is_translucent(actor) == FVIZ_FALSE ? GL_TRUE : GL_FALSE);
            gl->glBindVertexArray(resource->vao);
            gl->glBindBuffer(FVIZ_GL_ELEMENT_ARRAY_BUFFER, resource->point_index_buffer);
            if (resource->instance_count > 0)
                gl->glDrawElementsInstanced(FVIZ_GL_POINTS, resource->point_index_count, GL_UNSIGNED_INT,
                                            (const void*)0, resource->instance_count);
            else
                glDrawElements(FVIZ_GL_POINTS, resource->point_index_count, GL_UNSIGNED_INT, (const void*)0);
            ++device->frame_statistics.draw_calls;
        }
        gl->glBindVertexArray(0u);
        gl->glUseProgram(0u);
        glDisable(0x8642 /* GL_PROGRAM_POINT_SIZE */);
    }

    glDepthMask(depth_write);
    glDepthFunc((GLenum)depth_function);
    if (blend_enabled != GL_FALSE) glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    if (cull_enabled != GL_FALSE) glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    return glGetError() == GL_NO_ERROR ? FVIZ_OK : FVIZ_ERROR_GRAPHICS;
}

FVizResult fviz_internal_gl_device_render_stage(FVizGLDevice* device, FVizRenderer* renderer, float aspect_ratio,
                                                int viewport_width, int viewport_height, FVizRenderPassStage stage)
{
    const FVizGLFunctions* gl;
    FVizCamera* camera;
    FVizScene* scene;
    FVizMat4 projection;
    FVizMat4 view;
    FVizMat4 mvp;
    GLfloat light_position_intensity[16];
    GLfloat light_colors[12];
    GLint light_count = 0;
    FVizSize i;
    FVizSize actor_iteration_count;
    FVizFrustum culling_frustum;
    FVizBool use_frustum = FVIZ_FALSE;

    if (device == NULL || renderer == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "device and renderer must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (stage == FVIZ_RENDER_PASS_EDGE && (device->edge_program != 0u || device->point_program != 0u))
        return fviz_gl_render_shader_edges(device, renderer, aspect_ratio, viewport_width, viewport_height);
    gl = &device->gl;
    scene = fviz_renderer_scene(renderer);
    camera = fviz_renderer_camera(renderer);
    if (scene == NULL || camera == NULL) return FVIZ_OK;
    if (fviz_renderer_frustum_culling(renderer) != FVIZ_FALSE &&
        fviz_renderer_get_frustum(renderer, aspect_ratio, &culling_frustum) == FVIZ_OK)
        use_frustum = FVIZ_TRUE;

    projection = fviz_camera_projection_matrix(camera, aspect_ratio);
    view = fviz_camera_view_matrix(camera);
    mvp = fviz_mat4_multiply(projection, view);

    gl->glUseProgram(device->program);
    gl->glUniformMatrix4fv(device->mvp_location, 1, GL_FALSE, mvp.m);
    {
        FVizWeightedOITOptions oit_options;
        fviz_renderer_get_weighted_oit_options(renderer, &oit_options);
        gl->glUniform1i(device->oit_pass_location, device->oit_pass);
        gl->glUniform1f(device->oit_weight_scale_location, oit_options.weight_scale);
        gl->glUniform1f(device->oit_depth_weight_location, oit_options.depth_weight);
        gl->glUniform1f(device->oit_minimum_weight_location, oit_options.minimum_weight);
        gl->glUniform1f(device->oit_alpha_cutoff_location, oit_options.alpha_cutoff);
    }

    for (i = 0u; i < fviz_renderer_light_count(renderer) && light_count < 4; ++i)
    {
        const FVizLight* light = fviz_renderer_light_at(renderer, i);
        FVizVec3 position;
        float red;
        float green;
        float blue;
        if (light == NULL || fviz_light_enabled(light) == FVIZ_FALSE || fviz_light_intensity(light) <= 0.0f) continue;
        position =
            fviz_light_type(light) == FVIZ_LIGHT_HEADLIGHT ? fviz_camera_position(camera) : fviz_light_position(light);
        fviz_light_get_color(light, &red, &green, &blue);
        light_position_intensity[light_count * 4 + 0] = position.x;
        light_position_intensity[light_count * 4 + 1] = position.y;
        light_position_intensity[light_count * 4 + 2] = position.z;
        light_position_intensity[light_count * 4 + 3] = fviz_light_intensity(light);
        light_colors[light_count * 3 + 0] = red;
        light_colors[light_count * 3 + 1] = green;
        light_colors[light_count * 3 + 2] = blue;
        ++light_count;
    }
    gl->glUniform1i(device->light_count_location, light_count);
    if (light_count > 0)
    {
        gl->glUniform4fv(device->light_position_intensity_location, light_count, light_position_intensity);
        gl->glUniform3fv(device->light_color_location, light_count, light_colors);
    }
    {
        const FVizVec3 camera_position = fviz_camera_position(camera);
        gl->glUniform3fv(device->camera_position_location, 1, &camera_position.x);
    }

    actor_iteration_count = fviz_scene_actor_count(scene);
    if (stage == FVIZ_RENDER_PASS_TRANSLUCENT && device->oit_pass == 0)
    {
        FVizResult order_result = fviz_gl_prepare_translucent_order(device, scene, camera, &actor_iteration_count);
        if (order_result != FVIZ_OK)
        {
            gl->glUseProgram(0u);
            return order_result;
        }
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
    }
    for (i = 0u; i < actor_iteration_count; ++i)
    {
        const FVizActor* actor = stage == FVIZ_RENDER_PASS_TRANSLUCENT && device->oit_pass == 0
                                     ? device->draw_items[i].actor
                                     : fviz_scene_const_actor(scene, i);
        FVizGLActorResource* resource;
        FVizMat4 model;
        FVizMat3 normal_matrix;
        FVizMat4 mvp_actor;
        float red;
        float green;
        float blue;
        float edge_red;
        float edge_green;
        float edge_blue;
        float opacity;
        FVizBool actor_translucent;
        FVizBool frustum_culled = FVIZ_FALSE;
        FVizBool small_object_culled = FVIZ_FALSE;
        FVizMapper* mapper;
        FVizPlane clip_planes[6];
        GLfloat clip_values[24];
        FVizSize clip_count;
        FVizSize clip_index;

        if (stage == FVIZ_RENDER_PASS_OPAQUE) ++device->frame_statistics.actors_considered;
        if (fviz_actor_is_visible(actor) == FVIZ_FALSE) continue;

        actor_translucent = fviz_gl_actor_is_translucent(actor);
        if (stage == FVIZ_RENDER_PASS_TRANSLUCENT && actor_translucent == FVIZ_FALSE) continue;
        if (stage != FVIZ_RENDER_PASS_OPAQUE && stage != FVIZ_RENDER_PASS_TRANSLUCENT && stage != FVIZ_RENDER_PASS_EDGE)
            continue;

        if (use_frustum != FVIZ_FALSE &&
            fviz_frustum_intersects_bounds(&culling_frustum, fviz_actor_bounds(actor)) == FVIZ_FALSE)
            frustum_culled = FVIZ_TRUE;
        if (frustum_culled == FVIZ_FALSE && fviz_renderer_small_object_culling(renderer) != FVIZ_FALSE &&
            fviz_renderer_actor_projected_diameter_pixels(renderer, actor, aspect_ratio, viewport_height) <
                fviz_renderer_small_object_threshold_pixels(renderer))
            small_object_culled = FVIZ_TRUE;

        if (stage == FVIZ_RENDER_PASS_OPAQUE)
        {
            if (frustum_culled != FVIZ_FALSE) ++device->frame_statistics.actors_frustum_culled;
            else if (small_object_culled != FVIZ_FALSE)
                ++device->frame_statistics.actors_small_object_culled;
            else
                ++device->frame_statistics.actors_visible_after_culling;
        }
        if (frustum_culled != FVIZ_FALSE || small_object_culled != FVIZ_FALSE) continue;
        if (stage == FVIZ_RENDER_PASS_OPAQUE && actor_translucent != FVIZ_FALSE) continue;

        if (stage == FVIZ_RENDER_PASS_TRANSLUCENT && fviz_actor_const_volume_mapper(actor) != NULL &&
            device->volume_program_ready != FVIZ_FALSE)
        {
            glDisable(GL_CULL_FACE);
            if (fviz_gl_render_volume(device, renderer, actor, aspect_ratio) == FVIZ_OK)
                ++device->frame_statistics.draw_calls;
            continue;
        }
        model = fviz_actor_transform_matrix(actor);
        opacity = fviz_actor_opacity(actor);
        if (fviz_gl_ensure_actor_resource(device, actor) != FVIZ_OK) continue;
        resource = fviz_gl_find_actor_resource(device, actor);
        if (resource == NULL || (resource->index_count == 0 && resource->line_index_count == 0)) continue;

        mvp_actor = fviz_mat4_multiply(mvp, model);
        gl->glUniformMatrix4fv(device->mvp_location, 1, GL_FALSE, mvp_actor.m);
        gl->glUniformMatrix4fv(device->model_location, 1, GL_FALSE, model.m);

        {
            FVizMat3 model3;
            model3.m[0] = model.m[0];
            model3.m[1] = model.m[1];
            model3.m[2] = model.m[2];
            model3.m[3] = model.m[4];
            model3.m[4] = model.m[5];
            model3.m[5] = model.m[6];
            model3.m[6] = model.m[8];
            model3.m[7] = model.m[9];
            model3.m[8] = model.m[10];
            normal_matrix = fviz_mat3_transpose(fviz_mat3_inverse(model3));
        }
        gl->glUniformMatrix3fv(device->normal_matrix_location, 1, GL_FALSE, normal_matrix.m);

        fviz_actor_get_color(actor, &red, &green, &blue);
        gl->glUniform3fv(device->diffuse_location, 1, (const GLfloat[]){red, green, blue});
        gl->glUniform1i(device->instancing_location, resource->instance_count > 0 ? 1 : 0);
        gl->glUniform1i(device->scalar_color_location,
                        resource->instance_count > 0 || resource->has_color == FVIZ_TRUE ? 1 : 0);
        gl->glUniform1f(device->opacity_location, opacity);
        mapper = fviz_actor_mapper((FVizActor*)actor);
        clip_count = mapper != NULL ? fviz_mapper_clipping_plane_count(mapper) : 0u;
        for (clip_index = 0u; clip_index < clip_count; ++clip_index)
        {
            (void)fviz_mapper_clipping_plane(mapper, clip_index, &clip_planes[clip_index]);
            clip_values[clip_index * 4u + 0u] = clip_planes[clip_index].normal.x;
            clip_values[clip_index * 4u + 1u] = clip_planes[clip_index].normal.y;
            clip_values[clip_index * 4u + 2u] = clip_planes[clip_index].normal.z;
            clip_values[clip_index * 4u + 3u] = clip_planes[clip_index].distance;
        }
        gl->glUniform1i(device->clip_plane_count_location, (GLint)clip_count);
        if (clip_count > 0u) gl->glUniform4fv(device->clip_planes_location, (GLsizei)clip_count, clip_values);
        {
            float ambient;
            float diffuse_factor;
            float specular;
            float specular_power;
            fviz_actor_get_material(actor, &ambient, &diffuse_factor, &specular, &specular_power);
            gl->glUniform1f(device->ambient_factor_location, ambient);
            gl->glUniform1f(device->diffuse_factor_location, diffuse_factor);
            gl->glUniform1f(device->specular_factor_location, specular);
            gl->glUniform1f(device->specular_power_location, specular_power);
            gl->glUniform1i(device->flat_shading_location, fviz_actor_shading_mode(actor) == FVIZ_SHADING_FLAT ? 1 : 0);
        }
        switch (fviz_actor_cull_mode(actor))
        {
            case FVIZ_CULL_FRONT:
                glEnable(GL_CULL_FACE);
                glCullFace(GL_FRONT);
                break;
            case FVIZ_CULL_BACK:
                glEnable(GL_CULL_FACE);
                glCullFace(GL_BACK);
                break;
            case FVIZ_CULL_NONE:
            default:
                glDisable(GL_CULL_FACE);
                break;
        }

        gl->glBindVertexArray(resource->vao);
        if (stage != FVIZ_RENDER_PASS_EDGE && resource->index_count > 0 && fviz_actor_wireframe(actor) == FVIZ_FALSE)
        {
            gl->glBindBuffer(FVIZ_GL_ELEMENT_ARRAY_BUFFER, resource->index_buffer);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            if (fviz_actor_edge_visibility(actor) != FVIZ_FALSE)
            {
                glEnable(GL_POLYGON_OFFSET_FILL);
                glPolygonOffset(1.0f, 1.0f);
            }
            if (resource->instance_count > 0)
                gl->glDrawElementsInstanced(GL_TRIANGLES, resource->index_count, GL_UNSIGNED_INT, (const void*)0,
                                            resource->instance_count);
            else
                glDrawElements(GL_TRIANGLES, resource->index_count, GL_UNSIGNED_INT, (const void*)0);
            ++device->frame_statistics.draw_calls;
            device->frame_statistics.triangles +=
                ((uint64_t)resource->index_count / 3u) *
                (uint64_t)(resource->instance_count > 0 ? resource->instance_count : 1);
            if (fviz_actor_edge_visibility(actor) != FVIZ_FALSE)
            {
                glDisable(GL_POLYGON_OFFSET_FILL);
            }
        }
        if (stage == FVIZ_RENDER_PASS_EDGE)
        {
            /* Wireframe mode uses per-vertex scalar colors so the mesh is
             * clearly visible; plain edges keep the actor edge color. */
            const GLint edge_scalar_coloring =
                resource->has_color != FVIZ_FALSE && (fviz_actor_wireframe(actor) != FVIZ_FALSE ||
                                                      fviz_actor_line_scalar_coloring(actor) != FVIZ_FALSE)
                    ? 1
                    : 0;
            gl->glUniform1i(device->scalar_color_location, edge_scalar_coloring);
            if (edge_scalar_coloring == 0)
            {
                fviz_actor_get_edge_color(actor, &edge_red, &edge_green, &edge_blue);
                gl->glUniform3fv(device->diffuse_location, 1, (const GLfloat[]){edge_red, edge_green, edge_blue});
            }
            glLineWidth(fviz_actor_line_width(actor));
            if (resource->line_index_count > 0 && resource->line_index_buffer != 0u)
            {
                gl->glBindBuffer(FVIZ_GL_ELEMENT_ARRAY_BUFFER, resource->line_index_buffer);
                if (resource->instance_count > 0)
                    gl->glDrawElementsInstanced(GL_LINES, resource->line_index_count, GL_UNSIGNED_INT, (const void*)0,
                                                resource->instance_count);
                else
                    glDrawElements(GL_LINES, resource->line_index_count, GL_UNSIGNED_INT, (const void*)0);
                ++device->frame_statistics.draw_calls;
                device->frame_statistics.lines +=
                    ((uint64_t)resource->line_index_count / 2u) *
                    (uint64_t)(resource->instance_count > 0 ? resource->instance_count : 1);
            }
            if ((fviz_actor_edge_visibility(actor) != FVIZ_FALSE || fviz_actor_wireframe(actor) != FVIZ_FALSE) &&
                resource->triangle_edge_index_count > 0 && resource->triangle_edge_index_buffer != 0u)
            {
                gl->glBindBuffer(FVIZ_GL_ELEMENT_ARRAY_BUFFER, resource->triangle_edge_index_buffer);
                if (resource->instance_count > 0)
                    gl->glDrawElementsInstanced(GL_LINES, resource->triangle_edge_index_count, GL_UNSIGNED_INT,
                                                (const void*)0, resource->instance_count);
                else
                    glDrawElements(GL_LINES, resource->triangle_edge_index_count, GL_UNSIGNED_INT, (const void*)0);
                ++device->frame_statistics.draw_calls;
                device->frame_statistics.lines +=
                    ((uint64_t)resource->triangle_edge_index_count / 2u) *
                    (uint64_t)(resource->instance_count > 0 ? resource->instance_count : 1);
            }
        }
        gl->glBindVertexArray(0u);
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    glLineWidth(1.0f);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    if (stage == FVIZ_RENDER_PASS_TRANSLUCENT && device->oit_pass == 0)
    {
        glDepthMask(GL_TRUE);
        glDisable(GL_BLEND);
    }
    gl->glUseProgram(0u);
    return FVIZ_OK;
}

static FVizResult fviz_gl_ensure_selection_target(FVizGLDevice* device, int width, int height)
{
    const FVizGLFunctions* gl;
    if (device == NULL || width <= 0 || height <= 0) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_internal_gl_device_integer_selection_supported(device) == FVIZ_FALSE) return FVIZ_ERROR_NOT_SUPPORTED;
    if (device->selection_framebuffer == 0u)
    {
        gl = &device->gl;
        gl->glGenFramebuffers(1, &device->selection_framebuffer);
        glGenTextures(1, &device->selection_texture);
        gl->glGenRenderbuffers(1, &device->selection_depth_renderbuffer);
        if (device->selection_framebuffer == 0u || device->selection_texture == 0u ||
            device->selection_depth_renderbuffer == 0u)
            return FVIZ_ERROR_GRAPHICS;
        glBindTexture(GL_TEXTURE_2D, device->selection_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindTexture(GL_TEXTURE_2D, 0u);
    }
    if (device->selection_width == width && device->selection_height == height) return FVIZ_OK;
    gl = &device->gl;
    glBindTexture(GL_TEXTURE_2D, device->selection_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, FVIZ_GL_RGBA32UI, width, height, 0, FVIZ_GL_RGBA_INTEGER, GL_UNSIGNED_INT, NULL);
    glBindTexture(GL_TEXTURE_2D, 0u);
    gl->glBindRenderbuffer(FVIZ_GL_RENDERBUFFER, device->selection_depth_renderbuffer);
    gl->glRenderbufferStorage(FVIZ_GL_RENDERBUFFER, FVIZ_GL_DEPTH24_STENCIL8, width, height);
    gl->glBindRenderbuffer(FVIZ_GL_RENDERBUFFER, 0u);
    gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, device->selection_framebuffer);
    gl->glFramebufferTexture2D(FVIZ_GL_FRAMEBUFFER, FVIZ_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, device->selection_texture,
                               0);
    gl->glFramebufferRenderbuffer(FVIZ_GL_FRAMEBUFFER, FVIZ_GL_DEPTH_STENCIL_ATTACHMENT, FVIZ_GL_RENDERBUFFER,
                                  device->selection_depth_renderbuffer);
    if (gl->glCheckFramebufferStatus(FVIZ_GL_FRAMEBUFFER) != FVIZ_GL_FRAMEBUFFER_COMPLETE)
    {
        gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, 0u);
        return FVIZ_ERROR_GRAPHICS;
    }
    gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, 0u);
    device->selection_width = width;
    device->selection_height = height;
    return glGetError() == GL_NO_ERROR ? FVIZ_OK : FVIZ_ERROR_GRAPHICS;
}

static FVizGLActorResource* fviz_gl_selection_prepare_actor(FVizGLDevice* device, FVizRenderer* renderer,
                                                            const FVizActor* actor, FVizSize actor_index,
                                                            FVizSelectionAssociation association,
                                                            FVizBool depth_prepass, float aspect_ratio,
                                                            int viewport_height, FVizMat4 view_projection)
{
    const FVizGLFunctions* gl = &device->gl;
    const FVizGlyphMapper* glyph_mapper;
    const FVizBool has_glyphs = actor != NULL && fviz_actor_const_glyph_mapper(actor) != NULL ? FVIZ_TRUE : FVIZ_FALSE;
    FVizGLActorResource* resource;
    FVizMat4 model;
    FVizMat4 mvp;
    FVizMapper* mapper;
    GLfloat clip_values[24];
    FVizSize clip_count;
    FVizSize clip_index;

    if (actor == NULL || fviz_actor_is_visible(actor) == FVIZ_FALSE || fviz_actor_pickable(actor) == FVIZ_FALSE ||
        fviz_actor_opacity(actor) <= 0.0f ||
        fviz_renderer_actor_is_renderable(renderer, actor, aspect_ratio, viewport_height) == FVIZ_FALSE)
        return NULL;
    if (depth_prepass == FVIZ_FALSE)
    {
        if (association == FVIZ_SELECTION_GLYPH_INSTANCE && has_glyphs == FVIZ_FALSE) return NULL;
        if ((association == FVIZ_SELECTION_POINT || association == FVIZ_SELECTION_CELL ||
             association == FVIZ_SELECTION_EDGE) &&
            has_glyphs != FVIZ_FALSE)
            return NULL;
    }
    if (fviz_gl_ensure_actor_resource(device, actor) != FVIZ_OK) return NULL;
    resource = fviz_gl_find_actor_resource(device, actor);
    if (resource == NULL) return NULL;
    if (depth_prepass != FVIZ_FALSE && resource->index_count <= 0) return NULL;
    if (depth_prepass == FVIZ_FALSE)
    {
        if (association == FVIZ_SELECTION_CELL && resource->index_count <= 0) return NULL;
        if (association == FVIZ_SELECTION_POINT && resource->point_count == 0u) return NULL;
        if (association == FVIZ_SELECTION_EDGE)
        {
            const FVizPolyData* data = fviz_gl_actor_render_poly_data(actor);
            if (data == NULL || fviz_poly_data_triangle_count(data) == 0u) return NULL;
            if (fviz_gl_upload_triangle_edges(device, resource, data) != FVIZ_OK) return NULL;
            if (resource->triangle_edge_index_count <= 0) return NULL;
        }
        if (association == FVIZ_SELECTION_GLYPH_INSTANCE &&
            (resource->index_count <= 0 || resource->instance_count <= 0))
            return NULL;
        if (association == FVIZ_SELECTION_ACTOR && resource->index_count <= 0 && resource->line_index_count <= 0 &&
            resource->point_count == 0u)
            return NULL;
    }

    model = fviz_actor_transform_matrix(actor);
    mvp = fviz_mat4_multiply(view_projection, model);
    gl->glUniformMatrix4fv(device->selection_mvp_location, 1, GL_FALSE, mvp.m);
    gl->glUniformMatrix4fv(device->selection_model_location, 1, GL_FALSE, model.m);
    gl->glUniform1ui(device->selection_actor_id_location, (GLuint)actor_index);
    glyph_mapper = fviz_actor_const_glyph_mapper(actor);
    gl->glUniform1i(device->selection_instancing_location,
                    glyph_mapper != NULL && resource->instance_count > 0 ? 1 : 0);
    mapper = fviz_actor_mapper((FVizActor*)actor);
    clip_count = mapper != NULL ? fviz_mapper_clipping_plane_count(mapper) : 0u;
    if (clip_count > 6u) clip_count = 6u;
    for (clip_index = 0u; clip_index < clip_count; ++clip_index)
    {
        FVizPlane plane;
        (void)fviz_mapper_clipping_plane(mapper, clip_index, &plane);
        clip_values[clip_index * 4u + 0u] = plane.normal.x;
        clip_values[clip_index * 4u + 1u] = plane.normal.y;
        clip_values[clip_index * 4u + 2u] = plane.normal.z;
        clip_values[clip_index * 4u + 3u] = plane.distance;
    }
    gl->glUniform1i(device->selection_clip_plane_count_location, (GLint)clip_count);
    if (clip_count > 0u) gl->glUniform4fv(device->selection_clip_planes_location, (GLsizei)clip_count, clip_values);
    if (fviz_actor_cull_mode(actor) == FVIZ_CULL_NONE) glDisable(GL_CULL_FACE);
    else
    {
        glEnable(GL_CULL_FACE);
        glCullFace(fviz_actor_cull_mode(actor) == FVIZ_CULL_FRONT ? GL_FRONT : GL_BACK);
    }
    return resource;
}

static void fviz_gl_selection_draw_actor(FVizGLDevice* device, const FVizActor* actor, FVizGLActorResource* resource,
                                         FVizSelectionAssociation association, FVizBool depth_prepass)
{
    const FVizGLFunctions* gl = &device->gl;
    gl->glBindVertexArray(resource->vao);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    if (depth_prepass != FVIZ_FALSE || association == FVIZ_SELECTION_CELL ||
        association == FVIZ_SELECTION_GLYPH_INSTANCE)
    {
        if (resource->index_count <= 0) return;
        gl->glBindBuffer(FVIZ_GL_ELEMENT_ARRAY_BUFFER, resource->index_buffer);
        if (resource->instance_count > 0)
            gl->glDrawElementsInstanced(GL_TRIANGLES, resource->index_count, GL_UNSIGNED_INT, (const void*)0,
                                        resource->instance_count);
        else
            glDrawElements(GL_TRIANGLES, resource->index_count, GL_UNSIGNED_INT, (const void*)0);
        return;
    }
    if (association == FVIZ_SELECTION_POINT)
    {
        const float point_size = fviz_actor_point_size(actor) > 7.0f ? fviz_actor_point_size(actor) : 7.0f;
        glPointSize(point_size);
        glDrawArrays(FVIZ_GL_POINTS, 0, (GLsizei)resource->point_count);
        return;
    }
    if (association == FVIZ_SELECTION_EDGE)
    {
        const float line_width = fviz_actor_line_width(actor) > 5.0f ? fviz_actor_line_width(actor) : 5.0f;
        glLineWidth(line_width);
        gl->glBindBuffer(FVIZ_GL_ELEMENT_ARRAY_BUFFER, resource->triangle_edge_index_buffer);
        glDrawElements(GL_LINES, resource->triangle_edge_index_count, GL_UNSIGNED_INT, (const void*)0);
        return;
    }
    if (association == FVIZ_SELECTION_ACTOR)
    {
        if (resource->index_count > 0)
        {
            gl->glBindBuffer(FVIZ_GL_ELEMENT_ARRAY_BUFFER, resource->index_buffer);
            if (resource->instance_count > 0)
                gl->glDrawElementsInstanced(GL_TRIANGLES, resource->index_count, GL_UNSIGNED_INT, (const void*)0,
                                            resource->instance_count);
            else
                glDrawElements(GL_TRIANGLES, resource->index_count, GL_UNSIGNED_INT, (const void*)0);
        }
        else if (resource->line_index_count > 0)
        {
            glLineWidth(fviz_actor_line_width(actor) > 3.0f ? fviz_actor_line_width(actor) : 3.0f);
            gl->glBindBuffer(FVIZ_GL_ELEMENT_ARRAY_BUFFER, resource->line_index_buffer);
            glDrawElements(GL_LINES, resource->line_index_count, GL_UNSIGNED_INT, (const void*)0);
        }
        else if (resource->point_count > 0u)
        {
            glPointSize(fviz_actor_point_size(actor) > 7.0f ? fviz_actor_point_size(actor) : 7.0f);
            glDrawArrays(FVIZ_GL_POINTS, 0, (GLsizei)resource->point_count);
        }
    }
}

FVizResult fviz_internal_gl_device_select(FVizGLDevice* device, FVizRenderer* renderer, float aspect_ratio, int x,
                                          int y, int viewport_width, int viewport_height,
                                          FVizSelectionAssociation association, FVizSize* out_actor_index,
                                          FVizSize* out_primitive_id, float* out_depth)
{
    const FVizGLFunctions* gl;
    FVizCamera* camera;
    FVizScene* scene;
    FVizMat4 view_projection;
    GLuint ids[4] = {0u, 0u, 0u, 0u};
    const GLuint clear_ids[4] = {0u, 0u, 0u, 0u};
    GLboolean multisample_was_enabled;
    GLboolean blend_was_enabled;
    GLboolean dither_was_enabled;
    GLboolean depth_was_enabled;
    GLboolean cull_was_enabled;
    GLboolean scissor_was_enabled;
    GLboolean depth_write = GL_TRUE;
    GLboolean color_write[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    GLint previous_viewport[4] = {0, 0, 0, 0};
    GLint previous_cull_face_mode = GL_BACK;
    GLint previous_polygon_mode[2] = {GL_FILL, GL_FILL};
    GLint previous_framebuffer = 0;
    GLint previous_program = 0;
    GLint previous_vao = 0;
    GLint previous_read_buffer = 0;
    GLint previous_depth_func = FVIZ_GL_LESS;
    GLfloat previous_line_width = 1.0f;
    GLfloat previous_point_size = 1.0f;
    FVizSize i;
    FVizResult result;
    const FVizBool needs_surface_depth =
        association == FVIZ_SELECTION_POINT || association == FVIZ_SELECTION_EDGE ? FVIZ_TRUE : FVIZ_FALSE;

    if (device == NULL || renderer == NULL || out_actor_index == NULL || out_primitive_id == NULL ||
        out_depth == NULL || aspect_ratio <= 0.0f || viewport_width <= 0 || viewport_height <= 0 || x < 0 || y < 0 ||
        x >= viewport_width || y >= viewport_height || association < FVIZ_SELECTION_ACTOR ||
        association > FVIZ_SELECTION_GLYPH_INSTANCE)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    result = fviz_gl_ensure_selection_target(device, viewport_width, viewport_height);
    if (result != FVIZ_OK) return result;
    scene = fviz_renderer_scene(renderer);
    camera = fviz_renderer_camera(renderer);
    if (scene == NULL || camera == NULL) return FVIZ_ERROR_NOT_FOUND;
    gl = &device->gl;
    multisample_was_enabled = glIsEnabled(FVIZ_GL_MULTISAMPLE);
    blend_was_enabled = glIsEnabled(GL_BLEND);
    dither_was_enabled = glIsEnabled(GL_DITHER);
    depth_was_enabled = glIsEnabled(GL_DEPTH_TEST);
    cull_was_enabled = glIsEnabled(GL_CULL_FACE);
    scissor_was_enabled = glIsEnabled(FVIZ_GL_SCISSOR_TEST);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
    glGetBooleanv(FVIZ_GL_COLOR_WRITEMASK, color_write);
    glGetIntegerv(FVIZ_GL_VIEWPORT, previous_viewport);
    glGetIntegerv(FVIZ_GL_CULL_FACE_MODE, &previous_cull_face_mode);
    glGetIntegerv(FVIZ_GL_POLYGON_MODE, previous_polygon_mode);
    glGetIntegerv(FVIZ_GL_FRAMEBUFFER_BINDING, &previous_framebuffer);
    glGetIntegerv(FVIZ_GL_CURRENT_PROGRAM, &previous_program);
    glGetIntegerv(FVIZ_GL_VERTEX_ARRAY_BINDING, &previous_vao);
    glGetIntegerv(FVIZ_GL_READ_BUFFER, &previous_read_buffer);
    glGetIntegerv(FVIZ_GL_DEPTH_FUNC, &previous_depth_func);
    glGetFloatv(FVIZ_GL_LINE_WIDTH, &previous_line_width);
    glGetFloatv(FVIZ_GL_POINT_SIZE, &previous_point_size);

    if (multisample_was_enabled != GL_FALSE) glDisable(FVIZ_GL_MULTISAMPLE);
    glDisable(GL_BLEND);
    glDisable(GL_DITHER);
    glDisable(FVIZ_GL_SCISSOR_TEST);
    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(FVIZ_GL_LESS);
    gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, device->selection_framebuffer);
    glViewport(0, 0, viewport_width, viewport_height);
    gl->glClearBufferuiv(FVIZ_GL_COLOR, 0, clear_ids);
    glClear(GL_DEPTH_BUFFER_BIT);
    view_projection =
        fviz_mat4_multiply(fviz_camera_projection_matrix(camera, aspect_ratio), fviz_camera_view_matrix(camera));
    gl->glUseProgram(device->selection_program);

    if (needs_surface_depth != FVIZ_FALSE)
    {
        glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        gl->glUniform1ui(device->selection_association_location, (GLuint)association + 1u);
        for (i = 0u; i < fviz_scene_actor_count(scene) && i < (FVizSize)UINT32_MAX; ++i)
        {
            const FVizActor* actor = fviz_scene_const_actor(scene, i);
            FVizGLActorResource* resource = fviz_gl_selection_prepare_actor(
                device, renderer, actor, i, association, FVIZ_TRUE, aspect_ratio, viewport_height, view_projection);
            if (resource != NULL) fviz_gl_selection_draw_actor(device, actor, resource, association, FVIZ_TRUE);
        }
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glDepthMask(GL_FALSE);
        glDepthFunc(FVIZ_GL_LEQUAL);
    }

    gl->glUniform1ui(device->selection_association_location, (GLuint)association + 1u);
    for (i = 0u; i < fviz_scene_actor_count(scene) && i < (FVizSize)UINT32_MAX; ++i)
    {
        const FVizActor* actor = fviz_scene_const_actor(scene, i);
        FVizGLActorResource* resource = fviz_gl_selection_prepare_actor(
            device, renderer, actor, i, association, FVIZ_FALSE, aspect_ratio, viewport_height, view_projection);
        if (resource != NULL) fviz_gl_selection_draw_actor(device, actor, resource, association, FVIZ_FALSE);
    }

    gl->glBindVertexArray(0u);
    gl->glUseProgram(0u);
    glReadBuffer(FVIZ_GL_COLOR_ATTACHMENT0);
    glReadPixels(x, y, 1, 1, FVIZ_GL_RGBA_INTEGER, GL_UNSIGNED_INT, ids);
    glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, out_depth);

    gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, (GLuint)previous_framebuffer);
    glReadBuffer((GLenum)previous_read_buffer);
    gl->glBindVertexArray((GLuint)previous_vao);
    gl->glUseProgram((GLuint)previous_program);
    glPolygonMode(GL_FRONT_AND_BACK, (GLenum)previous_polygon_mode[0]);
    glCullFace((GLenum)previous_cull_face_mode);
    glLineWidth(previous_line_width);
    glPointSize(previous_point_size);
    glDepthFunc((GLenum)previous_depth_func);
    glColorMask(color_write[0], color_write[1], color_write[2], color_write[3]);
    glViewport(previous_viewport[0], previous_viewport[1], previous_viewport[2], previous_viewport[3]);
    if (multisample_was_enabled != GL_FALSE) glEnable(FVIZ_GL_MULTISAMPLE);
    else
        glDisable(FVIZ_GL_MULTISAMPLE);
    if (blend_was_enabled != GL_FALSE) glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    if (dither_was_enabled != GL_FALSE) glEnable(GL_DITHER);
    else
        glDisable(GL_DITHER);
    if (depth_was_enabled != GL_FALSE) glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (cull_was_enabled != GL_FALSE) glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    if (scissor_was_enabled != GL_FALSE) glEnable(FVIZ_GL_SCISSOR_TEST);
    else
        glDisable(FVIZ_GL_SCISSOR_TEST);
    glDepthMask(depth_write);
    if (glGetError() != GL_NO_ERROR) return FVIZ_ERROR_GRAPHICS;
    if (ids[0] == 0u || ids[1] == 0u || ids[2] == 0u) return FVIZ_ERROR_NOT_FOUND;
    if (ids[1] != (GLuint)association + 1u) return FVIZ_ERROR_NOT_FOUND;
    *out_actor_index = (FVizSize)(ids[0] - 1u);
    *out_primitive_id = (FVizSize)(ids[2] - 1u);
    return FVIZ_OK;
}

static FVizResult fviz_gl_ensure_oit_buffers(FVizGLDevice* device, int width, int height, uint32_t samples)
{
    const FVizGLFunctions* gl;
    if (device == NULL || width <= 0 || height <= 0 || samples == 0u || samples > 32u)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_internal_gl_device_weighted_oit_supported(device) == FVIZ_FALSE) return FVIZ_ERROR_NOT_SUPPORTED;
    if (device->oit_width == width && device->oit_height == height && device->oit_samples == samples) return FVIZ_OK;
    gl = &device->gl;

    gl->glBindRenderbuffer(FVIZ_GL_RENDERBUFFER, device->oit_color_renderbuffer);
    if (samples > 1u)
        gl->glRenderbufferStorageMultisample(FVIZ_GL_RENDERBUFFER, (GLsizei)samples, FVIZ_GL_RGBA16F, width, height);
    else
        gl->glRenderbufferStorage(FVIZ_GL_RENDERBUFFER, FVIZ_GL_RGBA16F, width, height);

    gl->glBindRenderbuffer(FVIZ_GL_RENDERBUFFER, device->oit_depth_renderbuffer);
    if (samples > 1u)
        gl->glRenderbufferStorageMultisample(FVIZ_GL_RENDERBUFFER, (GLsizei)samples, FVIZ_GL_DEPTH24_STENCIL8, width,
                                             height);
    else
        gl->glRenderbufferStorage(FVIZ_GL_RENDERBUFFER, FVIZ_GL_DEPTH24_STENCIL8, width, height);
    gl->glBindRenderbuffer(FVIZ_GL_RENDERBUFFER, 0u);

    gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, device->oit_framebuffer);
    gl->glFramebufferRenderbuffer(FVIZ_GL_FRAMEBUFFER, FVIZ_GL_COLOR_ATTACHMENT0, FVIZ_GL_RENDERBUFFER,
                                  device->oit_color_renderbuffer);
    gl->glFramebufferRenderbuffer(FVIZ_GL_FRAMEBUFFER, FVIZ_GL_DEPTH_STENCIL_ATTACHMENT, FVIZ_GL_RENDERBUFFER,
                                  device->oit_depth_renderbuffer);
    if (gl->glCheckFramebufferStatus(FVIZ_GL_FRAMEBUFFER) != FVIZ_GL_FRAMEBUFFER_COMPLETE)
    {
        gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, 0u);
        return FVIZ_ERROR_GRAPHICS;
    }

    gl->glActiveTexture(FVIZ_GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, device->oit_accum_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    glTexImage2D(GL_TEXTURE_2D, 0, FVIZ_GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindTexture(GL_TEXTURE_2D, device->oit_reveal_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
    glTexImage2D(GL_TEXTURE_2D, 0, FVIZ_GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindTexture(GL_TEXTURE_2D, 0u);
    gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, 0u);

    device->oit_width = width;
    device->oit_height = height;
    device->oit_samples = samples;
    return glGetError() == GL_NO_ERROR ? FVIZ_OK : FVIZ_ERROR_GRAPHICS;
}

static FVizResult fviz_gl_resolve_oit_color(FVizGLDevice* device, GLuint texture, int width, int height)
{
    const FVizGLFunctions* gl = &device->gl;
    gl->glBindFramebuffer(FVIZ_GL_READ_FRAMEBUFFER, device->oit_framebuffer);
    gl->glBindFramebuffer(FVIZ_GL_DRAW_FRAMEBUFFER, device->oit_resolve_framebuffer);
    gl->glFramebufferTexture2D(FVIZ_GL_DRAW_FRAMEBUFFER, FVIZ_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);
    if (gl->glCheckFramebufferStatus(FVIZ_GL_DRAW_FRAMEBUFFER) != FVIZ_GL_FRAMEBUFFER_COMPLETE)
        return FVIZ_ERROR_GRAPHICS;
    gl->glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    return FVIZ_OK;
}

static FVizResult fviz_gl_ensure_peel_buffers(FVizGLDevice* device, int width, int height)
{
    const FVizGLFunctions* gl;
    if (device == NULL || width <= 0 || height <= 0) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (device->peel_width == width && device->peel_height == height && device->peel_framebuffer != 0u) return FVIZ_OK;
    gl = &device->gl;
    if (device->peel_framebuffer == 0u)
    {
        gl->glGenFramebuffers(1, &device->peel_framebuffer);
        gl->glGenFramebuffers(1, &device->peel_depth_framebuffer);
        glGenTextures(1, &device->peel_front_depth_texture);
        glGenTextures(1, &device->peel_back_depth_texture);
        glGenTextures(1, &device->peel_color_texture);
        gl->glGenRenderbuffers(1, &device->peel_depth_renderbuffer);
        if (device->peel_framebuffer == 0u || device->peel_depth_framebuffer == 0u ||
            device->peel_front_depth_texture == 0u || device->peel_back_depth_texture == 0u ||
            device->peel_color_texture == 0u || device->peel_depth_renderbuffer == 0u)
            return FVIZ_ERROR_GRAPHICS;
    }
    /* Depth textures store the current peeled depth (GL_DEPTH_COMPONENT24). */
    glBindTexture(GL_TEXTURE_2D, device->peel_front_depth_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, FVIZ_GL_TEXTURE_WRAP_S, FVIZ_GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, FVIZ_GL_TEXTURE_WRAP_T, FVIZ_GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, FVIZ_GL_TEXTURE_COMPARE_MODE, FVIZ_GL_NONE);
    glTexImage2D(GL_TEXTURE_2D, 0, FVIZ_GL_DEPTH_COMPONENT24, width, height, 0, FVIZ_GL_DEPTH_COMPONENT, GL_FLOAT,
                 NULL);
    glBindTexture(GL_TEXTURE_2D, device->peel_back_depth_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, FVIZ_GL_TEXTURE_WRAP_S, FVIZ_GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, FVIZ_GL_TEXTURE_WRAP_T, FVIZ_GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, FVIZ_GL_TEXTURE_COMPARE_MODE, FVIZ_GL_NONE);
    glTexImage2D(GL_TEXTURE_2D, 0, FVIZ_GL_DEPTH_COMPONENT24, width, height, 0, FVIZ_GL_DEPTH_COMPONENT, GL_FLOAT,
                 NULL);
    glBindTexture(GL_TEXTURE_2D, 0u);
    /* Color accumulation texture. */
    glBindTexture(GL_TEXTURE_2D, device->peel_color_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, FVIZ_GL_RGBA16F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
    glBindTexture(GL_TEXTURE_2D, 0u);
    /* Depth-only framebuffer holds the peeled depth texture. */
    gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, device->peel_depth_framebuffer);
    gl->glFramebufferTexture2D(FVIZ_GL_FRAMEBUFFER, FVIZ_GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                               device->peel_front_depth_texture, 0);
    glDrawBuffer(FVIZ_GL_NONE);
    glReadBuffer(FVIZ_GL_NONE);
    if (gl->glCheckFramebufferStatus(FVIZ_GL_FRAMEBUFFER) != FVIZ_GL_FRAMEBUFFER_COMPLETE)
    {
        gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, 0u);
        return FVIZ_ERROR_GRAPHICS;
    }
    /* Accumulation framebuffer: color + its own depth buffer for peeling. */
    gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, device->peel_framebuffer);
    gl->glFramebufferTexture2D(FVIZ_GL_FRAMEBUFFER, FVIZ_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               device->peel_color_texture, 0);
    gl->glBindRenderbuffer(FVIZ_GL_RENDERBUFFER, device->peel_depth_renderbuffer);
    gl->glRenderbufferStorage(FVIZ_GL_RENDERBUFFER, FVIZ_GL_DEPTH24_STENCIL8, width, height);
    gl->glFramebufferRenderbuffer(FVIZ_GL_FRAMEBUFFER, FVIZ_GL_DEPTH_ATTACHMENT, FVIZ_GL_RENDERBUFFER,
                                  device->peel_depth_renderbuffer);
    if (gl->glCheckFramebufferStatus(FVIZ_GL_FRAMEBUFFER) != FVIZ_GL_FRAMEBUFFER_COMPLETE)
    {
        gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, 0u);
        return FVIZ_ERROR_GRAPHICS;
    }
    gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, 0u);
    device->peel_width = width;
    device->peel_height = height;
    return FVIZ_OK;
}

/* Sets the main program's peel uniforms for the depth-compare path. */
static void fviz_gl_set_peel_uniforms(FVizGLDevice* device, FVizBool enabled, GLuint depth_texture, int width,
                                      int height)
{
    const FVizGLFunctions* gl = &device->gl;
    gl->glUniform1i(device->peel_enabled_location, enabled != FVIZ_FALSE ? 1 : 0);
    gl->glUniform1i(device->peel_depth_texture_location, 0);
    gl->glActiveTexture(FVIZ_GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, depth_texture);
    gl->glUniform1f(device->peel_screen_width_inv_location, width > 0 ? 1.0f / (float)width : 0.0f);
    gl->glUniform1f(device->peel_screen_height_inv_location, height > 0 ? 1.0f / (float)height : 0.0f);
}

/* Lazily allocates the unit-cube vertex/index buffers used to bound the
 * volume ray-cast. The cube spans [0,1]^3 in object (texture) space. */
static FVizResult fviz_gl_ensure_volume_geometry(FVizGLDevice* device)
{
    const FVizGLFunctions* gl;
    static const float k_fviz_volume_cube_positions[8][3] = {{0.0f, 0.0f, 0.0f}, {1.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 0.0f},
                                                             {0.0f, 1.0f, 0.0f}, {0.0f, 0.0f, 1.0f}, {1.0f, 0.0f, 1.0f},
                                                             {1.0f, 1.0f, 1.0f}, {0.0f, 1.0f, 1.0f}};
    static const uint32_t k_fviz_volume_cube_indices[36] = {0u, 1u, 2u, 0u, 2u, 3u, 4u, 6u, 5u, 4u, 7u, 6u,
                                                            0u, 4u, 5u, 0u, 5u, 1u, 1u, 5u, 6u, 1u, 6u, 2u,
                                                            2u, 6u, 7u, 2u, 7u, 3u, 3u, 7u, 4u, 3u, 4u, 0u};
    if (device == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (device->volume_vao != 0u) return FVIZ_OK;
    gl = &device->gl;
    gl->glGenVertexArrays(1, &device->volume_vao);
    if (device->volume_vao == 0u) return FVIZ_ERROR_GRAPHICS;
    gl->glBindVertexArray(device->volume_vao);
    gl->glGenBuffers(1, &device->volume_vbo);
    gl->glBindBuffer(FVIZ_GL_ARRAY_BUFFER, device->volume_vbo);
    gl->glBufferData(FVIZ_GL_ARRAY_BUFFER, (GLsizeiptr)sizeof(k_fviz_volume_cube_positions),
                     k_fviz_volume_cube_positions, FVIZ_GL_STATIC_DRAW);
    gl->glEnableVertexAttribArray(0u);
    gl->glVertexAttribPointer(0u, 3, GL_FLOAT, GL_FALSE, 0, (const void*)0);
    gl->glGenBuffers(1, &device->volume_ibo);
    gl->glBindBuffer(FVIZ_GL_ELEMENT_ARRAY_BUFFER, device->volume_ibo);
    gl->glBufferData(FVIZ_GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)sizeof(k_fviz_volume_cube_indices),
                     k_fviz_volume_cube_indices, FVIZ_GL_STATIC_DRAW);
    gl->glBindVertexArray(0u);
    return FVIZ_OK;
}

/* Builds the transfer function LUT (256x1 RGBA8) from the mapper's color and
 * opacity control points and uploads it once per transfer-function mtime. */
static FVizResult fviz_gl_volume_upload_transfer(FVizGLDevice* device, const FVizVolumeMapper* mapper)
{
    const FVizGLFunctions* gl;
    uint8_t lut[256 * 4];
    FVizSize count;
    FVizSize i;
    if (device == NULL || mapper == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (device->volume_transfer_texture == 0u)
    {
        glGenTextures(1, &device->volume_transfer_texture);
        if (device->volume_transfer_texture == 0u) return FVIZ_ERROR_GRAPHICS;
        glBindTexture(GL_TEXTURE_2D, device->volume_transfer_texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, 0x812F);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, 0x812F);
        glBindTexture(GL_TEXTURE_2D, 0u);
    }
    count = fviz_volume_mapper_color_point_count(mapper);
    for (i = 0u; i < 256u; ++i)
    {
        const float s = (float)i / 255.0f;
        FVizVolumeColorPoint color_point;
        FVizVolumeOpacityPoint opacity_point;
        float scalar;
        float red = 0.5f;
        float green = 0.5f;
        float blue = 0.5f;
        float alpha = 0.0f;
        float minimum;
        float maximum;
        FVizSize low;
        FVizSize high;
        if (count == 0u)
        {
            red = 0.5f + 0.5f * sinf(s * 3.14159265f);
            green = 0.4f + 0.4f * cosf(s * 2.5f);
            blue = 0.6f + 0.4f * sinf(s * 2.0f + 0.5f);
            alpha = s;
        }
        else
        {
            fviz_volume_mapper_get_scalar_range(mapper, &minimum, &maximum);
            scalar = minimum + s * (maximum - minimum);
            low = 0u;
            high = count;
            while (low < high)
            {
                const FVizSize middle = low + (high - low) / 2u;
                if (fviz_volume_mapper_color_point_at(mapper, middle, &color_point) == FVIZ_OK &&
                    color_point.scalar < scalar)
                    low = middle + 1u;
                else
                    high = middle;
            }
            if (low == 0u)
            {
                (void)fviz_volume_mapper_color_point_at(mapper, 0u, &color_point);
                red = color_point.red;
                green = color_point.green;
                blue = color_point.blue;
            }
            else if (low >= count)
            {
                (void)fviz_volume_mapper_color_point_at(mapper, count - 1u, &color_point);
                red = color_point.red;
                green = color_point.green;
                blue = color_point.blue;
            }
            else
            {
                FVizVolumeColorPoint low_point;
                FVizVolumeColorPoint high_point;
                float fraction;
                (void)fviz_volume_mapper_color_point_at(mapper, low - 1u, &low_point);
                (void)fviz_volume_mapper_color_point_at(mapper, low, &high_point);
                fraction = high_point.scalar > low_point.scalar
                               ? (scalar - low_point.scalar) / (high_point.scalar - low_point.scalar)
                               : 0.0f;
                red = low_point.red + (high_point.red - low_point.red) * fraction;
                green = low_point.green + (high_point.green - low_point.green) * fraction;
                blue = low_point.blue + (high_point.blue - low_point.blue) * fraction;
            }
            {
                const FVizSize opacity_count = fviz_volume_mapper_opacity_point_count(mapper);
                FVizSize opacity_low = 0u;
                FVizSize opacity_high = opacity_count;
                while (opacity_low < opacity_high)
                {
                    const FVizSize middle = opacity_low + (opacity_high - opacity_low) / 2u;
                    if (fviz_volume_mapper_opacity_point_at(mapper, middle, &opacity_point) == FVIZ_OK &&
                        opacity_point.scalar < scalar)
                        opacity_low = middle + 1u;
                    else
                        opacity_high = middle;
                }
                if (opacity_count == 0u)
                {
                    alpha = s;
                }
                else if (opacity_low == 0u)
                {
                    (void)fviz_volume_mapper_opacity_point_at(mapper, 0u, &opacity_point);
                    alpha = opacity_point.opacity;
                }
                else if (opacity_low >= opacity_count)
                {
                    (void)fviz_volume_mapper_opacity_point_at(mapper, opacity_count - 1u, &opacity_point);
                    alpha = opacity_point.opacity;
                }
                else
                {
                    FVizVolumeOpacityPoint low_point;
                    FVizVolumeOpacityPoint high_point;
                    float fraction;
                    (void)fviz_volume_mapper_opacity_point_at(mapper, opacity_low - 1u, &low_point);
                    (void)fviz_volume_mapper_opacity_point_at(mapper, opacity_low, &high_point);
                    fraction = high_point.scalar > low_point.scalar
                                   ? (scalar - low_point.scalar) / (high_point.scalar - low_point.scalar)
                                   : 0.0f;
                    alpha = low_point.opacity + (high_point.opacity - low_point.opacity) * fraction;
                }
            }
        }
        {
            float clamped_red = red < 0.0f ? 0.0f : (red > 1.0f ? 1.0f : red);
            float clamped_green = green < 0.0f ? 0.0f : (green > 1.0f ? 1.0f : green);
            float clamped_blue = blue < 0.0f ? 0.0f : (blue > 1.0f ? 1.0f : blue);
            float clamped_alpha = alpha < 0.0f ? 0.0f : (alpha > 1.0f ? 1.0f : alpha);
            lut[i * 4u + 0u] = (uint8_t)(clamped_red * 255.0f + 0.5f);
            lut[i * 4u + 1u] = (uint8_t)(clamped_green * 255.0f + 0.5f);
            lut[i * 4u + 2u] = (uint8_t)(clamped_blue * 255.0f + 0.5f);
            lut[i * 4u + 3u] = (uint8_t)(clamped_alpha * 255.0f + 0.5f);
        }
    }
    gl = &device->gl;
    glBindTexture(GL_TEXTURE_2D, device->volume_transfer_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, FVIZ_GL_RGBA8, 256, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, lut);
    glBindTexture(GL_TEXTURE_2D, 0u);
    return FVIZ_OK;
}

/* Uploads the active scalar array of the image data as a GL_R32F 3D texture.
 * Returns FVIZ_OK when the texture is current, FVIZ_ERROR_NOT_SUPPORTED when
 * the array layout cannot be uploaded. */
static FVizResult fviz_gl_volume_upload_scalar(FVizGLDevice* device, const FVizVolumeMapper* mapper)
{
    const FVizImageData* image;
    const FVizAttributeSet* point_data;
    const FVizDataArray* array;
    const FVizGLFunctions* gl;
    FVizSize dimensions[3];
    FVizSize point_count;
    FVizSize i;
    float* values;
    FVizBool progress = FVIZ_FALSE;
    if (device == NULL || mapper == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    image = fviz_volume_mapper_const_image_data(mapper);
    if (image == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    point_data = fviz_image_data_const_point_data(image);
    if (point_data == NULL) return FVIZ_ERROR_NOT_SUPPORTED;
    array = fviz_attribute_set_const_active(point_data, FVIZ_ATTRIBUTE_SCALARS);
    if (array == NULL) return FVIZ_ERROR_NOT_SUPPORTED;
    if (fviz_data_array_type(array) != FVIZ_DATA_FLOAT32) return FVIZ_ERROR_NOT_SUPPORTED;
    if (fviz_data_array_components(array) != 1u) return FVIZ_ERROR_NOT_SUPPORTED;
    fviz_image_data_dimensions(image, dimensions);
    point_count = fviz_data_array_tuple_count(array);
    if (dimensions[0] == 0u || dimensions[1] == 0u || dimensions[2] == 0u ||
        point_count != dimensions[0] * dimensions[1] * dimensions[2])
        return FVIZ_ERROR_INVALID_ARGUMENT;
    gl = &device->gl;
    if (device->volume_scalar_texture == 0u)
    {
        glGenTextures(1, &device->volume_scalar_texture);
        if (device->volume_scalar_texture == 0u) return FVIZ_ERROR_GRAPHICS;
    }
    values = (float*)fviz_alloc(point_count * (FVizSize)sizeof(float));
    if (values == NULL) return fviz_last_error_code();
    if (fviz_data_array_const_data(array) != NULL)
    {
        (void)memcpy(values, fviz_data_array_const_data(array), point_count * (FVizSize)sizeof(float));
        progress = FVIZ_TRUE;
    }
    else
    {
        for (i = 0u; i < point_count; ++i)
        {
            double component_value = 0.0;
            if (fviz_data_array_get_component(array, i, 0u, &component_value) == FVIZ_OK)
            {
                values[i] = (float)component_value;
                progress = FVIZ_TRUE;
            }
            else
            {
                values[i] = 0.0f;
            }
        }
    }
    if (progress == FVIZ_FALSE)
    {
        fviz_free(values);
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    glBindTexture(FVIZ_GL_TEXTURE_3D, device->volume_scalar_texture);
    glTexParameteri(FVIZ_GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(FVIZ_GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(FVIZ_GL_TEXTURE_3D, GL_TEXTURE_WRAP_S, 0x812F);
    glTexParameteri(FVIZ_GL_TEXTURE_3D, GL_TEXTURE_WRAP_T, 0x812F);
    glTexParameteri(FVIZ_GL_TEXTURE_3D, FVIZ_GL_TEXTURE_WRAP_R, 0x812F);
    gl->glTexImage3D(FVIZ_GL_TEXTURE_3D, 0, FVIZ_GL_R32F, (GLsizei)dimensions[0], (GLsizei)dimensions[1],
                     (GLsizei)dimensions[2], 0, FVIZ_GL_RED, GL_FLOAT, values);
    glBindTexture(FVIZ_GL_TEXTURE_3D, 0u);
    fviz_free(values);
    device->volume_scalar_width = (int)dimensions[0];
    device->volume_scalar_height = (int)dimensions[1];
    device->volume_scalar_depth = (int)dimensions[2];
    device->volume_texture_ready = FVIZ_TRUE;
    return FVIZ_OK;
}

static FVizMat4 fviz_gl_invert_affine(const FVizMat4* matrix)
{
    FVizMat4 inverse;
    FVizMat3 linear;
    FVizVec3 translation;
    FVizVec3 inverse_translation;
    FVizMat3 inverse_linear;
    linear.m[0] = matrix->m[0];
    linear.m[1] = matrix->m[1];
    linear.m[2] = matrix->m[2];
    linear.m[3] = matrix->m[4];
    linear.m[4] = matrix->m[5];
    linear.m[5] = matrix->m[6];
    linear.m[6] = matrix->m[8];
    linear.m[7] = matrix->m[9];
    linear.m[8] = matrix->m[10];
    inverse_linear = fviz_mat3_inverse(linear);
    translation = fviz_vec3(matrix->m[12], matrix->m[13], matrix->m[14]);
    inverse_translation = fviz_vec3(-(inverse_linear.m[0] * translation.x + inverse_linear.m[3] * translation.y +
                                      inverse_linear.m[6] * translation.z),
                                    -(inverse_linear.m[1] * translation.x + inverse_linear.m[4] * translation.y +
                                      inverse_linear.m[7] * translation.z),
                                    -(inverse_linear.m[2] * translation.x + inverse_linear.m[5] * translation.y +
                                      inverse_linear.m[8] * translation.z));
    inverse.m[0] = inverse_linear.m[0];
    inverse.m[1] = inverse_linear.m[1];
    inverse.m[2] = inverse_linear.m[2];
    inverse.m[4] = inverse_linear.m[3];
    inverse.m[5] = inverse_linear.m[4];
    inverse.m[6] = inverse_linear.m[5];
    inverse.m[8] = inverse_linear.m[6];
    inverse.m[9] = inverse_linear.m[7];
    inverse.m[10] = inverse_linear.m[8];
    inverse.m[3] = 0.0f;
    inverse.m[7] = 0.0f;
    inverse.m[11] = 0.0f;
    inverse.m[12] = inverse_translation.x;
    inverse.m[13] = inverse_translation.y;
    inverse.m[14] = inverse_translation.z;
    inverse.m[15] = 1.0f;
    return inverse;
}

/* Renders one volume actor through the ray-casting program. The unit cube is
 * drawn with a box that bounds the volume in world space, so entry/exit slab
 * intersection and object-space marching both work for any camera placement. */
static FVizResult fviz_gl_render_volume(FVizGLDevice* device, FVizRenderer* renderer, const FVizActor* actor,
                                        float aspect_ratio)
{
    const FVizVolumeMapper* mapper;
    const FVizImageData* image;
    const FVizGLFunctions* gl;
    FVizCamera* camera;
    FVizMat4 model;
    FVizMat4 inv_model;
    FVizMat4 mvp;
    FVizBounds bounds;
    FVizVec3 bounds_min;
    FVizVec3 bounds_max;
    GLfloat light_position_intensity[16];
    GLfloat light_colors[12];
    GLint light_count = 0;
    float scalar_minimum;
    float scalar_maximum;
    float step_size;
    FVizSize i;
    FVizMTime mapper_mtime;
    FVizMTime transfer_mtime;
    FVizResult result;
    if (device == NULL || renderer == NULL || actor == NULL || device->volume_program_ready == FVIZ_FALSE)
        return FVIZ_ERROR_NOT_SUPPORTED;
    mapper = fviz_actor_const_volume_mapper(actor);
    if (mapper == NULL || fviz_volume_mapper_is_empty(mapper) != FVIZ_FALSE) return FVIZ_ERROR_INVALID_ARGUMENT;
    image = fviz_volume_mapper_const_image_data(mapper);
    if (image == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    mapper_mtime = fviz_object_mtime((const FVizObject*)mapper);
    transfer_mtime = mapper_mtime;
    if (device->volume_uploaded_mapper != mapper)
    {
        result = fviz_gl_volume_upload_scalar(device, mapper);
        if (result != FVIZ_OK) return result;
        device->volume_uploaded_mapper = mapper;
        device->volume_texture_mtime = mapper_mtime;
        result = fviz_gl_volume_upload_transfer(device, mapper);
        if (result != FVIZ_OK) return result;
        device->volume_transfer_mtime = transfer_mtime;
    }
    else
    {
        if (device->volume_texture_mtime != mapper_mtime || device->volume_texture_ready == FVIZ_FALSE)
        {
            result = fviz_gl_volume_upload_scalar(device, mapper);
            if (result != FVIZ_OK) return result;
            device->volume_texture_mtime = mapper_mtime;
        }
        if (device->volume_transfer_mtime != transfer_mtime)
        {
            result = fviz_gl_volume_upload_transfer(device, mapper);
            if (result != FVIZ_OK) return result;
            device->volume_transfer_mtime = transfer_mtime;
        }
    }
    result = fviz_gl_ensure_volume_geometry(device);
    if (result != FVIZ_OK) return result;
    gl = &device->gl;
    camera = fviz_renderer_camera(renderer);
    if (camera == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    bounds = fviz_volume_mapper_bounds(mapper);
    if (bounds.valid == FVIZ_FALSE) return FVIZ_ERROR_INVALID_ARGUMENT;
    bounds_min = fviz_vec3((float)bounds.min.x, (float)bounds.min.y, (float)bounds.min.z);
    bounds_max = fviz_vec3((float)bounds.max.x, (float)bounds.max.y, (float)bounds.max.z);
    /* Map the unit cube [0,1]^3 (texture space) onto the volume's physical
     * extent, then apply the actor transform. The cube spans exactly the
     * world-space bounds used by the slab intersection. */
    {
        FVizMat4 box_to_volume;
        const FVizVec3 size = fviz_vec3((float)(bounds.max.x - bounds.min.x), (float)(bounds.max.y - bounds.min.y),
                                        (float)(bounds.max.z - bounds.min.z));
        (void)memset(&box_to_volume, 0, sizeof(box_to_volume));
        box_to_volume.m[0] = size.x;
        box_to_volume.m[5] = size.y;
        box_to_volume.m[10] = size.z;
        box_to_volume.m[12] = bounds_min.x;
        box_to_volume.m[13] = bounds_min.y;
        box_to_volume.m[14] = bounds_min.z;
        box_to_volume.m[15] = 1.0f;
        model = fviz_mat4_multiply(fviz_actor_transform_matrix(actor), box_to_volume);
    }
    inv_model = fviz_gl_invert_affine(&model);
    mvp = fviz_mat4_multiply(fviz_camera_projection_matrix(camera, aspect_ratio),
                             fviz_mat4_multiply(fviz_camera_view_matrix(camera), model));
    fviz_volume_mapper_get_scalar_range(mapper, &scalar_minimum, &scalar_maximum);
    step_size = fviz_volume_mapper_sampling_step(mapper);
    if (step_size <= 0.0f)
    {
        const FVizVec3 size = fviz_vec3((float)(bounds.max.x - bounds.min.x), (float)(bounds.max.y - bounds.min.y),
                                        (float)(bounds.max.z - bounds.min.z));
        const float diagonal = sqrtf(size.x * size.x + size.y * size.y + size.z * size.z);
        step_size = diagonal / 128.0f;
        if (step_size <= 1e-6f) step_size = 1.0f;
    }
    gl->glUseProgram(device->volume_program);
    gl->glUniformMatrix4fv(device->volume_mvp_location, 1, GL_FALSE, mvp.m);
    gl->glUniformMatrix4fv(device->volume_model_location, 1, GL_FALSE, model.m);
    gl->glUniformMatrix4fv(device->volume_inv_model_location, 1, GL_FALSE, inv_model.m);
    {
        const FVizVec3 camera_position = fviz_camera_position(camera);
        gl->glUniform3fv(device->volume_camera_position_location, 1, &camera_position.x);
    }
    gl->glUniform3fv(device->volume_bounds_min_location, 1, &bounds_min.x);
    gl->glUniform3fv(device->volume_bounds_max_location, 1, &bounds_max.x);
    gl->glUniform1f(device->volume_step_size_location, step_size);
    gl->glUniform1f(device->volume_scalar_range_location, scalar_minimum);
    gl->glUniform1f(device->volume_scalar_range_max_location, scalar_maximum);
    gl->glUniform1i(device->volume_shading_location, fviz_volume_mapper_shading(mapper) != FVIZ_FALSE ? 1 : 0);
    for (i = 0u; i < fviz_renderer_light_count(renderer) && light_count < 4; ++i)
    {
        const FVizLight* light = fviz_renderer_light_at(renderer, i);
        FVizVec3 position;
        if (light == NULL || fviz_light_enabled(light) == FVIZ_FALSE || fviz_light_intensity(light) <= 0.0f) continue;
        position =
            fviz_light_type(light) == FVIZ_LIGHT_HEADLIGHT ? fviz_camera_position(camera) : fviz_light_position(light);
        light_position_intensity[light_count * 4 + 0] = position.x;
        light_position_intensity[light_count * 4 + 1] = position.y;
        light_position_intensity[light_count * 4 + 2] = position.z;
        light_position_intensity[light_count * 4 + 3] = fviz_light_intensity(light);
        {
            float red;
            float green;
            float blue;
            fviz_light_get_color(light, &red, &green, &blue);
            light_colors[light_count * 3 + 0] = red;
            light_colors[light_count * 3 + 1] = green;
            light_colors[light_count * 3 + 2] = blue;
        }
        ++light_count;
    }
    gl->glUniform1i(device->volume_light_count_location, light_count);
    if (light_count > 0)
    {
        gl->glUniform4fv(device->volume_light_position_intensity_location, light_count, light_position_intensity);
        gl->glUniform3fv(device->volume_light_color_location, light_count, light_colors);
    }
    gl->glUniform1f(device->volume_ambient_location, 0.25f);
    gl->glUniform1f(device->volume_diffuse_location, 0.80f);
    gl->glUniform1f(device->volume_specular_location, 0.20f);
    gl->glUniform1f(device->volume_specular_power_location, 40.0f);
    gl->glActiveTexture(FVIZ_GL_TEXTURE0);
    glBindTexture(FVIZ_GL_TEXTURE_3D, device->volume_scalar_texture);
    gl->glUniform1i(device->volume_scalar_texture_location, 0);
    gl->glActiveTexture(FVIZ_GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, device->volume_transfer_texture);
    gl->glUniform1i(device->volume_transfer_texture_location, 1);
    gl->glActiveTexture(FVIZ_GL_TEXTURE0);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    gl->glBindVertexArray(device->volume_vao);
    gl->glBindBuffer(FVIZ_GL_ELEMENT_ARRAY_BUFFER, device->volume_ibo);
    glDrawElements(GL_TRIANGLES, 36, GL_UNSIGNED_INT, (const void*)0);
    gl->glBindVertexArray(0u);
    return FVIZ_OK;
}

FVizResult fviz_internal_gl_device_render_depth_peeling(FVizGLDevice* device, FVizRenderer* renderer, int viewport_x,
                                                        int viewport_y, int width, int height, uint32_t samples,
                                                        float aspect_ratio, uint32_t target_framebuffer,
                                                        uint32_t max_layers)
{
    const FVizGLFunctions* gl;
    GLboolean depth_enabled;
    GLboolean blend_enabled;
    GLboolean depth_write = GL_TRUE;
    FVizResult result;
    uint32_t layer;
    if (device == NULL || renderer == NULL || width <= 0 || height <= 0 || aspect_ratio <= 0.0f)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (samples == 0u) samples = 1u;
    if (device->peel_supported == FVIZ_FALSE) return FVIZ_ERROR_NOT_SUPPORTED;
    if (max_layers == 0u) max_layers = 4u;
    result = fviz_gl_ensure_peel_buffers(device, width, height);
    if (result != FVIZ_OK) return result;
    gl = &device->gl;
    depth_enabled = glIsEnabled(GL_DEPTH_TEST);
    blend_enabled = glIsEnabled(GL_BLEND);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
    glViewport(0, 0, width, height);

    /* Seed the peel depth from the already-rendered opaque scene. */
    gl->glBindFramebuffer(FVIZ_GL_READ_FRAMEBUFFER, (GLuint)target_framebuffer);
    gl->glBindFramebuffer(FVIZ_GL_DRAW_FRAMEBUFFER, device->peel_depth_framebuffer);
    gl->glBlitFramebuffer(viewport_x, viewport_y, viewport_x + width, viewport_y + height, 0, 0, width, height,
                          GL_DEPTH_BUFFER_BIT, GL_NEAREST);

    /* Accumulate layers front-to-back. Layer 0 passes everything; later layers
     * discard fragments at or in front of the previously peeled depth. */
    gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, device->peel_framebuffer);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClearDepth(1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    for (layer = 0u; layer < max_layers; ++layer)
    {
        glClear(GL_DEPTH_BUFFER_BIT);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(FVIZ_GL_LEQUAL);
        glDepthMask(GL_TRUE);
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        gl->glUseProgram(device->program);
        if (layer == 0u) fviz_gl_set_peel_uniforms(device, FVIZ_FALSE, 0u, width, height);
        else
            fviz_gl_set_peel_uniforms(device, FVIZ_TRUE, device->peel_front_depth_texture, width, height);
        device->peel_pass = 1;
        result = fviz_internal_gl_device_render_stage(device, renderer, aspect_ratio, width, height,
                                                      FVIZ_RENDER_PASS_TRANSLUCENT);
        device->peel_pass = 0;
        if (result != FVIZ_OK) goto cleanup;
        /* Refresh the peeled depth from the layer just drawn. */
        gl->glBindFramebuffer(FVIZ_GL_READ_FRAMEBUFFER, device->peel_framebuffer);
        gl->glBindFramebuffer(FVIZ_GL_DRAW_FRAMEBUFFER, device->peel_depth_framebuffer);
        gl->glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    }
    /* Copy the accumulated color into the target framebuffer. */
    gl->glBindFramebuffer(FVIZ_GL_READ_FRAMEBUFFER, device->peel_framebuffer);
    gl->glBindFramebuffer(FVIZ_GL_DRAW_FRAMEBUFFER, (GLuint)target_framebuffer);
    gl->glBlitFramebuffer(0, 0, width, height, viewport_x, viewport_y, viewport_x + width, viewport_y + height,
                          GL_COLOR_BUFFER_BIT, GL_NEAREST);
    result = FVIZ_OK;

cleanup:
    gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, (GLuint)target_framebuffer);
    glViewport(viewport_x, viewport_y, width, height);
    if (depth_enabled) glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (blend_enabled) glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    if (depth_write) glDepthMask(GL_TRUE);
    else
        glDepthMask(GL_FALSE);
    return result;
}

FVizResult fviz_internal_gl_device_render_weighted_oit(FVizGLDevice* device, FVizRenderer* renderer, int viewport_x,
                                                       int viewport_y, int width, int height, uint32_t samples,
                                                       float aspect_ratio, uint32_t target_framebuffer)
{
    const FVizGLFunctions* gl;
    GLboolean depth_enabled;
    GLboolean blend_enabled;
    GLboolean cull_enabled;
    GLboolean depth_write = GL_TRUE;
    FVizResult result;
    if (device == NULL || renderer == NULL || width <= 0 || height <= 0 || aspect_ratio <= 0.0f)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (samples == 0u) samples = 1u;
    result = fviz_gl_ensure_oit_buffers(device, width, height, samples);
    if (result != FVIZ_OK) return result;
    gl = &device->gl;
    depth_enabled = glIsEnabled(GL_DEPTH_TEST);
    blend_enabled = glIsEnabled(GL_BLEND);
    cull_enabled = glIsEnabled(GL_CULL_FACE);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);

    /* Seed the OIT depth attachment from the already rendered opaque scene. */
    gl->glBindFramebuffer(FVIZ_GL_READ_FRAMEBUFFER, (GLuint)target_framebuffer);
    gl->glBindFramebuffer(FVIZ_GL_DRAW_FRAMEBUFFER, device->oit_framebuffer);
    gl->glBlitFramebuffer(viewport_x, viewport_y, viewport_x + width, viewport_y + height, 0, 0, width, height,
                          GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    glViewport(0, 0, width, height);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glEnable(GL_BLEND);

    /* Accumulation pass. */
    gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, device->oit_framebuffer);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBlendFunc(GL_ONE, GL_ONE);
    device->oit_pass = 1;
    result = fviz_internal_gl_device_render_stage(device, renderer, aspect_ratio, width, height,
                                                  FVIZ_RENDER_PASS_TRANSLUCENT);
    device->oit_pass = 0;
    if (result != FVIZ_OK) goto cleanup;
    result = fviz_gl_resolve_oit_color(device, device->oit_accum_texture, width, height);
    if (result != FVIZ_OK) goto cleanup;

    /* Revealage pass. A multiplicative blend stores product(1-alpha). */
    gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, device->oit_framebuffer);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glBlendFunc(GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
    device->oit_pass = 2;
    result = fviz_internal_gl_device_render_stage(device, renderer, aspect_ratio, width, height,
                                                  FVIZ_RENDER_PASS_TRANSLUCENT);
    device->oit_pass = 0;
    if (result != FVIZ_OK) goto cleanup;
    result = fviz_gl_resolve_oit_color(device, device->oit_reveal_texture, width, height);
    if (result != FVIZ_OK) goto cleanup;

    /* Composite the order-independent translucent result over opaque color. */
    gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, (GLuint)target_framebuffer);
    glViewport(viewport_x, viewport_y, width, height);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl->glUseProgram(device->oit_composite_program);
    gl->glActiveTexture(FVIZ_GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, device->oit_accum_texture);
    gl->glUniform1i(device->oit_accum_location, 0);
    gl->glActiveTexture(FVIZ_GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, device->oit_reveal_texture);
    gl->glUniform1i(device->oit_reveal_location, 1);
    gl->glBindVertexArray(device->oit_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    ++device->frame_statistics.draw_calls;
    gl->glBindVertexArray(0u);
    gl->glUseProgram(0u);
    glBindTexture(GL_TEXTURE_2D, 0u);
    gl->glActiveTexture(FVIZ_GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, 0u);

cleanup:
    device->oit_pass = 0;
    gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, (GLuint)target_framebuffer);
    glDepthMask(depth_write);
    if (depth_enabled != GL_FALSE) glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (blend_enabled != GL_FALSE) glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    if (cull_enabled != GL_FALSE) glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    glViewport(viewport_x, viewport_y, width, height);
    if (result == FVIZ_OK && glGetError() != GL_NO_ERROR) result = FVIZ_ERROR_GRAPHICS;
    return result;
}

FVizResult fviz_internal_gl_device_apply_fxaa(FVizGLDevice* device, int width, int height,
                                              const FVizFXAAOptions* options, FVizBool srgb,
                                              uint32_t target_framebuffer)
{
    const FVizGLFunctions* gl;
    GLfloat inv_screen[4];
    const GLfloat relative_threshold = options != NULL ? options->relative_threshold : 0.125f;
    const GLfloat absolute_threshold = options != NULL ? options->absolute_threshold : 0.0312f;
    const GLfloat span_max = options != NULL ? options->span_max : 8.0f;
    GLboolean depth_enabled;
    GLboolean blend_enabled;
    GLboolean cull_enabled;
    GLboolean depth_write = GL_TRUE;
    if (device == NULL || width <= 0 || height <= 0) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (device->fxaa_program == 0u || device->fxaa_texture == 0u || device->fxaa_vao == 0u)
        return FVIZ_ERROR_NOT_SUPPORTED;
    gl = &device->gl;
    if (device->fxaa_framebuffer == 0u) return FVIZ_ERROR_NOT_SUPPORTED;
    depth_enabled = glIsEnabled(GL_DEPTH_TEST);
    blend_enabled = glIsEnabled(GL_BLEND);
    cull_enabled = glIsEnabled(GL_CULL_FACE);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
    glBindTexture(GL_TEXTURE_2D, device->fxaa_texture);
    if (device->fxaa_width != width || device->fxaa_height != height ||
        device->fxaa_srgb != (srgb != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE))
    {
        glTexImage2D(GL_TEXTURE_2D, 0, srgb != FVIZ_FALSE ? FVIZ_GL_SRGB8_ALPHA8 : FVIZ_GL_RGBA8, width, height, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, NULL);
        device->fxaa_width = width;
        device->fxaa_height = height;
        device->fxaa_srgb = srgb != FVIZ_FALSE ? FVIZ_TRUE : FVIZ_FALSE;
        gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, device->fxaa_framebuffer);
        gl->glFramebufferTexture2D(FVIZ_GL_FRAMEBUFFER, FVIZ_GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, device->fxaa_texture,
                                   0);
        if (gl->glCheckFramebufferStatus(FVIZ_GL_FRAMEBUFFER) != FVIZ_GL_FRAMEBUFFER_COMPLETE)
        {
            gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, (GLuint)target_framebuffer);
            return FVIZ_ERROR_GRAPHICS;
        }
        gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, (GLuint)target_framebuffer);
    }
    gl->glBindFramebuffer(FVIZ_GL_READ_FRAMEBUFFER, (GLuint)target_framebuffer);
    /* The blit source is the back buffer of a double-buffered default
     * framebuffer (or the colour attachment of a caller FBO). A prior color
     * readback may have left glReadBuffer at GL_FRONT, which would make this
     * resolve read a stale front buffer and create a frame-to-frame feedback
     * loop; always select the correct read source first. */
    glReadBuffer(target_framebuffer != 0u ? FVIZ_GL_COLOR_ATTACHMENT0 : GL_BACK);
    gl->glBindFramebuffer(FVIZ_GL_DRAW_FRAMEBUFFER, device->fxaa_framebuffer);
    gl->glBlitFramebuffer(0, 0, width, height, 0, 0, width, height, GL_COLOR_BUFFER_BIT, GL_NEAREST);
    gl->glBindFramebuffer(FVIZ_GL_FRAMEBUFFER, (GLuint)target_framebuffer);
    glBindTexture(GL_TEXTURE_2D, device->fxaa_texture);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_BLEND);
    glDisable(GL_CULL_FACE);
    gl->glUseProgram(device->fxaa_program);
    gl->glUniform1i(device->fxaa_color_location, 0);
    inv_screen[0] = 1.0f / (float)width;
    inv_screen[1] = 1.0f / (float)height;
    inv_screen[2] = (float)width;
    inv_screen[3] = (float)height;
    gl->glUniform4fv(device->fxaa_inv_screen_location, 1, inv_screen);
    gl->glUniform1f(device->fxaa_edge_threshold_location, relative_threshold);
    gl->glUniform1f(device->fxaa_edge_threshold_min_location, absolute_threshold);
    gl->glUniform1f(device->fxaa_span_max_location, span_max);
    gl->glBindVertexArray(device->fxaa_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    ++device->frame_statistics.draw_calls;
    gl->glBindVertexArray(0u);
    gl->glUseProgram(0u);
    glBindTexture(GL_TEXTURE_2D, 0u);
    glDepthMask(depth_write);
    if (depth_enabled != GL_FALSE) glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (blend_enabled != GL_FALSE) glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    if (cull_enabled != GL_FALSE) glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    return glGetError() == GL_NO_ERROR ? FVIZ_OK : FVIZ_ERROR_GRAPHICS;
}

typedef struct FVizGLTextVertex
{
    float x;
    float y;
    float z;
    float u;
    float v;
    float r;
    float g;
    float b;
    float a;
} FVizGLTextVertex;

typedef struct FVizGLTextBuildContext
{
    FVizGLTextVertex* vertices;
    FVizSize count;
    FVizSize capacity;
    const FVizFontAtlas* atlas;
    float base_x;
    float base_y;
    float base_z;
    int viewport_width;
    int viewport_height;
    float color[4];
    float content_scale;
} FVizGLTextBuildContext;

static FVizResult fviz_gl_text_visit_glyph(const FVizFontGlyph* glyph, uint32_t codepoint, float x0, float y0, float x1,
                                           float y1, void* user_data)
{
    FVizGLTextBuildContext* context = (FVizGLTextBuildContext*)user_data;
    FVizGLTextVertex* v;
    float px0;
    float py0;
    float px1;
    float py1;
    float nx0;
    float ny0;
    float nx1;
    float ny1;
    float u0;
    float v0;
    float u1;
    float v1;
    (void)codepoint;
    if (context == NULL || glyph == NULL || context->atlas == NULL || context->viewport_width <= 0 ||
        context->viewport_height <= 0)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (context->count + 6u > context->capacity) return FVIZ_ERROR_OVERFLOW;
    px0 = context->base_x + x0 * context->content_scale;
    py0 = context->base_y + y0 * context->content_scale;
    px1 = context->base_x + x1 * context->content_scale;
    py1 = context->base_y + y1 * context->content_scale;
    nx0 = 2.0f * px0 / (float)context->viewport_width - 1.0f;
    ny0 = 2.0f * py0 / (float)context->viewport_height - 1.0f;
    nx1 = 2.0f * px1 / (float)context->viewport_width - 1.0f;
    ny1 = 2.0f * py1 / (float)context->viewport_height - 1.0f;
    u0 = (float)glyph->x / (float)fviz_font_atlas_width(context->atlas);
    v0 = (float)glyph->y / (float)fviz_font_atlas_height(context->atlas);
    u1 = (float)(glyph->x + glyph->width) / (float)fviz_font_atlas_width(context->atlas);
    v1 = (float)(glyph->y + glyph->height) / (float)fviz_font_atlas_height(context->atlas);
    v = &context->vertices[context->count];
#define FVIZ_SET_TEXT_VERTEX(index_, x_, y_, u_, v_)                                                                   \
    do                                                                                                                 \
    {                                                                                                                  \
        v[(index_)].x = (x_);                                                                                          \
        v[(index_)].y = (y_);                                                                                          \
        v[(index_)].z = context->base_z;                                                                               \
        v[(index_)].u = (u_);                                                                                          \
        v[(index_)].v = (v_);                                                                                          \
        v[(index_)].r = context->color[0];                                                                             \
        v[(index_)].g = context->color[1];                                                                             \
        v[(index_)].b = context->color[2];                                                                             \
        v[(index_)].a = context->color[3];                                                                             \
    } while (0)
    FVIZ_SET_TEXT_VERTEX(0u, nx0, ny0, u0, v0);
    FVIZ_SET_TEXT_VERTEX(1u, nx1, ny0, u1, v0);
    FVIZ_SET_TEXT_VERTEX(2u, nx1, ny1, u1, v1);
    FVIZ_SET_TEXT_VERTEX(3u, nx0, ny0, u0, v0);
    FVIZ_SET_TEXT_VERTEX(4u, nx1, ny1, u1, v1);
    FVIZ_SET_TEXT_VERTEX(5u, nx0, ny1, u0, v1);
#undef FVIZ_SET_TEXT_VERTEX
    context->count += 6u;
    return FVIZ_OK;
}

static FVizResult fviz_gl_ensure_text_atlas(FVizGLDevice* device, const FVizFontAtlas* atlas)
{
    GLint unpack_alignment = 4;
    if (device == NULL || atlas == NULL || device->text_texture == 0u) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (device->text_atlas == atlas) return FVIZ_OK;
    if (fviz_font_atlas_pixels(atlas) == NULL || fviz_font_atlas_width(atlas) == 0u ||
        fviz_font_atlas_height(atlas) == 0u)
        return FVIZ_ERROR_INVALID_STATE;
    if (fviz_retain((void*)atlas) == NULL) return fviz_last_error_code();
    glGetIntegerv(0x0CF5, &unpack_alignment); /* GL_UNPACK_ALIGNMENT */
    glPixelStorei(0x0CF5, 1);
    glBindTexture(GL_TEXTURE_2D, device->text_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, 0x8229, (GLsizei)fviz_font_atlas_width(atlas),
                 (GLsizei)fviz_font_atlas_height(atlas), 0, 0x1903, GL_UNSIGNED_BYTE,
                 fviz_font_atlas_pixels(atlas)); /* GL_R8 / GL_RED */
    glPixelStorei(0x0CF5, unpack_alignment);
    glBindTexture(GL_TEXTURE_2D, 0u);
    if (glGetError() != GL_NO_ERROR)
    {
        fviz_release((void*)atlas);
        return FVIZ_ERROR_GRAPHICS;
    }
    fviz_release((void*)device->text_atlas);
    device->text_atlas = atlas;
    return FVIZ_OK;
}

static void fviz_gl_text_anchor_bounds(const FVizTextProperty* property, const FVizTextMetrics* metrics, float* out_x0,
                                       float* out_y0, float* out_x1, float* out_y1)
{
    float x0 = 0.0f;
    float y0 = 0.0f;
    if (fviz_text_property_horizontal_alignment(property) == FVIZ_TEXT_ALIGN_CENTER) x0 = -0.5f * metrics->width;
    else if (fviz_text_property_horizontal_alignment(property) == FVIZ_TEXT_ALIGN_RIGHT)
        x0 = -metrics->width;
    if (fviz_text_property_vertical_alignment(property) == FVIZ_TEXT_ALIGN_MIDDLE) y0 = -0.5f * metrics->height;
    else if (fviz_text_property_vertical_alignment(property) == FVIZ_TEXT_ALIGN_TOP)
        y0 = -metrics->height;
    *out_x0 = x0;
    *out_y0 = y0;
    *out_x1 = x0 + metrics->width;
    *out_y1 = y0 + metrics->height;
}

static FVizResult fviz_gl_draw_text_vertices(FVizGLDevice* device, const FVizGLTextVertex* vertices,
                                             FVizSize vertex_count, const FVizFontAtlas* atlas, FVizBool depth_test,
                                             FVizBool solid)
{
    const FVizGLFunctions* gl;
    GLboolean depth_enabled;
    GLboolean blend_enabled;
    GLboolean cull_enabled;
    GLboolean depth_write = GL_TRUE;
    GLint active_texture = (GLint)FVIZ_GL_TEXTURE0;
    GLint texture_binding = 0;
    GLsizeiptr bytes;
    if (device == NULL || vertices == NULL || vertex_count == 0u || atlas == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (device->text_program == 0u || vertex_count > (FVizSize)INT_MAX ||
        vertex_count > (FVizSize)(PTRDIFF_MAX / (ptrdiff_t)sizeof(FVizGLTextVertex)))
        return FVIZ_ERROR_NOT_SUPPORTED;
    if (fviz_gl_ensure_text_atlas(device, atlas) != FVIZ_OK) return fviz_last_error_code();
    bytes = (GLsizeiptr)(vertex_count * sizeof(FVizGLTextVertex));
    gl = &device->gl;
    gl->glBindVertexArray(device->text_vao);
    gl->glBindBuffer(FVIZ_GL_ARRAY_BUFFER, device->text_vbo);
    if (device->text_capacity_bytes < bytes)
    {
        gl->glBufferData(FVIZ_GL_ARRAY_BUFFER, bytes, vertices, FVIZ_GL_DYNAMIC_DRAW);
        device->text_capacity_bytes = bytes;
    }
    else
        gl->glBufferSubData(FVIZ_GL_ARRAY_BUFFER, 0, bytes, vertices);
    gl->glVertexAttribPointer(0u, 3, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(FVizGLTextVertex), (const void*)0);
    gl->glEnableVertexAttribArray(0u);
    gl->glVertexAttribPointer(1u, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(FVizGLTextVertex),
                              (const void*)(3u * sizeof(float)));
    gl->glEnableVertexAttribArray(1u);
    gl->glVertexAttribPointer(2u, 4, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(FVizGLTextVertex),
                              (const void*)(5u * sizeof(float)));
    gl->glEnableVertexAttribArray(2u);
    ++device->frame_statistics.gpu_uploads;
    device->frame_statistics.gpu_upload_bytes += (uint64_t)bytes;
    depth_enabled = glIsEnabled(GL_DEPTH_TEST);
    blend_enabled = glIsEnabled(GL_BLEND);
    cull_enabled = glIsEnabled(GL_CULL_FACE);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
    glGetIntegerv(0x84E0, &active_texture); /* GL_ACTIVE_TEXTURE */
    gl->glActiveTexture(FVIZ_GL_TEXTURE0);
    glGetIntegerv(0x8069, &texture_binding); /* GL_TEXTURE_BINDING_2D */
    if (depth_test != FVIZ_FALSE) glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    gl->glUseProgram(device->text_program);
    gl->glUniform1i(device->text_atlas_location, 0);
    gl->glUniform1i(device->text_solid_location, solid != FVIZ_FALSE ? 1 : 0);
    glBindTexture(GL_TEXTURE_2D, device->text_texture);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertex_count);
    ++device->frame_statistics.draw_calls;
    glBindTexture(GL_TEXTURE_2D, (GLuint)texture_binding);
    gl->glActiveTexture((GLenum)active_texture);
    gl->glUseProgram(0u);
    gl->glBindBuffer(FVIZ_GL_ARRAY_BUFFER, 0u);
    gl->glBindVertexArray(0u);
    glDepthMask(depth_write);
    if (depth_enabled != GL_FALSE) glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (blend_enabled != GL_FALSE) glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    if (cull_enabled != GL_FALSE) glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    return glGetError() == GL_NO_ERROR ? FVIZ_OK : FVIZ_ERROR_GRAPHICS;
}

static FVizGLTextVertex* fviz_gl_text_staging_vertices(FVizGLDevice* device, FVizSize required_vertices)
{
    FVizSize capacity;
    FVizSize bytes;
    void* memory;
    if (device == NULL || required_vertices == 0u) return NULL;
    if (required_vertices <= device->text_staging_capacity_vertices) return (FVizGLTextVertex*)device->text_staging;
    capacity = device->text_staging_capacity_vertices != 0u ? device->text_staging_capacity_vertices : 256u;
    while (capacity < required_vertices)
    {
        FVizSize next = capacity + capacity / 2u + 64u;
        if (next <= capacity || next > SIZE_MAX / sizeof(FVizGLTextVertex))
        {
            capacity = required_vertices;
            break;
        }
        capacity = next;
    }
    if (capacity > SIZE_MAX / sizeof(FVizGLTextVertex))
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "Text staging buffer size overflow.");
        return NULL;
    }
    bytes = capacity * sizeof(FVizGLTextVertex);
    memory = fviz_realloc(device->text_staging, bytes);
    if (memory == NULL) return NULL;
    device->text_staging = memory;
    device->text_staging_capacity_vertices = capacity;
    return (FVizGLTextVertex*)memory;
}

static FVizResult fviz_gl_draw_text_string(FVizGLDevice* device, const FVizTextProperty* property, const char* text,
                                           float base_x, float base_y, float ndc_z, int width, int height,
                                           float content_scale, FVizBool depth_test)
{
    FVizTextMetrics metrics;
    FVizGLTextBuildContext context;
    FVizGLTextVertex* vertices;
    const FVizFontAtlas* atlas;
    FVizSize capacity;
    FVizResult result;
    float red, green, blue, alpha;
    float bg_r, bg_g, bg_b, bg_a;
    if (device == NULL || property == NULL || text == NULL || width <= 0 || height <= 0 || !isfinite(content_scale) ||
        content_scale <= 0.0f)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (*text == '\0') return FVIZ_OK;
    result = fviz_text_measure_utf8(property, text, &metrics);
    if (result != FVIZ_OK || metrics.glyph_count == 0u) return result;
    if (metrics.glyph_count > (FVizSize)(SIZE_MAX / (6u * sizeof(FVizGLTextVertex)))) return FVIZ_ERROR_OVERFLOW;
    capacity = metrics.glyph_count * 6u;
    vertices = fviz_gl_text_staging_vertices(device, capacity);
    if (vertices == NULL) return fviz_last_error_code();
    atlas = fviz_font_const_atlas(fviz_text_property_const_font(property));
    (void)memset(&context, 0, sizeof(context));
    context.vertices = vertices;
    context.capacity = capacity;
    context.atlas = atlas;
    context.base_x = base_x;
    context.base_y = base_y;
    context.base_z = ndc_z;
    context.viewport_width = width;
    context.viewport_height = height;
    context.content_scale = content_scale;
    fviz_text_property_get_color(property, &red, &green, &blue, &alpha);
    context.color[0] = red;
    context.color[1] = green;
    context.color[2] = blue;
    context.color[3] = alpha;
    fviz_text_property_get_background(property, &bg_r, &bg_g, &bg_b, &bg_a);
    if (bg_a > 0.001f)
    {
        FVizGLTextVertex quad[6];
        float x0, y0, x1, y1;
        FVizSize q;
        fviz_gl_text_anchor_bounds(property, &metrics, &x0, &y0, &x1, &y1);
        x0 = (x0 - 2.0f) * content_scale;
        y0 = (y0 - 2.0f) * content_scale;
        x1 = (x1 + 2.0f) * content_scale;
        y1 = (y1 + 2.0f) * content_scale;
        for (q = 0u; q < 6u; ++q)
        {
            quad[q].u = 0;
            quad[q].v = 0;
            quad[q].r = bg_r;
            quad[q].g = bg_g;
            quad[q].b = bg_b;
            quad[q].a = bg_a;
            quad[q].z = ndc_z;
        }
#define FVIZ_BGXY(i_, px_, py_)                                                                                        \
    do                                                                                                                 \
    {                                                                                                                  \
        quad[(i_)].x = 2.0f * (base_x + (px_)) / (float)width - 1.0f;                                                  \
        quad[(i_)].y = 2.0f * (base_y + (py_)) / (float)height - 1.0f;                                                 \
    } while (0)
        FVIZ_BGXY(0u, x0, y0);
        FVIZ_BGXY(1u, x1, y0);
        FVIZ_BGXY(2u, x1, y1);
        FVIZ_BGXY(3u, x0, y0);
        FVIZ_BGXY(4u, x1, y1);
        FVIZ_BGXY(5u, x0, y1);
#undef FVIZ_BGXY
        result = fviz_gl_draw_text_vertices(device, quad, 6u, atlas, depth_test, FVIZ_TRUE);
        if (result != FVIZ_OK) return result;
    }
    if (fviz_text_property_shadow(property) != FVIZ_FALSE)
    {
        float sx, sy, so;
        fviz_text_property_get_shadow(property, &sx, &sy, &so);
        context.base_x = base_x + sx * content_scale;
        context.base_y = base_y + sy * content_scale;
        context.count = 0u;
        context.color[0] = 0;
        context.color[1] = 0;
        context.color[2] = 0;
        context.color[3] = alpha * so;
        result = fviz_internal_text_layout_visit(property, text, fviz_gl_text_visit_glyph, &context, NULL);
        if (result == FVIZ_OK)
            result = fviz_gl_draw_text_vertices(device, vertices, context.count, atlas, depth_test, FVIZ_FALSE);
        if (result != FVIZ_OK) return result;
    }
    context.base_x = base_x;
    context.base_y = base_y;
    context.count = 0u;
    context.color[0] = red;
    context.color[1] = green;
    context.color[2] = blue;
    context.color[3] = alpha;
    result = fviz_internal_text_layout_visit(property, text, fviz_gl_text_visit_glyph, &context, NULL);
    if (result == FVIZ_OK)
        result = fviz_gl_draw_text_vertices(device, vertices, context.count, atlas, depth_test, FVIZ_FALSE);
    return result;
}

static FVizResult fviz_gl_draw_label_set_3d(FVizGLDevice* device, FVizRenderer* renderer, FVizLabelSet3D* label_set,
                                            int width, int height, float content_scale)
{
    const FVizTextProperty* property;
    const FVizFontAtlas* atlas;
    const FVizLabelSet3DEntry* entries;
    FVizSize label_count;
    FVizSize total_glyphs = 0u;
    FVizSize i;
    FVizGLTextVertex* vertices;
    FVizGLTextBuildContext context;
    float ox, oy;
    float red, green, blue, alpha;
    float bg_r, bg_g, bg_b, bg_a;
    FVizResult result;
    if (device == NULL || renderer == NULL || label_set == NULL || width <= 0 || height <= 0 ||
        !isfinite(content_scale) || content_scale <= 0.0f)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (fviz_label_set_3d_visible(label_set) == FVIZ_FALSE) return FVIZ_OK;
    label_count = fviz_label_set_3d_count(label_set);
    if (label_count == 0u) return FVIZ_OK;
    property = fviz_label_set_3d_const_text_property(label_set);
    atlas = fviz_font_const_atlas(fviz_text_property_const_font(property));
    entries = fviz_internal_label_set_3d_entries(label_set);
    if (property == NULL || atlas == NULL || entries == NULL) return FVIZ_ERROR_INVALID_STATE;

    /* Background/shadow need additional per-label geometry. Keep correctness first and
       use the single-label renderer for these uncommon styles. */
    fviz_text_property_get_background(property, &bg_r, &bg_g, &bg_b, &bg_a);
    (void)bg_r;
    (void)bg_g;
    (void)bg_b;
    if (bg_a > 0.001f || fviz_text_property_shadow(property) != FVIZ_FALSE)
    {
        fviz_label_set_3d_get_pixel_offset(label_set, &ox, &oy);
        for (i = 0u; i < label_count; ++i)
        {
            FVizVec3 view, ndc;
            const char* text = fviz_string_c_str(entries[i].text);
            if (text == NULL || *text == '\0') continue;
            if (fviz_renderer_world_to_view(renderer, entries[i].position, &view) != FVIZ_OK) continue;
            if (fviz_renderer_view_to_ndc(renderer, view, (float)width / (float)height, &ndc) != FVIZ_OK) continue;
            if (ndc.z < -1.0f || ndc.z > 1.0f) continue;
            result = fviz_gl_draw_text_string(device, property, text,
                                              (ndc.x + 1.0f) * 0.5f * (float)width + ox * content_scale,
                                              (ndc.y + 1.0f) * 0.5f * (float)height + oy * content_scale, ndc.z, width,
                                              height, content_scale, fviz_label_set_3d_depth_test(label_set));
            if (result != FVIZ_OK) return result;
        }
        return FVIZ_OK;
    }

    for (i = 0u; i < label_count; ++i)
    {
        FVizTextMetrics metrics;
        const char* text = fviz_string_c_str(entries[i].text);
        if (text == NULL || *text == '\0') continue;
        if (fviz_text_measure_utf8(property, text, &metrics) != FVIZ_OK) return fviz_last_error_code();
        if (metrics.glyph_count > (SIZE_MAX - total_glyphs)) return FVIZ_ERROR_OVERFLOW;
        total_glyphs += metrics.glyph_count;
    }
    if (total_glyphs == 0u) return FVIZ_OK;
    if (total_glyphs > SIZE_MAX / 6u) return FVIZ_ERROR_OVERFLOW;
    vertices = fviz_gl_text_staging_vertices(device, total_glyphs * 6u);
    if (vertices == NULL) return fviz_last_error_code();

    (void)memset(&context, 0, sizeof(context));
    context.vertices = vertices;
    context.capacity = total_glyphs * 6u;
    context.atlas = atlas;
    context.viewport_width = width;
    context.viewport_height = height;
    context.content_scale = content_scale;
    fviz_text_property_get_color(property, &red, &green, &blue, &alpha);
    context.color[0] = red;
    context.color[1] = green;
    context.color[2] = blue;
    context.color[3] = alpha;
    fviz_label_set_3d_get_pixel_offset(label_set, &ox, &oy);
    for (i = 0u; i < label_count; ++i)
    {
        FVizVec3 view, ndc;
        const char* text = fviz_string_c_str(entries[i].text);
        if (text == NULL || *text == '\0') continue;
        if (fviz_renderer_world_to_view(renderer, entries[i].position, &view) != FVIZ_OK) continue;
        if (fviz_renderer_view_to_ndc(renderer, view, (float)width / (float)height, &ndc) != FVIZ_OK) continue;
        if (ndc.z < -1.0f || ndc.z > 1.0f) continue;
        context.base_x = (ndc.x + 1.0f) * 0.5f * (float)width + ox * content_scale;
        context.base_y = (ndc.y + 1.0f) * 0.5f * (float)height + oy * content_scale;
        context.base_z = ndc.z;
        result = fviz_internal_text_layout_visit(property, text, fviz_gl_text_visit_glyph, &context, NULL);
        if (result != FVIZ_OK) return result;
    }
    if (context.count == 0u) return FVIZ_OK;
    return fviz_gl_draw_text_vertices(device, vertices, context.count, atlas, fviz_label_set_3d_depth_test(label_set),
                                      FVIZ_FALSE);
}

FVizResult fviz_internal_gl_device_render_text_actors(FVizGLDevice* device, FVizRenderer* renderer, int width,
                                                      int height, float content_scale)
{
    FVizSize i;
    if (device == NULL || renderer == NULL || width <= 0 || height <= 0 || !isfinite(content_scale) ||
        content_scale <= 0.0f)
        return FVIZ_ERROR_INVALID_ARGUMENT;
    if (device->text_program == 0u) return FVIZ_ERROR_NOT_SUPPORTED;
    for (i = 0u; i < fviz_renderer_label_set_3d_count(renderer); ++i)
    {
        FVizLabelSet3D* label_set = fviz_renderer_label_set_3d_at(renderer, i);
        FVizResult result;
        if (label_set == NULL) continue;
        result = fviz_gl_draw_label_set_3d(device, renderer, label_set, width, height, content_scale);
        if (result != FVIZ_OK) return result;
    }
    for (i = 0u; i < fviz_renderer_billboard_text_actor_3d_count(renderer); ++i)
    {
        FVizBillboardTextActor3D* actor = fviz_renderer_billboard_text_actor_3d_at(renderer, i);
        FVizVec3 view, ndc;
        float ox, oy;
        FVizResult result;
        if (actor == NULL || fviz_billboard_text_actor_3d_is_visible(actor) == FVIZ_FALSE) continue;
        if (fviz_renderer_world_to_view(renderer, fviz_billboard_text_actor_3d_world_position(actor), &view) != FVIZ_OK)
            continue;
        if (fviz_renderer_view_to_ndc(renderer, view, (float)width / (float)height, &ndc) != FVIZ_OK) continue;
        if (ndc.z < -1.0f || ndc.z > 1.0f) continue;
        fviz_billboard_text_actor_3d_get_pixel_offset(actor, &ox, &oy);
        result = fviz_gl_draw_text_string(device, fviz_billboard_text_actor_3d_const_text_property(actor),
                                          fviz_billboard_text_actor_3d_text(actor),
                                          (ndc.x + 1.0f) * 0.5f * (float)width + ox * content_scale,
                                          (ndc.y + 1.0f) * 0.5f * (float)height + oy * content_scale, ndc.z, width,
                                          height, content_scale, fviz_billboard_text_actor_3d_depth_test(actor));
        if (result != FVIZ_OK) return result;
    }
    for (i = 0u; i < fviz_renderer_text_actor_2d_count(renderer); ++i)
    {
        FVizTextActor2D* actor = fviz_renderer_text_actor_2d_at(renderer, i);
        float x, y;
        FVizResult result;
        if (actor == NULL || fviz_text_actor_2d_is_visible(actor) == FVIZ_FALSE) continue;
        fviz_text_actor_2d_get_position(actor, &x, &y);
        if (fviz_text_actor_2d_coordinate_system(actor) == FVIZ_TEXT_COORDINATE_NORMALIZED_VIEWPORT)
        {
            x *= (float)width;
            y *= (float)height;
        }
        else
        {
            x *= content_scale;
            y *= content_scale;
        }
        result = fviz_gl_draw_text_string(device, fviz_text_actor_2d_const_text_property(actor),
                                          fviz_text_actor_2d_text(actor), x, y, -0.95f, width, height, content_scale,
                                          FVIZ_FALSE);
        if (result != FVIZ_OK) return result;
    }
    return FVIZ_OK;
}

typedef struct FVizGL2DVertex
{
    float x;
    float y;
    float r;
    float g;
    float b;
} FVizGL2DVertex;

static void fviz_gl2d_emit_quad(FVizGL2DVertex* vertices, FVizSize* count, float x0, float y0, float x1, float y1,
                                float r, float g, float b)
{
    const FVizSize index = *count;
    vertices[index + 0u] = (FVizGL2DVertex){x0, y0, r, g, b};
    vertices[index + 1u] = (FVizGL2DVertex){x1, y0, r, g, b};
    vertices[index + 2u] = (FVizGL2DVertex){x1, y1, r, g, b};
    vertices[index + 3u] = (FVizGL2DVertex){x0, y0, r, g, b};
    vertices[index + 4u] = (FVizGL2DVertex){x1, y1, r, g, b};
    vertices[index + 5u] = (FVizGL2DVertex){x0, y1, r, g, b};
    *count += 6u;
}

static FVizResult fviz_gl_draw_2d_vertices(FVizGLDevice* device, const FVizGL2DVertex* vertices, FVizSize vertex_count,
                                           const FVizMat4* matrix)
{
    const FVizGLFunctions* gl;
    GLboolean depth_enabled;
    GLboolean cull_enabled;
    GLboolean blend_enabled;
    GLboolean depth_write = GL_TRUE;
    GLsizeiptr bytes;
    if (device == NULL || vertices == NULL || vertex_count == 0u || matrix == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    if (vertex_count > (FVizSize)INT_MAX) return FVIZ_ERROR_OVERFLOW;
    if (vertex_count > (FVizSize)(PTRDIFF_MAX / (ptrdiff_t)sizeof(FVizGL2DVertex))) return FVIZ_ERROR_OVERFLOW;
    bytes = (GLsizeiptr)(vertex_count * sizeof(FVizGL2DVertex));
    gl = &device->gl;
    if (device->overlay_vao == 0u)
    {
        gl->glGenVertexArrays(1, &device->overlay_vao);
        gl->glGenBuffers(1, &device->overlay_vbo);
        if (device->overlay_vao == 0u || device->overlay_vbo == 0u) return FVIZ_ERROR_GRAPHICS;
        gl->glBindVertexArray(device->overlay_vao);
        gl->glBindBuffer(FVIZ_GL_ARRAY_BUFFER, device->overlay_vbo);
        gl->glVertexAttribPointer(0u, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(FVizGL2DVertex), (const void*)0);
        gl->glEnableVertexAttribArray(0u);
        gl->glVertexAttribPointer(1u, 3, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(FVizGL2DVertex),
                                  (const void*)(2 * sizeof(float)));
        gl->glEnableVertexAttribArray(1u);
    }
    else
    {
        gl->glBindVertexArray(device->overlay_vao);
        gl->glBindBuffer(FVIZ_GL_ARRAY_BUFFER, device->overlay_vbo);
    }
    if (bytes > device->overlay_capacity_bytes)
    {
        gl->glBufferData(FVIZ_GL_ARRAY_BUFFER, bytes, vertices, FVIZ_GL_DYNAMIC_DRAW);
        device->overlay_capacity_bytes = bytes;
    }
    else
        gl->glBufferSubData(FVIZ_GL_ARRAY_BUFFER, 0, bytes, vertices);
    ++device->frame_statistics.gpu_uploads;
    device->frame_statistics.gpu_upload_bytes += (uint64_t)bytes;

    depth_enabled = glIsEnabled(GL_DEPTH_TEST);
    cull_enabled = glIsEnabled(GL_CULL_FACE);
    blend_enabled = glIsEnabled(GL_BLEND);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_write);
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    gl->glUseProgram(device->program_2d);
    gl->glUniformMatrix4fv(device->mvp_location_2d, 1, GL_FALSE, matrix->m);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertex_count);
    ++device->frame_statistics.draw_calls;
    gl->glUseProgram(0u);
    gl->glBindBuffer(FVIZ_GL_ARRAY_BUFFER, 0u);
    gl->glBindVertexArray(0u);
    glDepthMask(depth_write);
    if (depth_enabled != GL_FALSE) glEnable(GL_DEPTH_TEST);
    else
        glDisable(GL_DEPTH_TEST);
    if (cull_enabled != GL_FALSE) glEnable(GL_CULL_FACE);
    else
        glDisable(GL_CULL_FACE);
    if (blend_enabled != GL_FALSE) glEnable(GL_BLEND);
    else
        glDisable(GL_BLEND);
    return glGetError() == GL_NO_ERROR ? FVIZ_OK : FVIZ_ERROR_GRAPHICS;
}

FVizResult fviz_internal_gl_device_render_gradient_background(FVizGLDevice* device, const float bottom_color[3],
                                                              const float top_color[3])
{
    FVizGL2DVertex vertices[6];
    FVizMat4 identity = fviz_mat4_identity();
    if (device == NULL || bottom_color == NULL || top_color == NULL) return FVIZ_ERROR_INVALID_ARGUMENT;
    vertices[0] = (FVizGL2DVertex){-1.0f, -1.0f, bottom_color[0], bottom_color[1], bottom_color[2]};
    vertices[1] = (FVizGL2DVertex){1.0f, -1.0f, bottom_color[0], bottom_color[1], bottom_color[2]};
    vertices[2] = (FVizGL2DVertex){1.0f, 1.0f, top_color[0], top_color[1], top_color[2]};
    vertices[3] = (FVizGL2DVertex){-1.0f, -1.0f, bottom_color[0], bottom_color[1], bottom_color[2]};
    vertices[4] = (FVizGL2DVertex){1.0f, 1.0f, top_color[0], top_color[1], top_color[2]};
    vertices[5] = (FVizGL2DVertex){-1.0f, 1.0f, top_color[0], top_color[1], top_color[2]};
    return fviz_gl_draw_2d_vertices(device, vertices, 6u, &identity);
}

FVizResult fviz_internal_gl_device_render_legend(FVizGLDevice* device, const FVizScalarLegend* legend, int width,
                                                 int height, float content_scale)
{
    FVizGL2DVertex vertices[512];
    FVizSize vertex_count = 0u;
    FVizMat4 ortho;
    FVizLookupTable* table;
    float bar_x;
    float bar_y;
    float bar_w;
    float bar_h;
    float margin_x;
    float margin_y;
    float top_text_extent;
    float maximum_label_width = 0.0f;
    float layout_width;
    float content_width;
    float title_width = 0.0f;
    float stats_width = 0.0f;
    /* Keep the conservative correction so the frame never overlaps glyphs. */
    const float text_extent_correction = 0.75f;
    FVizOverlayLayoutContext layout_context;
    FVizOverlayLayoutItem layout_item;
    FVizOverlayLayoutResult layout_result;
    FVizSize i;
    FVizSize segments = 32u;
    float panel_r;
    float panel_g;
    float panel_b;
    float panel_a;
    float border_r;
    float border_g;
    float border_b;
    FVizBool ticks_visible;
    float tick_length;
    FVizBool discrete;
    float title_subtitle_spacing;
    float subtitle_bar_spacing;
    float bar_label_spacing;
    float bar_statistics_spacing;

    if (device == NULL || legend == NULL || width <= 0 || height <= 0 || !isfinite(content_scale) ||
        content_scale <= 0.0f)
    {
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_scalar_legend_is_visible(legend) == FVIZ_FALSE) return FVIZ_OK;
    table = fviz_scalar_legend_lookup_table((FVizScalarLegend*)legend);
    if (table == NULL) return FVIZ_OK;

    {
        float padding_x;
        float padding_y;
        fviz_scalar_legend_get_viewport_padding(legend, &padding_x, &padding_y);
        margin_x = padding_x >= 0.0f ? padding_x * (float)width : 16.0f * content_scale;
        margin_y = padding_y >= 0.0f ? padding_y * (float)height : 16.0f * content_scale;
    }
    {
        float configured_width;
        float configured_height;
        fviz_scalar_legend_get_bar_size(legend, &configured_width, &configured_height);
        bar_w = configured_width > 0.0f ? configured_width * content_scale : 22.0f * content_scale;
        bar_h = configured_height > 0.0f ? configured_height * content_scale : (float)height * 0.5f;
    }
    if (bar_h > 420.0f * content_scale) bar_h = 420.0f * content_scale;
    discrete = fviz_scalar_legend_is_discrete(legend);
    if (discrete != FVIZ_FALSE)
        segments =
            fviz_scalar_legend_tick_count(legend) > 1u ? (FVizSize)fviz_scalar_legend_tick_count(legend) - 1u : 1u;
    fviz_scalar_legend_get_panel_color(legend, &panel_r, &panel_g, &panel_b, &panel_a);
    fviz_scalar_legend_get_border_color(legend, &border_r, &border_g, &border_b, NULL);
    fviz_scalar_legend_get_tick_style(legend, &ticks_visible, &tick_length);
    fviz_scalar_legend_get_layout_spacing(legend, &title_subtitle_spacing, &subtitle_bar_spacing, &bar_label_spacing,
                                          &bar_statistics_spacing);
    tick_length *= content_scale;
    top_text_extent = 0.45f * content_scale *
                      fviz_text_property_font_size(fviz_scalar_legend_label_text_property((FVizScalarLegend*)legend));
    if (fviz_scalar_legend_title(legend) != NULL && *fviz_scalar_legend_title(legend) != '\0')
    {
        top_text_extent =
            content_scale *
            (14.0f + fviz_text_property_font_size(fviz_scalar_legend_title_text_property((FVizScalarLegend*)legend)) +
             (fviz_scalar_legend_units(legend) != NULL && *fviz_scalar_legend_units(legend) != '\0'
                  ? fviz_text_property_font_size(fviz_scalar_legend_label_text_property((FVizScalarLegend*)legend)) +
                        3.0f
                  : 0.0f));
    }
    {
        float range_min;
        float range_max;
        uint32_t tick_count = fviz_scalar_legend_tick_count(legend);
        uint32_t tick;
        if (tick_count < 2u) tick_count = 2u;
        fviz_scalar_legend_get_range(legend, &range_min, &range_max);
        for (tick = 0u; tick < tick_count; ++tick)
        {
            char label[96];
            FVizTextMetrics metrics;
            const float t = (float)tick / (float)(tick_count - 1u);
            const float value = range_min + t * (range_max - range_min);
            if (snprintf(label, sizeof(label), fviz_scalar_legend_label_format(legend), (double)value) >= 0 &&
                fviz_text_measure_utf8(fviz_scalar_legend_label_text_property((FVizScalarLegend*)legend), label,
                                       &metrics) == FVIZ_OK &&
                metrics.width * content_scale * text_extent_correction > maximum_label_width)
                maximum_label_width = metrics.width * content_scale * text_extent_correction;
        }
    }
    layout_width = bar_w + 8.0f * content_scale + maximum_label_width;
    {
        const char* title = fviz_scalar_legend_title(legend);
        if (title != NULL && *title != '\0')
        {
            char title_buffer[256];
            FVizTextMetrics metrics;
            const char* units = fviz_scalar_legend_units(legend);
            if (units != NULL && *units != '\0')
                (void)snprintf(title_buffer, sizeof(title_buffer), "%s [%s]", title, units);
            else
                (void)snprintf(title_buffer, sizeof(title_buffer), "%s", title);
            if (fviz_text_measure_utf8(fviz_scalar_legend_title_text_property((FVizScalarLegend*)legend), title_buffer,
                                       &metrics) == FVIZ_OK &&
                metrics.width * content_scale * text_extent_correction > layout_width)
                layout_width = metrics.width * content_scale * text_extent_correction;
            title_width = metrics.width * content_scale * text_extent_correction;
        }
    }
    {
        char max_buffer[96];
        char min_buffer[96];
        FVizTextMetrics metrics;
        float range_min;
        float range_max;
        fviz_scalar_legend_get_range(legend, &range_min, &range_max);
        (void)snprintf(max_buffer, sizeof(max_buffer), "Max: %+.3e", (double)range_max);
        (void)snprintf(min_buffer, sizeof(min_buffer), "Min: %+.3e", (double)range_min);
        if (fviz_text_measure_utf8(fviz_scalar_legend_label_text_property((FVizScalarLegend*)legend), max_buffer,
                                   &metrics) == FVIZ_OK)
            stats_width = metrics.width * content_scale * text_extent_correction;
        if (fviz_text_measure_utf8(fviz_scalar_legend_label_text_property((FVizScalarLegend*)legend), min_buffer,
                                   &metrics) == FVIZ_OK &&
            metrics.width * content_scale * text_extent_correction > stats_width)
            stats_width = metrics.width * content_scale * text_extent_correction;
    }
    content_width = bar_w + tick_length + maximum_label_width + 8.0f * content_scale;
    if (title_width > content_width) content_width = title_width;
    if (stats_width > content_width) content_width = stats_width;
    layout_width = content_width + 16.0f * content_scale;
    fviz_overlay_layout_context_initialize(&layout_context);
    layout_context.window_width = (float)width;
    layout_context.window_height = (float)height;
    layout_context.viewport_width = (float)width;
    layout_context.viewport_height = (float)height;
    layout_context.content_scale = content_scale;
    fviz_overlay_layout_item_initialize(&layout_item);
    layout_item.width = layout_width;
    layout_item.height = bar_h + top_text_extent + 48.0f * content_scale;
    switch (fviz_scalar_legend_position(legend))
    {
        case FVIZ_LEGEND_TOP_LEFT:
            layout_item.anchor = fviz_vec3(margin_x / (float)width, 1.0f - margin_y / (float)height, 0.0f);
            layout_item.vertical_alignment = FVIZ_OVERLAY_ALIGN_TOP;
            break;
        case FVIZ_LEGEND_BOTTOM_RIGHT:
            layout_item.anchor = fviz_vec3(1.0f - margin_x / (float)width, margin_y / (float)height, 0.0f);
            layout_item.horizontal_alignment = FVIZ_OVERLAY_ALIGN_RIGHT;
            break;
        case FVIZ_LEGEND_BOTTOM_LEFT:
            layout_item.anchor = fviz_vec3(margin_x / (float)width, margin_y / (float)height, 0.0f);
            break;
        case FVIZ_LEGEND_TOP_RIGHT:
        default:
            layout_item.anchor = fviz_vec3(1.0f - margin_x / (float)width, 1.0f - margin_y / (float)height, 0.0f);
            layout_item.horizontal_alignment = FVIZ_OVERLAY_ALIGN_RIGHT;
            layout_item.vertical_alignment = FVIZ_OVERLAY_ALIGN_TOP;
            break;
    }
    if (fviz_overlay_layout_resolve(&layout_context, &layout_item, 1u, &layout_result) != FVIZ_OK)
        return fviz_last_error_code();
    bar_x = layout_result.x;
    bar_y = layout_result.y;

    if (panel_a > 0.001f)
        fviz_gl2d_emit_quad(vertices, &vertex_count, bar_x - 8.0f * content_scale, bar_y - 44.0f * content_scale,
                            bar_x + content_width, bar_y + bar_h + top_text_extent + 5.0f * content_scale, panel_r,
                            panel_g, panel_b);
    /* Abaqus-style bounding box and accent frames for title/statistics. */
    {
        const float box_l = bar_x - 8.0f * content_scale;
        const float box_r = bar_x + content_width;
        const float box_t = bar_y - 44.0f * content_scale;
        const float box_b = bar_y + bar_h + top_text_extent + 5.0f * content_scale;
        const float line = 1.0f * content_scale;
        fviz_gl2d_emit_quad(vertices, &vertex_count, box_l, box_t, box_r, box_t + line, border_r, border_g, border_b);
        fviz_gl2d_emit_quad(vertices, &vertex_count, box_l, box_b - line, box_r, box_b, border_r, border_g, border_b);
        fviz_gl2d_emit_quad(vertices, &vertex_count, box_l, box_t, box_l + line, box_b, border_r, border_g, border_b);
        fviz_gl2d_emit_quad(vertices, &vertex_count, box_r - line, box_t, box_r, box_b, border_r, border_g, border_b);
    }

    {
        float range_min;
        float range_max;
        fviz_lookup_table_get_range(table, &range_min, &range_max);
        for (i = 0u; i < segments; ++i)
        {
            const float f0 = (float)i / (float)segments;
            const float f1 = (float)(i + 1) / (float)segments;
            const float value = range_min + (f0 + (f1 - f0) * 0.5f) * (range_max - range_min);
            float r;
            float g;
            float b;
            fviz_lookup_table_map_scalar(table, value, &r, &g, &b);
            fviz_gl2d_emit_quad(vertices, &vertex_count, bar_x, bar_y + f0 * bar_h, bar_x + bar_w, bar_y + f1 * bar_h,
                                r, g, b);
            if (discrete != FVIZ_FALSE && i > 0u)
                fviz_gl2d_emit_quad(vertices, &vertex_count, bar_x, bar_y + f0 * bar_h - 0.5f * content_scale,
                                    bar_x + bar_w, bar_y + f0 * bar_h + 0.5f * content_scale, border_r, border_g,
                                    border_b);
        }
        fviz_gl2d_emit_quad(vertices, &vertex_count, bar_x - 2.0f * content_scale, bar_y,
                            bar_x + bar_w + 2.0f * content_scale, bar_y + 2.0f * content_scale, border_r, border_g,
                            border_b);
        fviz_gl2d_emit_quad(vertices, &vertex_count, bar_x - 2.0f * content_scale, bar_y + bar_h - 2.0f * content_scale,
                            bar_x + bar_w + 2.0f * content_scale, bar_y + bar_h, border_r, border_g, border_b);
        fviz_gl2d_emit_quad(vertices, &vertex_count, bar_x - 2.0f * content_scale, bar_y, bar_x, bar_y + bar_h,
                            border_r, border_g, border_b);
        fviz_gl2d_emit_quad(vertices, &vertex_count, bar_x + bar_w, bar_y, bar_x + bar_w + 2.0f * content_scale,
                            bar_y + bar_h, border_r, border_g, border_b);
        if (ticks_visible != FVIZ_FALSE && tick_length > 0.0f)
        {
            uint32_t tick_count = fviz_scalar_legend_tick_count(legend);
            uint32_t tick;
            for (tick = 0u; tick < tick_count; ++tick)
            {
                const float t = tick_count > 1u ? (float)tick / (float)(tick_count - 1u) : 0.0f;
                const float y = bar_y + t * bar_h;
                fviz_gl2d_emit_quad(vertices, &vertex_count, bar_x + bar_w, y - 0.5f * content_scale,
                                    bar_x + bar_w + tick_length, y + 0.5f * content_scale, border_r, border_g,
                                    border_b);
                fviz_gl2d_emit_quad(vertices, &vertex_count, bar_x + bar_w + tick_length, y - 0.5f * content_scale,
                                    bar_x + bar_w + tick_length + 4.0f * content_scale, y + 0.5f * content_scale,
                                    border_r, border_g, border_b);
            }
        }
        for (i = 0u; i < (FVizSize)fviz_scalar_legend_tick_count(legend); ++i)
        {
            const uint32_t tick_count = fviz_scalar_legend_tick_count(legend);
            const float t = tick_count > 1u ? (float)i / (float)(tick_count - 1u) : 0.0f;
            const float y = bar_y + t * bar_h;
            fviz_gl2d_emit_quad(vertices, &vertex_count, bar_x + bar_w + tick_length, y - 0.5f * content_scale,
                                bar_x + bar_w + 6.0f * content_scale, y + 0.5f * content_scale, border_r, border_g,
                                border_b);
        }
    }

    (void)memset(&ortho, 0, sizeof(ortho));
    ortho.m[0] = 2.0f / (float)width;
    ortho.m[5] = 2.0f / (float)height;
    ortho.m[10] = -1.0f;
    ortho.m[12] = -1.0f;
    ortho.m[13] = -1.0f;
    ortho.m[15] = 1.0f;

    {
        FVizResult result = fviz_gl_draw_2d_vertices(device, vertices, vertex_count, &ortho);
        if (result != FVIZ_OK) return result;
        if (device->text_program != 0u)
        {
            const char* title = fviz_scalar_legend_title(legend);
            const char* units = fviz_scalar_legend_units(legend);
            char title_buffer[256];
            uint32_t tick_count = fviz_scalar_legend_tick_count(legend);
            float range_min;
            float range_max;
            uint32_t tick;
            fviz_scalar_legend_get_range(legend, &range_min, &range_max);
            if (title != NULL && *title != '\0')
            {
                (void)snprintf(title_buffer, sizeof(title_buffer), "%s", title);
                if (units != NULL && *units != '\0')
                {
                    result = fviz_gl_draw_text_string(
                        device, fviz_scalar_legend_label_text_property((FVizScalarLegend*)legend), units, bar_x,
                        bar_y + bar_h + subtitle_bar_spacing * content_scale, -0.95f, width, height, content_scale,
                        FVIZ_FALSE);
                    if (result != FVIZ_OK) return result;
                }
                result = fviz_gl_draw_text_string(
                    device, fviz_scalar_legend_title_text_property((FVizScalarLegend*)legend), title_buffer, bar_x,
                    bar_y + bar_h + subtitle_bar_spacing * content_scale +
                        fviz_text_property_font_size(
                            fviz_scalar_legend_label_text_property((FVizScalarLegend*)legend)) *
                            content_scale +
                        title_subtitle_spacing * content_scale,
                    -0.95f, width, height, content_scale, FVIZ_FALSE);
                if (result != FVIZ_OK) return result;
            }
            {
                char max_buffer[96];
                char min_buffer[96];
                (void)snprintf(max_buffer, sizeof(max_buffer), "Max: %+.3e", (double)range_max);
                (void)snprintf(min_buffer, sizeof(min_buffer), "Min: %+.3e", (double)range_min);
                result = fviz_gl_draw_text_string(
                    device, fviz_scalar_legend_label_text_property((FVizScalarLegend*)legend), max_buffer, bar_x,
                    bar_y - bar_statistics_spacing * content_scale, -0.95f, width, height, content_scale, FVIZ_FALSE);
                if (result != FVIZ_OK) return result;
                result = fviz_gl_draw_text_string(
                    device, fviz_scalar_legend_label_text_property((FVizScalarLegend*)legend), min_buffer, bar_x,
                    bar_y - (bar_statistics_spacing +
                             fviz_text_property_font_size(
                                 fviz_scalar_legend_label_text_property((FVizScalarLegend*)legend)) +
                             4.0f) *
                                content_scale,
                    -0.95f, width, height, content_scale, FVIZ_FALSE);
                if (result != FVIZ_OK) return result;
            }
            if (tick_count < 2u) tick_count = 2u;
            for (tick = 0u; tick < tick_count; ++tick)
            {
                char label[96];
                const float t = (float)tick / (float)(tick_count - 1u);
                const float value = range_min + t * (range_max - range_min);
                const int written =
                    snprintf(label, sizeof(label), fviz_scalar_legend_label_format(legend), (double)value);
                if (written < 0) continue;
                result = fviz_gl_draw_text_string(
                    device, fviz_scalar_legend_label_text_property((FVizScalarLegend*)legend), label,
                    bar_x + bar_w + bar_label_spacing * content_scale,
                    bar_y + t * bar_h -
                        0.45f * content_scale *
                            fviz_text_property_font_size(
                                fviz_scalar_legend_label_text_property((FVizScalarLegend*)legend)),
                    -0.95f, width, height, content_scale, FVIZ_FALSE);
                if (result != FVIZ_OK) return result;
            }
        }
        return FVIZ_OK;
    }
}
