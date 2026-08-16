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
#define FVIZ_GL_GEOMETRY_SHADER 0x8DD9
#define FVIZ_GL_COMPILE_STATUS 0x8B81
#define FVIZ_GL_LINK_STATUS 0x8B82
#define FVIZ_GL_INFO_LOG_LENGTH 0x8B84
#define FVIZ_GL_ARRAY_BUFFER 0x8892
#define FVIZ_GL_ELEMENT_ARRAY_BUFFER 0x8893
#define FVIZ_GL_STATIC_DRAW 0x88E4
#define FVIZ_GL_DYNAMIC_DRAW 0x88E8
#define FVIZ_GL_FRAMEBUFFER 0x8D40
#define FVIZ_GL_READ_FRAMEBUFFER 0x8CA8
#define FVIZ_GL_DRAW_FRAMEBUFFER 0x8CA9
#define FVIZ_GL_COLOR_ATTACHMENT0 0x8CE0
#define FVIZ_GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define FVIZ_GL_RGBA8 0x8058
#define FVIZ_GL_SRGB8_ALPHA8 0x8C43
#define FVIZ_GL_RGBA16F 0x881A
#define FVIZ_GL_RGBA32UI 0x8D70
#define FVIZ_GL_R8 0x8229
#define FVIZ_GL_R32F 0x822E
#define FVIZ_GL_RED 0x1903
#define FVIZ_GL_TEXTURE_3D 0x806F
#define FVIZ_GL_TEXTURE_WRAP_R 0x8072
#define FVIZ_GL_RGBA_INTEGER 0x8D99
#define FVIZ_GL_COLOR 0x1800
#define FVIZ_GL_DEPTH24_STENCIL8 0x88F0
#define FVIZ_GL_DEPTH_STENCIL_ATTACHMENT 0x821A
#define FVIZ_GL_RENDERBUFFER 0x8D41
#define FVIZ_GL_TEXTURE0 0x84C0
#define FVIZ_GL_TEXTURE1 0x84C1
#define FVIZ_GL_TEXTURE2 0x84C2
#define FVIZ_GL_TEXTURE3 0x84C3
#define FVIZ_GL_MAJOR_VERSION 0x821B
#define FVIZ_GL_MINOR_VERSION 0x821C
#define FVIZ_GL_DEPTH_COMPONENT 0x1902
#define FVIZ_GL_DEPTH_COMPONENT24 0x81A6
#define FVIZ_GL_DEPTH_ATTACHMENT 0x8D00
#define FVIZ_GL_NONE 0
#define FVIZ_GL_TEXTURE_COMPARE_MODE 0x884C
#define FVIZ_GL_TEXTURE_WRAP_S 0x2802
#define FVIZ_GL_TEXTURE_WRAP_T 0x2803
#define FVIZ_GL_CLAMP_TO_EDGE 0x812F
typedef void(APIENTRY* FVizGLGenVertexArraysFn)(GLsizei n, GLuint* arrays);
typedef void(APIENTRY* FVizGLBindVertexArrayFn)(GLuint array);
typedef void(APIENTRY* FVizGLDeleteVertexArraysFn)(GLsizei n, const GLuint* arrays);
typedef void(APIENTRY* FVizGLGenBuffersFn)(GLsizei n, GLuint* buffers);
typedef void(APIENTRY* FVizGLBindBufferFn)(GLenum target, GLuint buffer);
typedef void(APIENTRY* FVizGLBufferDataFn)(GLenum target, GLsizeiptr size, const void* data, GLenum usage);
typedef void(APIENTRY* FVizGLBufferSubDataFn)(GLenum target, GLintptr offset, GLsizeiptr size, const void* data);
typedef void(APIENTRY* FVizGLDeleteBuffersFn)(GLsizei n, const GLuint* buffers);
typedef void(APIENTRY* FVizGLGenRenderbuffersFn)(GLsizei n, GLuint* renderbuffers);
typedef void(APIENTRY* FVizGLBindRenderbufferFn)(GLenum target, GLuint renderbuffer);
typedef void(APIENTRY* FVizGLRenderbufferStorageFn)(GLenum target, GLenum internalformat, GLsizei width,
                                                    GLsizei height);
