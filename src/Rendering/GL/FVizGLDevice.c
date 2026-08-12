#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Data/FVizDataArray.h>
#include <FViz/Math/FVizMat3.h>
#include <FViz/Math/FVizMat4.h>
#include <FViz/Rendering/FVizActor.h>
#include <FViz/Rendering/FVizCamera.h>
#include <FViz/Rendering/FVizLookupTable.h>
#include <FViz/Rendering/FVizMapper.h>
#include <FViz/Rendering/FVizRenderer.h>
#include <FViz/Rendering/FVizScalarLegend.h>
#include <FViz/Rendering/FVizScene.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Mesh/FVizPolyDataPrivate.h>
#include <FViz/Rendering/FVizGL.h>
#include <FViz/Rendering/FVizGLDevice.h>

#define FVIZ_GL_POSITION_ATTRIBUTE_INDEX 0u
#define FVIZ_GL_NORMAL_ATTRIBUTE_INDEX 1u
#define FVIZ_GL_COLOR_ATTRIBUTE_INDEX 2u

static const char* const k_fviz_gl_vertex_shader_source =
    "#version 330 core\n"
    "layout(location = 0) in vec3 aPosition;\n"
    "layout(location = 1) in vec3 aNormal;\n"
    "layout(location = 2) in vec3 aColor;\n"
    "uniform mat4 uMvp;\n"
    "uniform mat4 uModel;\n"
    "uniform mat3 uNormalMatrix;\n"
    "uniform int uScalarColorEnabled;\n"
    "out vec3 vNormal;\n"
    "out vec3 vWorldPos;\n"
    "out vec3 vColor;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = uMvp * vec4(aPosition, 1.0);\n"
    "    vNormal = normalize(uNormalMatrix * aNormal);\n"
    "    vWorldPos = vec3(uModel * vec4(aPosition, 1.0));\n"
    "    vColor = aColor;\n"
    "}\n";

static const char* const k_fviz_gl_fragment_shader_source =
    "#version 330 core\n"
    "in vec3 vNormal;\n"
    "in vec3 vWorldPos;\n"
    "in vec3 vColor;\n"
    "uniform vec3 uDiffuse;\n"
    "uniform vec3 uLightPosition;\n"
    "uniform vec3 uLightAmbient;\n"
    "uniform int uScalarColorEnabled;\n"
    "out vec4 outColor;\n"
    "void main()\n"
    "{\n"
    "    vec3 baseColor = uScalarColorEnabled == 1 ? vColor : uDiffuse;\n"
    "    vec3 n = normalize(vNormal);\n"
    "    if (length(n) < 0.5)\n"
    "    {\n"
    "        n = normalize(cross(dFdx(vWorldPos), dFdy(vWorldPos)));\n"
    "    }\n"
    "    vec3 l = normalize(uLightPosition - vWorldPos);\n"
    "    float diffuse = max(dot(n, l), 0.0);\n"
    "    vec3 color = baseColor * (uLightAmbient + 0.9 * diffuse);\n"
    "    outColor = vec4(color, 1.0);\n"
    "}\n";

static const char* const k_fviz_gl2d_vertex_shader_source =
    "#version 330 core\n"
    "layout(location = 0) in vec2 aPosition;\n"
    "layout(location = 1) in vec3 aColor;\n"
    "uniform mat4 uMvp;\n"
    "out vec3 vColor;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = uMvp * vec4(aPosition, 0.0, 1.0);\n"
    "    vColor = aColor;\n"
    "}\n";

static const char* const k_fviz_gl2d_fragment_shader_source =
    "#version 330 core\n"
    "in vec3 vColor;\n"
    "out vec4 outColor;\n"
    "void main()\n"
    "{\n"
    "    outColor = vec4(vColor, 1.0);\n"
    "}\n";

typedef struct FVizGLActorResource
{
    const FVizActor* actor;
    GLuint vao;
    GLuint position_buffer;
    GLuint normal_buffer;
    GLuint index_buffer;
    GLuint color_buffer;
    GLuint line_index_buffer;
    FVizBool has_color;
    GLsizei index_count;
    GLsizei line_index_count;
    uint32_t generation;
    FVizSize point_count;
} FVizGLActorResource;

