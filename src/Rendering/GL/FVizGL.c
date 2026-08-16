#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gl/GL.h>

#include <stdint.h>
#include <string.h>

#include <FViz/Rendering/FVizGL.h>

#include <FViz/Core/FVizErrorInternal.h>

static PROC fviz_gl_lookup_symbol(const char* name)
{
    PROC symbol = wglGetProcAddress(name);
    HMODULE opengl_module;
    uintptr_t value = 0u;

    _Static_assert(sizeof(symbol) <= sizeof(value), "PROC must fit in uintptr_t");
    memcpy(&value, &symbol, sizeof(symbol));
    if (symbol != NULL && value > (uintptr_t)3u && value != UINTPTR_MAX)
    {
        return symbol;
    }
    opengl_module = GetModuleHandleA("opengl32.dll");
    if (opengl_module == NULL)
    {
        return NULL;
    }
    return GetProcAddress(opengl_module, name);
}

#define FVIZ_GL_LOAD_PROC(field, proc_name) \
    do \
    { \
        PROC fviz_proc_address = fviz_gl_lookup_symbol(proc_name); \
        _Static_assert(sizeof(functions->field) == sizeof(fviz_proc_address), \
                       "OpenGL function pointer size must match PROC"); \
        memcpy(&functions->field, &fviz_proc_address, sizeof(fviz_proc_address)); \
    } while (0)