typedef void(APIENTRY* FVizGLRenderbufferStorageMultisampleFn)(GLenum target, GLsizei samples, GLenum internalformat,
                                                               GLsizei width, GLsizei height);
typedef void(APIENTRY* FVizGLFramebufferRenderbufferFn)(GLenum target, GLenum attachment, GLenum renderbuffertarget,
                                                        GLuint renderbuffer);
typedef void(APIENTRY* FVizGLDeleteRenderbuffersFn)(GLsizei n, const GLuint* renderbuffers);
typedef void(APIENTRY* FVizGLActiveTextureFn)(GLenum texture);
typedef void(APIENTRY* FVizGLGenFramebuffersFn)(GLsizei n, GLuint* framebuffers);
typedef void(APIENTRY* FVizGLBindFramebufferFn)(GLenum target, GLuint framebuffer);
typedef void(APIENTRY* FVizGLFramebufferTexture2DFn)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture,
                                                     GLint level);
typedef GLenum(APIENTRY* FVizGLCheckFramebufferStatusFn)(GLenum target);
typedef void(APIENTRY* FVizGLDeleteFramebuffersFn)(GLsizei n, const GLuint* framebuffers);
typedef void(APIENTRY* FVizGLBlitFramebufferFn)(GLint src_x0, GLint src_y0, GLint src_x1, GLint src_y1, GLint dst_x0,
                                                GLint dst_y0, GLint dst_x1, GLint dst_y1, GLbitfield mask,
                                                GLenum filter);
typedef void(APIENTRY* FVizGLVertexAttribPointerFn)(GLuint index, GLint size, GLenum type, GLboolean normalized,
                                                    GLsizei stride, const void* pointer);
typedef void(APIENTRY* FVizGLEnableVertexAttribArrayFn)(GLuint index);
typedef void(APIENTRY* FVizGLDisableVertexAttribArrayFn)(GLuint index);
typedef void(APIENTRY* FVizGLVertexAttribDivisorFn)(GLuint index, GLuint divisor);
typedef void(APIENTRY* FVizGLDrawElementsInstancedFn)(GLenum mode, GLsizei count, GLenum type, const void* indices,
                                                      GLsizei instancecount);
typedef GLuint(APIENTRY* FVizGLCreateShaderFn)(GLenum type);
typedef void(APIENTRY* FVizGLShaderSourceFn)(GLuint shader, GLsizei count, const char** string, const GLint* length);
typedef void(APIENTRY* FVizGLCompileShaderFn)(GLuint shader);
typedef void(APIENTRY* FVizGLGetShaderivFn)(GLuint shader, GLenum pname, GLint* params);
typedef void(APIENTRY* FVizGLGetShaderInfoLogFn)(GLuint shader, GLsizei buf_size, GLsizei* length, char* info_log);
typedef void(APIENTRY* FVizGLDeleteShaderFn)(GLuint shader);
typedef GLuint(APIENTRY* FVizGLCreateProgramFn)(void);
typedef void(APIENTRY* FVizGLAttachShaderFn)(GLuint program, GLuint shader);
typedef void(APIENTRY* FVizGLLinkProgramFn)(GLuint program);
typedef void(APIENTRY* FVizGLGetProgramivFn)(GLuint program, GLenum pname, GLint* params);
typedef void(APIENTRY* FVizGLGetProgramInfoLogFn)(GLuint program, GLsizei buf_size, GLsizei* length, char* info_log);
typedef void(APIENTRY* FVizGLDeleteProgramFn)(GLuint program);
typedef void(APIENTRY* FVizGLUseProgramFn)(GLuint program);
typedef GLint(APIENTRY* FVizGLGetUniformLocationFn)(GLuint program, const char* name);
typedef void(APIENTRY* FVizGLUniformMatrix4fvFn)(GLint location, GLsizei count, GLboolean transpose,
                                                 const GLfloat* value);
typedef void(APIENTRY* FVizGLUniformMatrix3fvFn)(GLint location, GLsizei count, GLboolean transpose,
                                                 const GLfloat* value);