struct FVizGLDevice
{
    FVizGLFunctions gl;
    GLuint program;
    GLint mvp_location;
    GLint model_location;
    GLint normal_matrix_location;
    GLint diffuse_location;
    GLint light_position_location;
    GLint light_ambient_location;
    GLint scalar_color_location;
    GLuint program_2d;
    GLint mvp_location_2d;
    FVizGLActorResource* actors;
    FVizSize actor_count;
    FVizSize actor_capacity;
};

static FVizGLActorResource* fviz_gl_find_actor_resource(FVizGLDevice* device, const FVizActor* actor)
{
    FVizSize i;
    for (i = 0u; i < device->actor_count; ++i)
    {
        if (device->actors[i].actor == actor) return &device->actors[i];
    }
    return NULL;
}

static void fviz_gl_actor_resource_destroy(FVizGLDevice* device, FVizGLActorResource* resource)
{
    const FVizGLFunctions* gl = &device->gl;
    if (resource->line_index_buffer != 0u)
    {
        gl->glDeleteBuffers(1, &resource->line_index_buffer);
        resource->line_index_buffer = 0u;
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
    resource->has_color = FVIZ_FALSE;
    resource->index_count = 0;
    resource->line_index_count = 0;
    resource->generation = 0u;
    resource->point_count = 0u;
}

static void fviz_gl_upload_buffer(
    FVizGLDevice* device,
    GLuint* buffer,
    GLenum target,
    const void* data,
    GLsizeiptr size)
{
    const FVizGLFunctions* gl = &device->gl;
    if (*buffer == 0u) gl->glGenBuffers(1, buffer);
    gl->glBindBuffer(target, *buffer);
    gl->glBufferData(target, size, data, FVIZ_GL_STATIC_DRAW);
}

static FVizResult fviz_gl_build_color_buffer(
    FVizGLDevice* device,
    FVizGLActorResource* resource,
    const FVizActor* actor,
    const FVizPolyData* poly_data,
    FVizSize point_count)
{
    const FVizGLFunctions* gl = &device->gl;
    FVizMapper* mapper = fviz_actor_mapper((FVizActor*)actor);
    const FVizLookupTable* table;
    const FVizDataArray* scalars;
    const float* scalar_data;
    FVizVec3* colors;
    FVizSize i;
    if (mapper == NULL || fviz_mapper_scalar_visibility(mapper) == FVIZ_FALSE) return FVIZ_OK;
    table = fviz_mapper_lookup_table(mapper);
    scalars = fviz_poly_data_const_scalars(poly_data);
    if (table == NULL || scalars == NULL || point_count == 0u) return FVIZ_OK;
    scalar_data = (const float*)fviz_data_array_const_data((FVizDataArray*)scalars);
    if (scalar_data == NULL) return FVIZ_OK;

    if (fviz_mapper_scalar_range_valid(mapper) == FVIZ_FALSE)
    {
        float minimum = scalar_data[0];
        float maximum = scalar_data[0];
        for (i = 1u; i < point_count; ++i)
        {
            if (scalar_data[i] < minimum) minimum = scalar_data[i];
            if (scalar_data[i] > maximum) maximum = scalar_data[i];
        }
        if (maximum <= minimum) maximum = minimum + 1.0f;
        fviz_mapper_set_scalar_range(mapper, minimum, maximum);
    }

    colors = (FVizVec3*)fviz_alloc(point_count * sizeof(FVizVec3));
    if (colors == NULL) return fviz_last_error_code();
    for (i = 0u; i < point_count; ++i)
    {
        fviz_lookup_table_map_scalar(table, scalar_data[i], &colors[i].x, &colors[i].y, &colors[i].z);
    }
    fviz_gl_upload_buffer(device, &resource->color_buffer, FVIZ_GL_ARRAY_BUFFER, colors,
        (GLsizeiptr)(point_count * sizeof(FVizVec3)));
    fviz_free(colors);
    gl->glVertexAttribPointer(FVIZ_GL_COLOR_ATTRIBUTE_INDEX, 3, GL_FLOAT, GL_FALSE,
        (GLsizei)sizeof(FVizVec3), (const void*)0);
    gl->glEnableVertexAttribArray(FVIZ_GL_COLOR_ATTRIBUTE_INDEX);
    resource->has_color = FVIZ_TRUE;
    return FVIZ_OK;
}

static FVizResult fviz_gl_ensure_actor_resource(FVizGLDevice* device, const FVizActor* actor)
{
    const FVizPolyData* poly_data;
    const FVizVec3* points;
    const FVizVec3* normals;
    const uint32_t* indices;
    const uint32_t* line_indices;
    const FVizGLFunctions* gl = &device->gl;
    FVizGLActorResource* resource;
    FVizSize point_count;
    FVizSize index_count;
    FVizSize line_index_count;
    uint32_t generation;

    poly_data = fviz_actor_const_poly_data(actor);
    if (poly_data == NULL) return FVIZ_OK;
    point_count = fviz_poly_data_point_count(poly_data);
    index_count = fviz_poly_data_triangle_count(poly_data) * 3u;
    line_index_count = fviz_poly_data_line_count(poly_data) * 2u;
    generation = fviz_internal_poly_data_generation(poly_data);
    if (point_count == 0u || (index_count == 0u && line_index_count == 0u)) return FVIZ_OK;
    if (index_count > (FVizSize)INT_MAX || line_index_count > (FVizSize)INT_MAX)
    {
        fviz_internal_set_error(FVIZ_ERROR_OVERFLOW, "poly_data index count exceeds OpenGL draw limits");
        return FVIZ_ERROR_OVERFLOW;
    }

    resource = fviz_gl_find_actor_resource(device, actor);
    if (resource != NULL && resource->generation == generation)
    {
        return FVIZ_OK;
    }

    points = fviz_poly_data_points(poly_data);
    normals = fviz_poly_data_normals(poly_data);
    indices = fviz_poly_data_triangle_indices(poly_data);
    line_indices = fviz_poly_data_line_indices(poly_data);
    if (points == NULL) return FVIZ_OK;

    if (resource == NULL)
    {
        if (device->actor_count == device->actor_capacity)
        {
            FVizSize new_capacity;
            FVizGLActorResource* new_actors;
            if (device->actor_capacity == 0u) new_capacity = 4u;
            else new_capacity = device->actor_capacity * 2u;
            new_actors = (FVizGLActorResource*)fviz_realloc(
                device->actors, new_capacity * sizeof(FVizGLActorResource));
            if (new_actors == NULL)
            {
                return fviz_last_error_code();
            }
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
    resource->actor = actor;

    gl->glGenVertexArrays(1, &resource->vao);
    gl->glBindVertexArray(resource->vao);

    fviz_gl_upload_buffer(device, &resource->position_buffer, FVIZ_GL_ARRAY_BUFFER, points,
        (GLsizeiptr)(point_count * sizeof(FVizVec3)));
    gl->glVertexAttribPointer(FVIZ_GL_POSITION_ATTRIBUTE_INDEX, 3, GL_FLOAT, GL_FALSE,
        (GLsizei)sizeof(FVizVec3), (const void*)0);
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
    gl->glVertexAttribPointer(FVIZ_GL_NORMAL_ATTRIBUTE_INDEX, 3, GL_FLOAT, GL_FALSE,
        (GLsizei)sizeof(FVizVec3), (const void*)0);
    gl->glEnableVertexAttribArray(FVIZ_GL_NORMAL_ATTRIBUTE_INDEX);

    (void)fviz_gl_build_color_buffer(device, resource, actor, poly_data, point_count);

    if (indices != NULL && index_count > 0u)
    {
        fviz_gl_upload_buffer(device, &resource->index_buffer, FVIZ_GL_ELEMENT_ARRAY_BUFFER, indices,
            (GLsizeiptr)(index_count * sizeof(uint32_t)));
    }
    if (line_indices != NULL && line_index_count > 0u)
    {
        fviz_gl_upload_buffer(device, &resource->line_index_buffer, FVIZ_GL_ELEMENT_ARRAY_BUFFER, line_indices,
            (GLsizeiptr)(line_index_count * sizeof(uint32_t)));
    }

    gl->glBindVertexArray(0u);

    resource->index_count = (GLsizei)index_count;
    resource->line_index_count = (GLsizei)line_index_count;
    resource->point_count = point_count;
    resource->generation = generation;
    return FVIZ_OK;
}

static GLuint fviz_gl_compile_shader(
    const FVizGLFunctions* gl,
    GLenum shader_type,
    const char* source,
    char* info_log,
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

    vertex_shader = fviz_gl_compile_shader(gl, FVIZ_GL_VERTEX_SHADER,
        k_fviz_gl_vertex_shader_source, info_log, sizeof(info_log));
    if (vertex_shader == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "FEAViz vertex shader failed to compile");
        return FVIZ_ERROR_GRAPHICS;
    }
    fragment_shader = fviz_gl_compile_shader(gl, FVIZ_GL_FRAGMENT_SHADER,
        k_fviz_gl_fragment_shader_source, info_log, sizeof(info_log));
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
    device->diffuse_location = gl->glGetUniformLocation(device->program, "uDiffuse");
    device->light_position_location = gl->glGetUniformLocation(device->program, "uLightPosition");
    device->light_ambient_location = gl->glGetUniformLocation(device->program, "uLightAmbient");
    device->scalar_color_location = gl->glGetUniformLocation(device->program, "uScalarColorEnabled");
    return FVIZ_OK;
}

static FVizResult fviz_gl_create_2d_program(FVizGLDevice* device)
{
    const FVizGLFunctions* gl = &device->gl;
    GLuint vertex_shader;
    GLuint fragment_shader;
    char info_log[2048];
    GLint status = GL_FALSE;
    vertex_shader = fviz_gl_compile_shader(gl, FVIZ_GL_VERTEX_SHADER,
        k_fviz_gl2d_vertex_shader_source, info_log, sizeof(info_log));
    if (vertex_shader == 0u)
    {
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "FEAViz 2D vertex shader failed to compile");
        return FVIZ_ERROR_GRAPHICS;
    }
    fragment_shader = fviz_gl_compile_shader(gl, FVIZ_GL_FRAGMENT_SHADER,
        k_fviz_gl2d_fragment_shader_source, info_log, sizeof(info_log));
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
    if (fviz_gl_create_program(device) != FVIZ_OK ||
        fviz_gl_create_2d_program(device) != FVIZ_OK)
    {
        fviz_internal_gl_device_destroy(device);
        return NULL;
    }
    return device;
}

void fviz_internal_gl_device_destroy(FVizGLDevice* device)
{
    FVizSize i;
    if (device == NULL) return;
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
    fviz_free(device->actors);
    device->actors = NULL;
    device->actor_count = 0u;
    device->actor_capacity = 0u;
    fviz_free(device);
}

FVizResult fviz_internal_gl_device_render(
    FVizGLDevice* device,
    FVizRenderer* renderer,
    float aspect_ratio)
{
    const FVizGLFunctions* gl;
    FVizCamera* camera;
    FVizScene* scene;
    FVizMat4 projection;
    FVizMat4 view;
    FVizMat4 mvp;
    FVizVec3 light_position;
    FVizVec3 view_direction;
    FVizVec3 light_ambient;
    FVizSize i;

    if (device == NULL || renderer == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "device and renderer must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    gl = &device->gl;
    scene = fviz_renderer_scene(renderer);
    camera = fviz_renderer_camera(renderer);
    if (scene == NULL || camera == NULL) return FVIZ_OK;

    projection = fviz_camera_projection_matrix(camera, aspect_ratio);
    view = fviz_camera_view_matrix(camera);
    mvp = fviz_mat4_multiply(projection, view);

    gl->glUseProgram(device->program);
    gl->glUniformMatrix4fv(device->mvp_location, 1, GL_FALSE, mvp.m);

    light_position = fviz_camera_position(camera);
    view_direction = fviz_vec3_normalize(fviz_vec3_sub(fviz_camera_target(camera), light_position));
    light_position = fviz_vec3_add(light_position, fviz_vec3_scale(view_direction, -0.15f));
    gl->glUniform3fv(device->light_position_location, 1, &light_position.x);

    for (i = 0u; i < fviz_scene_actor_count(scene); ++i)
    {
        const FVizActor* actor = fviz_scene_const_actor(scene, i);
        FVizGLActorResource* resource;
        FVizMat4 model = fviz_actor_transform_matrix(actor);
        FVizMat3 normal_matrix;
        FVizMat4 mvp_actor;
        float red;
        float green;
        float blue;

        if (fviz_actor_is_visible(actor) == FVIZ_FALSE) continue;
        if (fviz_gl_ensure_actor_resource(device, actor) != FVIZ_OK) continue;
        resource = fviz_gl_find_actor_resource(device, actor);
        if (resource == NULL || (resource->index_count == 0 && resource->line_index_count == 0)) continue;

        mvp_actor = fviz_mat4_multiply(mvp, model);
        gl->glUniformMatrix4fv(device->mvp_location, 1, GL_FALSE, mvp_actor.m);
        gl->glUniformMatrix4fv(device->model_location, 1, GL_FALSE, model.m);

        {
            const FVizVec3 scale = fviz_actor_scale(actor);
            FVizMat3 rotation = fviz_mat3_from_quaternion(fviz_actor_orientation(actor));
            FVizMat3 scale_matrix = fviz_mat3_identity();
            FVizMat3 model3;
            scale_matrix.m[0] = scale.x;
            scale_matrix.m[4] = scale.y;
            scale_matrix.m[8] = scale.z;
            model3 = fviz_mat3_multiply(rotation, scale_matrix);
            normal_matrix = fviz_mat3_transpose(fviz_mat3_inverse(model3));
        }
        gl->glUniformMatrix3fv(device->normal_matrix_location, 1, GL_FALSE, normal_matrix.m);

        fviz_actor_get_color(actor, &red, &green, &blue);
        gl->glUniform3fv(device->diffuse_location, 1, (const GLfloat[]) {red, green, blue});
        gl->glUniform1i(device->scalar_color_location, resource->has_color == FVIZ_TRUE ? 1 : 0);
        light_ambient.x = 0.22f;
        light_ambient.y = 0.22f;
        light_ambient.z = 0.26f;
        gl->glUniform3fv(device->light_ambient_location, 1, &light_ambient.x);

        gl->glBindVertexArray(resource->vao);
        if (resource->index_count > 0)
        {
            glPolygonMode(GL_FRONT_AND_BACK, fviz_actor_wireframe(actor) == FVIZ_TRUE ? GL_LINE : GL_FILL);
            glDrawElements(GL_TRIANGLES, resource->index_count, GL_UNSIGNED_INT, (const void*)0);
        }
        if (resource->line_index_count > 0)
        {
            gl->glUniform1i(device->scalar_color_location, 0);
            gl->glUniform3fv(device->diffuse_location, 1, (const GLfloat[]) {0.05f, 0.05f, 0.05f});
            glDrawElements(GL_LINES, resource->line_index_count, GL_UNSIGNED_INT, (const void*)0);
        }
        gl->glBindVertexArray(0u);
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    gl->glUseProgram(0u);
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

static void fviz_gl2d_emit_quad(
    FVizGL2DVertex* vertices,
    FVizSize* count,
    float x0,
    float y0,
    float x1,
    float y1,
    float r,
    float g,
    float b)
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

FVizResult fviz_internal_gl_device_render_legend(
    FVizGLDevice* device,
    const FVizScalarLegend* legend,
    int width,
    int height)
{
    const FVizGLFunctions* gl;
    FVizGL2DVertex* vertices;
    FVizSize vertex_count = 0u;
    FVizSize max_vertices;
    GLuint vbo = 0u;
    FVizMat4 ortho;
    FVizLookupTable* table;
    float bar_x;
    float bar_y;
    float bar_w;
    float bar_h;
    float margin = 16.0f;
    FVizSize i;
    const FVizSize segments = 32u;

    if (device == NULL || legend == NULL || width <= 0 || height <= 0)
    {
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    if (fviz_scalar_legend_is_visible(legend) == FVIZ_FALSE) return FVIZ_OK;
    table = fviz_scalar_legend_lookup_table((FVizScalarLegend*)legend);
    if (table == NULL) return FVIZ_OK;

    bar_w = 22.0f;
    bar_h = (float)height * 0.5f;
    if (bar_h > 420.0f) bar_h = 420.0f;
    switch (fviz_scalar_legend_position(legend))
    {
        case FVIZ_LEGEND_TOP_LEFT: bar_x = margin; bar_y = margin; break;
        case FVIZ_LEGEND_BOTTOM_RIGHT: bar_x = (float)width - margin - bar_w; bar_y = (float)height - margin - bar_h; break;
        case FVIZ_LEGEND_BOTTOM_LEFT: bar_x = margin; bar_y = (float)height - margin - bar_h; break;
        case FVIZ_LEGEND_TOP_RIGHT:
        default: bar_x = (float)width - margin - bar_w; bar_y = margin; break;
    }

    max_vertices = (segments + 2u) * 6u + 6u;
    vertices = (FVizGL2DVertex*)fviz_alloc(max_vertices * sizeof(FVizGL2DVertex));
    if (vertices == NULL) return fviz_last_error_code();

    fviz_gl2d_emit_quad(vertices, &vertex_count,
        bar_x - 4.0f, bar_y - 4.0f,
        bar_x + bar_w + 4.0f, bar_y + bar_h + 4.0f,
        0.10f, 0.11f, 0.14f);

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
            fviz_gl2d_emit_quad(vertices, &vertex_count,
                bar_x, bar_y + f0 * bar_h,
                bar_x + bar_w, bar_y + f1 * bar_h,
                r, g, b);
        }
        fviz_gl2d_emit_quad(vertices, &vertex_count,
            bar_x - 2.0f, bar_y,
            bar_x + bar_w + 2.0f, bar_y + 2.0f,
            0.85f, 0.85f, 0.9f);
        fviz_gl2d_emit_quad(vertices, &vertex_count,
            bar_x - 2.0f, bar_y + bar_h - 2.0f,
            bar_x + bar_w + 2.0f, bar_y + bar_h,
            0.85f, 0.85f, 0.9f);
    }

    (void)memset(&ortho, 0, sizeof(ortho));
    ortho.m[0] = 2.0f / (float)width;
    ortho.m[5] = 2.0f / (float)height;
    ortho.m[10] = -1.0f;
    ortho.m[12] = -1.0f;
    ortho.m[13] = -1.0f;
    ortho.m[15] = 1.0f;

    gl = &device->gl;
    gl->glUseProgram(device->program_2d);
    gl->glUniformMatrix4fv(device->mvp_location_2d, 1, GL_FALSE, ortho.m);
    glDisable(GL_DEPTH_TEST);
    gl->glGenBuffers(1, &vbo);
    gl->glBindBuffer(FVIZ_GL_ARRAY_BUFFER, vbo);
    gl->glBufferData(FVIZ_GL_ARRAY_BUFFER, (GLsizeiptr)(vertex_count * sizeof(FVizGL2DVertex)), vertices, FVIZ_GL_STATIC_DRAW);
    gl->glVertexAttribPointer(0u, 2, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(FVizGL2DVertex), (const void*)0);
    gl->glEnableVertexAttribArray(0u);
    gl->glVertexAttribPointer(1u, 3, GL_FLOAT, GL_FALSE, (GLsizei)sizeof(FVizGL2DVertex), (const void*)(2 * sizeof(float)));
    gl->glEnableVertexAttribArray(1u);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)vertex_count);
    gl->glDisableVertexAttribArray(0u);
    gl->glDisableVertexAttribArray(1u);
    gl->glBindBuffer(FVIZ_GL_ARRAY_BUFFER, 0u);
    gl->glDeleteBuffers(1, &vbo);
    glEnable(GL_DEPTH_TEST);
    gl->glUseProgram(0u);
    fviz_free(vertices);
    return FVIZ_OK;
}
