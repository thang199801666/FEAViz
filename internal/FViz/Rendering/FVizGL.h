#ifndef FVIZ_INTERNAL_RENDERING_GL_H
#define FVIZ_INTERNAL_RENDERING_GL_H

#include <stddef.h>
#include <stdint.h>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <gl/GL.h>

#include <FViz/Core/FVizResult.h>
#include <FViz/Core/FVizTypes.h>

#if !defined(GLsizeiptr)
typedef ptrdiff_t GLsizeiptr;
#endif
#if !defined(GLintptr)
typedef ptrdiff_t GLintptr;
#endif

/* OpenGL 3.3 constants that are missing from the legacy <gl/GL.h> header. */
#define FVIZ_GL_VERTEX_SHADER 0x8B31
#define FVIZ_GL_FRAGMENT_SHADER 0x8B30
#define FVIZ_GL_COMPILE_STATUS 0x8B81
#define FVIZ_GL_LINK_STATUS 0x8B82
#define FVIZ_GL_INFO_LOG_LENGTH 0x8B84
#define FVIZ_GL_ARRAY_BUFFER 0x8892
#define FVIZ_GL_ELEMENT_ARRAY_BUFFER 0x8893
#define FVIZ_GL_STATIC_DRAW 0x88E4
#define FVIZ_GL_DYNAMIC_DRAW 0x88E8
#define FVIZ_GL_MAJOR_VERSION 0x821B
#define FVIZ_GL_MINOR_VERSION 0x821C

typedef void (APIENTRY* FVizGLGenVertexArraysFn)(GLsizei n, GLuint* arrays);
typedef void (APIENTRY* FVizGLBindVertexArrayFn)(GLuint array);
typedef void (APIENTRY* FVizGLDeleteVertexArraysFn)(GLsizei n, const GLuint* arrays);
typedef void (APIENTRY* FVizGLGenBuffersFn)(GLsizei n, GLuint* buffers);
typedef void (APIENTRY* FVizGLBindBufferFn)(GLenum target, GLuint buffer);
typedef void (APIENTRY* FVizGLBufferDataFn)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
typedef void (APIENTRY* FVizGLBufferSubDataFn)(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
typedef void (APIENTRY* FVizGLDeleteBuffersFn)(GLsizei n, const GLuint* buffers);
typedef void (APIENTRY* FVizGLVertexAttribPointerFn)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer);
typedef void (APIENTRY* FVizGLEnableVertexAttribArrayFn)(GLuint index);
typedef void (APIENTRY* FVizGLDisableVertexAttribArrayFn)(GLuint index);
typedef GLuint (APIENTRY* FVizGLCreateShaderFn)(GLenum type);
typedef void (APIENTRY* FVizGLShaderSourceFn)(GLuint shader, GLsizei count, const char** string, const GLint* length);
typedef void (APIENTRY* FVizGLCompileShaderFn)(GLuint shader);
typedef void (APIENTRY* FVizGLGetShaderivFn)(GLuint shader, GLenum pname, GLint* params);
typedef void (APIENTRY* FVizGLGetShaderInfoLogFn)(GLuint shader, GLsizei buf_size, GLsizei* length, char* info_log);
typedef void (APIENTRY* FVizGLDeleteShaderFn)(GLuint shader);
typedef GLuint (APIENTRY* FVizGLCreateProgramFn)(void);
typedef void (APIENTRY* FVizGLAttachShaderFn)(GLuint program, GLuint shader);
typedef void (APIENTRY* FVizGLLinkProgramFn)(GLuint program);
typedef void (APIENTRY* FVizGLGetProgramivFn)(GLuint program, GLenum pname, GLint* params);
typedef void (APIENTRY* FVizGLGetProgramInfoLogFn)(GLuint program, GLsizei buf_size, GLsizei* length, char* info_log);
typedef void (APIENTRY* FVizGLDeleteProgramFn)(GLuint program);
typedef void (APIENTRY* FVizGLUseProgramFn)(GLuint program);
typedef GLint (APIENTRY* FVizGLGetUniformLocationFn)(GLuint program, const char* name);
typedef void (APIENTRY* FVizGLUniformMatrix4fvFn)(GLint location, GLsizei count, GLboolean transpose, const GLfloat* value);
typedef void (APIENTRY* FVizGLUniform3fvFn)(GLint location, GLsizei count, const GLfloat* value);
typedef void (APIENTRY* FVizGLUniform1iFn)(GLint location, GLint v0);

typedef struct FVizGLFunctions
{
    FVizGLGenVertexArraysFn glGenVertexArrays;
    FVizGLBindVertexArrayFn glBindVertexArray;
    FVizGLDeleteVertexArraysFn glDeleteVertexArrays;
    FVizGLGenBuffersFn glGenBuffers;
    FVizGLBindBufferFn glBindBuffer;
    FVizGLBufferDataFn glBufferData;
    FVizGLBufferSubDataFn glBufferSubData;
    FVizGLDeleteBuffersFn glDeleteBuffers;
    FVizGLVertexAttribPointerFn glVertexAttribPointer;
    FVizGLEnableVertexAttribArrayFn glEnableVertexAttribArray;
    FVizGLDisableVertexAttribArrayFn glDisableVertexAttribArray;
    FVizGLCreateShaderFn glCreateShader;
    FVizGLShaderSourceFn glShaderSource;
    FVizGLCompileShaderFn glCompileShader;
    FVizGLGetShaderivFn glGetShaderiv;
    FVizGLGetShaderInfoLogFn glGetShaderInfoLog;
    FVizGLDeleteShaderFn glDeleteShader;
    FVizGLCreateProgramFn glCreateProgram;
    FVizGLAttachShaderFn glAttachShader;
    FVizGLLinkProgramFn glLinkProgram;
    FVizGLGetProgramivFn glGetProgramiv;
    FVizGLGetProgramInfoLogFn glGetProgramInfoLog;
    FVizGLDeleteProgramFn glDeleteProgram;
    FVizGLUseProgramFn glUseProgram;
    FVizGLGetUniformLocationFn glGetUniformLocation;
    FVizGLUniformMatrix4fvFn glUniformMatrix4fv;
    FVizGLUniform3fvFn glUniform3fv;
    FVizGLUniform1iFn glUniform1i;
} FVizGLFunctions;

FVizResult fviz_internal_gl_load(FVizGLFunctions* functions);

#endif /* FVIZ_INTERNAL_RENDERING_GL_H */
