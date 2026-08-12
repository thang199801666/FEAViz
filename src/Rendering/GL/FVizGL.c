#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <gl/GL.h>

#include <FViz/Rendering/FVizGL.h>

#include <FViz/Core/FVizErrorInternal.h>

static void* fviz_gl_lookup_symbol(const char* name)
{
    void* symbol = (void*)wglGetProcAddress(name);
    HMODULE opengl_module;
    if (symbol != NULL)
    {
        return symbol;
    }
    opengl_module = GetModuleHandleA("opengl32.dll");
    if (opengl_module == NULL)
    {
        return NULL;
    }
    return (void*)GetProcAddress(opengl_module, name);
}

FVizResult fviz_internal_gl_load(FVizGLFunctions* functions)
{
    if (functions == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_INVALID_ARGUMENT, "functions must not be NULL");
        return FVIZ_ERROR_INVALID_ARGUMENT;
    }
    functions->glGenVertexArrays = (FVizGLGenVertexArraysFn)fviz_gl_lookup_symbol("glGenVertexArrays");
    functions->glBindVertexArray = (FVizGLBindVertexArrayFn)fviz_gl_lookup_symbol("glBindVertexArray");
    functions->glDeleteVertexArrays = (FVizGLDeleteVertexArraysFn)fviz_gl_lookup_symbol("glDeleteVertexArrays");
    functions->glGenBuffers = (FVizGLGenBuffersFn)fviz_gl_lookup_symbol("glGenBuffers");
    functions->glBindBuffer = (FVizGLBindBufferFn)fviz_gl_lookup_symbol("glBindBuffer");
    functions->glBufferData = (FVizGLBufferDataFn)fviz_gl_lookup_symbol("glBufferData");
    functions->glBufferSubData = (FVizGLBufferSubDataFn)fviz_gl_lookup_symbol("glBufferSubData");
    functions->glDeleteBuffers = (FVizGLDeleteBuffersFn)fviz_gl_lookup_symbol("glDeleteBuffers");
    functions->glVertexAttribPointer = (FVizGLVertexAttribPointerFn)fviz_gl_lookup_symbol("glVertexAttribPointer");
    functions->glEnableVertexAttribArray = (FVizGLEnableVertexAttribArrayFn)fviz_gl_lookup_symbol("glEnableVertexAttribArray");
    functions->glDisableVertexAttribArray = (FVizGLDisableVertexAttribArrayFn)fviz_gl_lookup_symbol("glDisableVertexAttribArray");
    functions->glCreateShader = (FVizGLCreateShaderFn)fviz_gl_lookup_symbol("glCreateShader");
    functions->glShaderSource = (FVizGLShaderSourceFn)fviz_gl_lookup_symbol("glShaderSource");
    functions->glCompileShader = (FVizGLCompileShaderFn)fviz_gl_lookup_symbol("glCompileShader");
    functions->glGetShaderiv = (FVizGLGetShaderivFn)fviz_gl_lookup_symbol("glGetShaderiv");
    functions->glGetShaderInfoLog = (FVizGLGetShaderInfoLogFn)fviz_gl_lookup_symbol("glGetShaderInfoLog");
    functions->glDeleteShader = (FVizGLDeleteShaderFn)fviz_gl_lookup_symbol("glDeleteShader");
    functions->glCreateProgram = (FVizGLCreateProgramFn)fviz_gl_lookup_symbol("glCreateProgram");
    functions->glAttachShader = (FVizGLAttachShaderFn)fviz_gl_lookup_symbol("glAttachShader");
    functions->glLinkProgram = (FVizGLLinkProgramFn)fviz_gl_lookup_symbol("glLinkProgram");
    functions->glGetProgramiv = (FVizGLGetProgramivFn)fviz_gl_lookup_symbol("glGetProgramiv");
    functions->glGetProgramInfoLog = (FVizGLGetProgramInfoLogFn)fviz_gl_lookup_symbol("glGetProgramInfoLog");
    functions->glDeleteProgram = (FVizGLDeleteProgramFn)fviz_gl_lookup_symbol("glDeleteProgram");
    functions->glUseProgram = (FVizGLUseProgramFn)fviz_gl_lookup_symbol("glUseProgram");
    functions->glGetUniformLocation = (FVizGLGetUniformLocationFn)fviz_gl_lookup_symbol("glGetUniformLocation");
    functions->glUniformMatrix4fv = (FVizGLUniformMatrix4fvFn)fviz_gl_lookup_symbol("glUniformMatrix4fv");
    functions->glUniformMatrix3fv = (FVizGLUniformMatrix3fvFn)fviz_gl_lookup_symbol("glUniformMatrix3fv");
    functions->glUniform3fv = (FVizGLUniform3fvFn)fviz_gl_lookup_symbol("glUniform3fv");
    functions->glUniform1i = (FVizGLUniform1iFn)fviz_gl_lookup_symbol("glUniform1i");

    if (functions->glGenVertexArrays == NULL ||
        functions->glBindVertexArray == NULL ||
        functions->glDeleteVertexArrays == NULL ||
        functions->glGenBuffers == NULL ||
        functions->glBindBuffer == NULL ||
        functions->glBufferData == NULL ||
        functions->glBufferSubData == NULL ||
        functions->glDeleteBuffers == NULL ||
        functions->glVertexAttribPointer == NULL ||
        functions->glEnableVertexAttribArray == NULL ||
        functions->glDisableVertexAttribArray == NULL ||
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
        functions->glUniform1i == NULL)
    {
        fviz_internal_set_error(FVIZ_ERROR_GRAPHICS, "OpenGL 3.3 function loader is incomplete");
        return FVIZ_ERROR_GRAPHICS;
    }
    return FVIZ_OK;
}
