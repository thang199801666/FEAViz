#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include <FViz/Core/FVizError.h>
#include <FViz/Core/FVizMemory.h>
#include <FViz/Math/FVizMat3.h>
#include <FViz/Math/FVizMat4.h>
#include <FViz/Rendering/FVizActor.h>
#include <FViz/Rendering/FVizCamera.h>
#include <FViz/Rendering/FVizRenderer.h>
#include <FViz/Rendering/FVizScene.h>

#include <FViz/Core/FVizErrorInternal.h>
#include <FViz/Mesh/FVizPolyDataPrivate.h>
#include <FViz/Rendering/FVizGL.h>
#include <FViz/Rendering/FVizGLDevice.h>

#define FVIZ_GL_POSITION_ATTRIBUTE_INDEX 0u
#define FVIZ_GL_NORMAL_ATTRIBUTE_INDEX 1u

static const char* const k_fviz_gl_vertex_shader_source =
    "#version 330 core\n"
    "layout(location = 0) in vec3 aPosition;\n"
    "layout(location = 1) in vec3 aNormal;\n"
    "uniform mat4 uMvp;\n"
    "uniform mat4 uModel;\n"
    "uniform mat3 uNormalMatrix;\n"
    "out vec3 vNormal;\n"
    "out vec3 vWorldPos;\n"
    "void main()\n"
    "{\n"
    "    gl_Position = uMvp * vec4(aPosition, 1.0);\n"
    "    vNormal = normalize(uNormalMatrix * aNormal);\n"
    "    vWorldPos = vec3(uModel * vec4(aPosition, 1.0));\n"
    "}\n";

static const char* const k_fviz_gl_fragment_shader_source =
    "#version 330 core\n"
    "in vec3 vNormal;\n"
    "in vec3 vWorldPos;\n"
    "uniform vec3 uDiffuse;\n"
    "uniform vec3 uLightPosition;\n"
    "uniform vec3 uLightAmbient;\n"
    "out vec4 outColor;\n"
    "void main()\n"
    "{\n"
    "    vec3 n = normalize(vNormal);\n"
    "    if (length(n) < 0.5)\n"
    "    {\n"
    "        n = normalize(cross(dFdx(vWorldPos), dFdy(vWorldPos)));\n"
    "    }\n"
    "    vec3 l = normalize(uLightPosition - vWorldPos);\n"
    "    float diffuse = max(dot(n, l), 0.0);\n"
    "    vec3 color = uDiffuse * (uLightAmbient + 0.9 * diffuse);\n"
    "    outColor = vec4(color, 1.0);\n"
    "}\n";

typedef struct FVizGLActorResource
{
    const FVizActor* actor;
    GLuint vao;
    GLuint position_buffer;
    GLuint normal_buffer;
    GLuint index_buffer;
    GLsizei index_count;
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
    resource->index_count = 0;
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

static FVizResult fviz_gl_ensure_actor_resource(FVizGLDevice* device, const FVizActor* actor)
{
    const FVizPolyData* poly_data;
    const FVizVec3* points;
    const FVizVec3* normals;
    const uint32_t* indices;
    const FVizGLFunctions* gl = &device->gl;
    FVizGLActorResource* resource;
    FVizSize point_count;
    FVizSize index_count;
    uint32_t generation;

    poly_data = fviz_actor_const_poly_data(actor);
    if (poly_data == NULL) return FVIZ_OK;
    point_count = fviz_poly_data_point_count(poly_data);
    index_count = fviz_poly_data_triangle_count(poly_data) * 3u;
    generation = fviz_internal_poly_data_generation(poly_data);
    if (point_count == 0u || index_count == 0u) return FVIZ_OK;
    if (index_count > (FVizSize)INT_MAX)
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
    if (points == NULL || indices == NULL) return FVIZ_OK;

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

    fviz_gl_upload_buffer(device, &resource->index_buffer, FVIZ_GL_ELEMENT_ARRAY_BUFFER, indices,
        (GLsizeiptr)(index_count * sizeof(uint32_t)));

    gl->glBindVertexArray(0u);

    resource->index_count = (GLsizei)index_count;
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
    if (fviz_gl_create_program(device) != FVIZ_OK)
    {
        fviz_free(device);
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
        if (resource == NULL || resource->index_count == 0) continue;

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
        light_ambient.x = 0.22f;
        light_ambient.y = 0.22f;
        light_ambient.z = 0.26f;
        gl->glUniform3fv(device->light_ambient_location, 1, &light_ambient.x);

        glPolygonMode(GL_FRONT_AND_BACK, fviz_actor_wireframe(actor) == FVIZ_TRUE ? GL_LINE : GL_FILL);
        gl->glBindVertexArray(resource->vao);
        glDrawElements(GL_TRIANGLES, resource->index_count, GL_UNSIGNED_INT, (const void*)0);
        gl->glBindVertexArray(0u);
    }
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    gl->glUseProgram(0u);
    return FVIZ_OK;
}