FVizResult fviz_internal_gl_load(FVizGLFunctions* functions)
{
    if (functions == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "functions must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    FVIZ_GL_LOAD_PROC(glGenVertexArrays, "glGenVertexArrays");
    FVIZ_GL_LOAD_PROC(glBindVertexArray, "glBindVertexArray");
    FVIZ_GL_LOAD_PROC(glDeleteVertexArrays, "glDeleteVertexArrays");
    FVIZ_GL_LOAD_PROC(glGenBuffers, "glGenBuffers");
    FVIZ_GL_LOAD_PROC(glBindBuffer, "glBindBuffer");
    FVIZ_GL_LOAD_PROC(glBufferData, "glBufferData");
    FVIZ_GL_LOAD_PROC(glBufferSubData, "glBufferSubData");
    FVIZ_GL_LOAD_PROC(glDeleteBuffers, "glDeleteBuffers");
    FVIZ_GL_LOAD_PROC(glGenRenderbuffers, "glGenRenderbuffers");
    FVIZ_GL_LOAD_PROC(glBindRenderbuffer, "glBindRenderbuffer");
    FVIZ_GL_LOAD_PROC(glRenderbufferStorage, "glRenderbufferStorage");
    FVIZ_GL_LOAD_PROC(glRenderbufferStorageMultisample, "glRenderbufferStorageMultisample");
    FVIZ_GL_LOAD_PROC(glFramebufferRenderbuffer, "glFramebufferRenderbuffer");
    FVIZ_GL_LOAD_PROC(glDeleteRenderbuffers, "glDeleteRenderbuffers");
    FVIZ_GL_LOAD_PROC(glActiveTexture, "glActiveTexture");
    FVIZ_GL_LOAD_PROC(glGenFramebuffers, "glGenFramebuffers");
    FVIZ_GL_LOAD_PROC(glBindFramebuffer, "glBindFramebuffer");
    FVIZ_GL_LOAD_PROC(glFramebufferTexture2D, "glFramebufferTexture2D");
    FVIZ_GL_LOAD_PROC(glCheckFramebufferStatus, "glCheckFramebufferStatus");
    FVIZ_GL_LOAD_PROC(glDeleteFramebuffers, "glDeleteFramebuffers");
    FVIZ_GL_LOAD_PROC(glBlitFramebuffer, "glBlitFramebuffer");
    FVIZ_GL_LOAD_PROC(glVertexAttribPointer, "glVertexAttribPointer");
    FVIZ_GL_LOAD_PROC(glEnableVertexAttribArray, "glEnableVertexAttribArray");
    FVIZ_GL_LOAD_PROC(glDisableVertexAttribArray, "glDisableVertexAttribArray");
    FVIZ_GL_LOAD_PROC(glVertexAttribDivisor, "glVertexAttribDivisor");
    FVIZ_GL_LOAD_PROC(glDrawElementsInstanced, "glDrawElementsInstanced");
    FVIZ_GL_LOAD_PROC(glCreateShader, "glCreateShader");
    FVIZ_GL_LOAD_PROC(glShaderSource, "glShaderSource");
    FVIZ_GL_LOAD_PROC(glCompileShader, "glCompileShader");
    FVIZ_GL_LOAD_PROC(glGetShaderiv, "glGetShaderiv");
    FVIZ_GL_LOAD_PROC(glGetShaderInfoLog, "glGetShaderInfoLog");
    FVIZ_GL_LOAD_PROC(glDeleteShader, "glDeleteShader");
    FVIZ_GL_LOAD_PROC(glCreateProgram, "glCreateProgram");
    FVIZ_GL_LOAD_PROC(glAttachShader, "glAttachShader");
    FVIZ_GL_LOAD_PROC(glLinkProgram, "glLinkProgram");
    FVIZ_GL_LOAD_PROC(glGetProgramiv, "glGetProgramiv");
    FVIZ_GL_LOAD_PROC(glGetProgramInfoLog, "glGetProgramInfoLog");
    FVIZ_GL_LOAD_PROC(glDeleteProgram, "glDeleteProgram");
    FVIZ_GL_LOAD_PROC(glUseProgram, "glUseProgram");
    FVIZ_GL_LOAD_PROC(glGetUniformLocation, "glGetUniformLocation");
    FVIZ_GL_LOAD_PROC(glUniformMatrix4fv, "glUniformMatrix4fv");
    FVIZ_GL_LOAD_PROC(glUniformMatrix3fv, "glUniformMatrix3fv");
    FVIZ_GL_LOAD_PROC(glUniform3fv, "glUniform3fv");
    FVIZ_GL_LOAD_PROC(glUniform4fv, "glUniform4fv");
    FVIZ_GL_LOAD_PROC(glUniform1f, "glUniform1f");
    FVIZ_GL_LOAD_PROC(glUniform1i, "glUniform1i");
    FVIZ_GL_LOAD_PROC(glUniform1ui, "glUniform1ui");
    FVIZ_GL_LOAD_PROC(glClearBufferuiv, "glClearBufferuiv");
    /* Timer-query entry points are optional. OpenGL 3.3-class drivers normally
       expose them, but rendering must remain usable without GPU timing. */
    FVIZ_GL_LOAD_PROC(glGenQueries, "glGenQueries");
    FVIZ_GL_LOAD_PROC(glDeleteQueries, "glDeleteQueries");
    FVIZ_GL_LOAD_PROC(glBeginQuery, "glBeginQuery");
    FVIZ_GL_LOAD_PROC(glEndQuery, "glEndQuery");
    FVIZ_GL_LOAD_PROC(glGetQueryObjectiv, "glGetQueryObjectiv");
    FVIZ_GL_LOAD_PROC(glGetQueryObjectui64v, "glGetQueryObjectui64v");

    if (functions->glGenVertexArrays == NULL ||
        functions->glBindVertexArray == NULL ||
        functions->glDeleteVertexArrays == NULL ||
        functions->glGenBuffers == NULL ||
        functions->glBindBuffer == NULL ||
        functions->glBufferData == NULL ||
        functions->glBufferSubData == NULL ||
        functions->glDeleteBuffers == NULL ||
        functions->glGenRenderbuffers == NULL ||
        functions->glBindRenderbuffer == NULL ||
        functions->glRenderbufferStorage == NULL ||
        functions->glRenderbufferStorageMultisample == NULL ||
        functions->glFramebufferRenderbuffer == NULL ||
        functions->glDeleteRenderbuffers == NULL ||
        functions->glActiveTexture == NULL ||
        functions->glGenFramebuffers == NULL ||
        functions->glBindFramebuffer == NULL ||
        functions->glFramebufferTexture2D == NULL ||
        functions->glCheckFramebufferStatus == NULL ||
        functions->glDeleteFramebuffers == NULL ||
        functions->glBlitFramebuffer == NULL ||
        functions->glVertexAttribPointer == NULL ||
        functions->glEnableVertexAttribArray == NULL ||
        functions->glDisableVertexAttribArray == NULL ||
        functions->glVertexAttribDivisor == NULL ||
        functions->glDrawElementsInstanced == NULL ||
        functions->glCreateShader == NULL ||
        functions->glShaderSource == NULL ||
        functions->glCompileShader == NULL ||
        functions->glGetShaderiv == NULL ||
        functions->glGetShaderInfoLog == NULL ||
        functions->glDeleteShader == NULL ||
        functions->glCreateProgram == NULL ||
        functions->glAttachShader == NULL ||
        functions->glLinkProgram == NULL ||
        functions->glGetProgramiv == NULL ||
        functions->glGetProgramInfoLog == NULL ||
        functions->glDeleteProgram == NULL ||
        functions->glUseProgram == NULL ||
        functions->glGetUniformLocation == NULL ||
        functions->glUniformMatrix4fv == NULL ||
        functions->glUniformMatrix3fv == NULL ||
        functions->glUniform3fv == NULL ||
        functions->glUniform4fv == NULL ||
        functions->glUniform1f == NULL ||
        functions->glUniform1i == NULL ||
        functions->glUniform1ui == NULL ||
        functions->glClearBufferuiv == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "OpenGL 3.3 function loader is incomplete");
        return FVIZ_ERROR_GRAPHICS;
    }
    return FVIZ_OK;
}