typedef void(APIENTRY* FVizGLUniform3fvFn)(GLint location, GLsizei count, const GLfloat* value);
typedef void(APIENTRY* FVizGLUniform4fvFn)(GLint location, GLsizei count, const GLfloat* value);
typedef void(APIENTRY* FVizGLUniform1fFn)(GLint location, GLfloat value);
typedef void(APIENTRY* FVizGLUniform1iFn)(GLint location, GLint v0);
typedef void(APIENTRY* FVizGLUniform1uiFn)(GLint location, GLuint v0);
typedef void(APIENTRY* FVizGLClearBufferuivFn)(GLenum buffer, GLint drawbuffer, const GLuint* value);
typedef void(APIENTRY* FVizGLGenQueriesFn)(GLsizei n, GLuint* ids);
typedef void(APIENTRY* FVizGLDeleteQueriesFn)(GLsizei n, const GLuint* ids);
typedef void(APIENTRY* FVizGLBeginQueryFn)(GLenum target, GLuint id);
typedef void(APIENTRY* FVizGLEndQueryFn)(GLenum target);
typedef void(APIENTRY* FVizGLGetQueryObjectivFn)(GLuint id, GLenum pname, GLint* params);
typedef void(APIENTRY* FVizGLGetQueryObjectui64vFn)(GLuint id, GLenum pname, uint64_t* params);
typedef void(APIENTRY* FVizGLTexImage3DFn)(GLenum target, GLint level, GLint internalformat, GLsizei width,
                                           GLsizei height, GLsizei depth, GLint border, GLenum format, GLenum type,
                                           const void* pixels);
typedef void(APIENTRY* FVizGLTexSubImage3DFn)(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset,
                                              GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type,
                                              const void* pixels);

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
    FVizGLGenRenderbuffersFn glGenRenderbuffers;
    FVizGLBindRenderbufferFn glBindRenderbuffer;
    FVizGLRenderbufferStorageFn glRenderbufferStorage;
    FVizGLRenderbufferStorageMultisampleFn glRenderbufferStorageMultisample;
    FVizGLFramebufferRenderbufferFn glFramebufferRenderbuffer;
    FVizGLDeleteRenderbuffersFn glDeleteRenderbuffers;
    FVizGLActiveTextureFn glActiveTexture;
    FVizGLGenFramebuffersFn glGenFramebuffers;
    FVizGLBindFramebufferFn glBindFramebuffer;
    FVizGLFramebufferTexture2DFn glFramebufferTexture2D;
    FVizGLCheckFramebufferStatusFn glCheckFramebufferStatus;
    FVizGLDeleteFramebuffersFn glDeleteFramebuffers;
    FVizGLBlitFramebufferFn glBlitFramebuffer;
    FVizGLVertexAttribPointerFn glVertexAttribPointer;
    FVizGLEnableVertexAttribArrayFn glEnableVertexAttribArray;
    FVizGLDisableVertexAttribArrayFn glDisableVertexAttribArray;
    FVizGLVertexAttribDivisorFn glVertexAttribDivisor;
    FVizGLDrawElementsInstancedFn glDrawElementsInstanced;
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
    FVizGLUniformMatrix3fvFn glUniformMatrix3fv;
    FVizGLUniform3fvFn glUniform3fv;
    FVizGLUniform4fvFn glUniform4fv;
    FVizGLUniform1fFn glUniform1f;
    FVizGLUniform1iFn glUniform1i;
    FVizGLUniform1uiFn glUniform1ui;
    FVizGLClearBufferuivFn glClearBufferuiv;
    FVizGLGenQueriesFn glGenQueries;
    FVizGLDeleteQueriesFn glDeleteQueries;
    FVizGLBeginQueryFn glBeginQuery;
    FVizGLEndQueryFn glEndQuery;
    FVizGLGetQueryObjectivFn glGetQueryObjectiv;
    FVizGLGetQueryObjectui64vFn glGetQueryObjectui64v;
    FVizGLTexImage3DFn glTexImage3D;
    FVizGLTexSubImage3DFn glTexSubImage3D;
} FVizGLFunctions;

FVizResult fviz_internal_gl_load(FVizGLFunctions* functions);

#endif /* FVIZ_INTERNAL_RENDERING_GL_H */
