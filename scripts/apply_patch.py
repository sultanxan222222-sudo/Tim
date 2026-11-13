#!/usr/bin/env python3
"""Apply the pinned Mali/GLES2 compatibility changes to Flycast."""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

PINNED_COMMIT = "72b8e67141d85ca5c9259f04563b49f7f92f928b"


def replace_once(source: str, old: str, new: str, label: str) -> str:
    count = source.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one source match, found {count}")
    return source.replace(old, new, 1)


def patch_cpp(source: str) -> str:
    source = replace_once(
        source,
        "#include <cmath>\n#include <memory>\n",
        "#include <cmath>\n#include <memory>\n#include <vector>\n",
        "vector include",
    )

    old_pixel_compat = '''const char *PixelCompatShader = R"(
#if TARGET_GL == GLES2 || TARGET_GL == GL2
#define in varying
#endif
)";'''
    new_pixel_compat = '''const char *PixelCompatShader = R"(
#if TARGET_GL == GLES2 || TARGET_GL == GL2
#define in varying
#endif

// Fragment highp is optional in OpenGL ES 2.0. Keep vertex precision unchanged.
#if TARGET_GL == GLES2 && HIGH_PRECISION_FLOAT == 0
#define highp mediump
#endif
)";'''
    source = replace_once(source, old_pixel_compat, new_pixel_compat, "fragment precision fallback")

    old_extensions = '''\t\tconst char *extensions = (const char *)glGetString(GL_EXTENSIONS);
\t\tif (strstr(extensions, "GL_OES_packed_depth_stencil") != NULL)'''
    new_extensions = '''\t\tconst char *extensions = (const char *)glGetString(GL_EXTENSIONS);
\t\tif (extensions == nullptr) extensions = "";
\t\tif (strstr(extensions, "GL_OES_packed_depth_stencil") != NULL)'''
    source = replace_once(source, old_extensions, new_extensions, "extension string null guard")

    old_precision = '''\t\tGLint ranges[2];
\t\tGLint precision;
\t\tglGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, ranges, &precision);
\t\tgl.highp_float_supported = (ranges[0] != 0 || ranges[1] != 0) && precision != 0;
\t\tif (!gl.border_clamp_supported)'''
    new_precision = '''\t\tGLint ranges[2] = { 0, 0 };
\t\tGLint precision = 0;
\t\tglGetShaderPrecisionFormat(GL_FRAGMENT_SHADER, GL_HIGH_FLOAT, ranges, &precision);
\t\tgl.highp_float_supported = (ranges[0] != 0 || ranges[1] != 0) && precision != 0;
\t\tif (gl.gl_major < 3 && !gl.highp_float_supported)
\t\t\tWARN_LOG(RENDERER, "GLES2 fragment highp is unavailable; using mediump in fragment shaders");
\t\tif (!gl.border_clamp_supported)'''
    source = replace_once(source, old_precision, new_precision, "fragment precision capability probe")

    old_driver_strings = '''\tconst char *vendor = (const char *)glGetString(GL_VENDOR);
\tconst char *renderer = (const char *)glGetString(GL_RENDERER);
\tgl.mesa_nouveau = !stricmp(vendor, "nouveau")
\t\t\t|| (!stricmp(vendor, "Mesa") && !strncmp(renderer, "NV", 2));
\tgl.mali = !stricmp(vendor, "arm");
\tNOTICE_LOG(RENDERER, "OpenGL%s version %d.%d", gl.is_gles ? " ES" : "", gl.gl_major, gl.gl_minor);
\tNOTICE_LOG(RENDERER, "Vendor '%s' Renderer '%s' Version '%s'", vendor, renderer, glGetString(GL_VERSION));'''
    new_driver_strings = '''\tconst char *vendor = (const char *)glGetString(GL_VENDOR);
\tconst char *renderer = (const char *)glGetString(GL_RENDERER);
\tif (vendor == nullptr) vendor = "unknown";
\tif (renderer == nullptr) renderer = "unknown";
\tconst char *version = (const char *)glGetString(GL_VERSION);
\tif (version == nullptr) version = "unknown";
\tgl.mesa_nouveau = !stricmp(vendor, "nouveau")
\t\t\t|| (!stricmp(vendor, "Mesa") && !strncmp(renderer, "NV", 2));
\tgl.mali = !stricmp(vendor, "arm") || strstr(renderer, "Mali") != nullptr;
\tNOTICE_LOG(RENDERER, "OpenGL%s version %d.%d", gl.is_gles ? " ES" : "", gl.gl_major, gl.gl_minor);
\tNOTICE_LOG(RENDERER, "Vendor '%s' Renderer '%s' Version '%s'", vendor, renderer, version);'''
    source = replace_once(source, old_driver_strings, new_driver_strings, "driver string safety and Mali detection")

    shader_start = source.index("GLuint gl_CompileShader(const char* shader,GLuint type)\n{")
    shader_end = source.index("\nGLuint gl_CompileAndLink(", shader_start)
    old_shader_function = source[shader_start:shader_end]
    if "glGetShaderInfoLog" not in old_shader_function or "glCompileShader(rv);" not in old_shader_function:
        raise RuntimeError("shader compiler function does not match pinned source")
    new_shader_function = r'''GLuint gl_CompileShader(const char* shader,GLuint type)
{
\tGLint result = GL_FALSE;
\tGLint compile_log_len = 0;
\tGLuint rv = glCreateShader(type);
\tif (rv == 0)
\t{
\t\tWARN_LOG(RENDERER, "Shader: glCreateShader failed for %s shader", type == GL_VERTEX_SHADER ? "vertex" : "fragment");
\t\treturn 0;
\t}
\tglShaderSource(rv, 1, &shader, NULL);
\tglCompileShader(rv);

\tglGetShaderiv(rv, GL_COMPILE_STATUS, &result);
\tglGetShaderiv(rv, GL_INFO_LOG_LENGTH, &compile_log_len);
\tif (result != GL_TRUE)
\t{
\t\tstd::vector<GLchar> compile_log((size_t)(compile_log_len > 0 ? compile_log_len : 1) + 1, '\0');
\t\tGLsizei written = 0;
\t\tif (compile_log_len > 0)
\t\t\tglGetShaderInfoLog(rv, (GLsizei)compile_log.size(), &written, compile_log.data());
\t\tconst char *stage = type == GL_VERTEX_SHADER ? "vertex" : "fragment";
\t\tWARN_LOG(RENDERER, "Shader: failed to compile %s shader: %s", stage,
\t\t\t\tcompile_log_len > 0 ? compile_log.data() : "driver returned no compiler log");
\t\tINFO_LOG(RENDERER, "// %s SHADER SOURCE\n%s\n// END", stage, shader);
\t\tglDeleteShader(rv);
\t\treturn 0;
\t}
\treturn rv;
}
'''
    # The raw replacement above uses escaped tabs so that the C++ indentation stays stable.
    new_shader_function = new_shader_function.replace("\\t", "\t")
    source = source[:shader_start] + new_shader_function + source[shader_end:]

    link_start = source.index("GLuint gl_CompileAndLink(const char *vertexShader, const char *fragmentShader)\n{")
    link_end = source.index("\nPipelineShader *GetProgram(", link_start)
    old_link_function = source[link_start:link_end]
    if "glBindAttribLocation(program, VERTEX_POS_ARRAY" not in old_link_function:
        raise RuntimeError("program linker function does not match pinned source")
    new_link_function = r'''GLuint gl_CompileAndLink(const char *vertexShader, const char *fragmentShader)
{
\tGLuint vs = gl_CompileShader(vertexShader, GL_VERTEX_SHADER);
\tif (vs == 0)
\t\tthrow RendererException(i18n::T("OpenGL shader compilation failed"));

\tGLuint ps = gl_CompileShader(fragmentShader, GL_FRAGMENT_SHADER);
\tif (ps == 0)
\t{
\t\tglDeleteShader(vs);
\t\tthrow RendererException(i18n::T("OpenGL shader compilation failed"));
\t}

\tGLuint program = glCreateProgram();
\tif (program == 0)
\t{
\t\tglDeleteShader(vs);
\t\tglDeleteShader(ps);
\t\tthrow RendererException("OpenGL program creation failed");
\t}
\tglAttachShader(program, vs);
\tglAttachShader(program, ps);

\t// Bind vertex attributes to the VBO inputs.
\tglBindAttribLocation(program, VERTEX_POS_ARRAY,      "in_pos");
\tglBindAttribLocation(program, VERTEX_COL_BASE_ARRAY, "in_base");
\tglBindAttribLocation(program, VERTEX_COL_OFFS_ARRAY, "in_offs");
\tglBindAttribLocation(program, VERTEX_UV_ARRAY,       "in_uv");
\tglBindAttribLocation(program, VERTEX_COL_BASE1_ARRAY, "in_base1");
\tglBindAttribLocation(program, VERTEX_COL_OFFS1_ARRAY, "in_offs1");
\tglBindAttribLocation(program, VERTEX_UV1_ARRAY,       "in_uv1");
\tglBindAttribLocation(program, VERTEX_NORM_ARRAY,     "in_normal");

#ifndef GLES
\tif (!gl.is_gles && gl.gl_major >= 3)
\t\tglBindFragDataLocation(program, 0, "FragColor");
#endif

\tglLinkProgram(program);

\tGLint result = GL_FALSE;
\tglGetProgramiv(program, GL_LINK_STATUS, &result);
\tGLint compile_log_len = 0;
\tglGetProgramiv(program, GL_INFO_LOG_LENGTH, &compile_log_len);
\tif (result != GL_TRUE)
\t{
\t\tstd::vector<GLchar> compile_log((size_t)(compile_log_len > 0 ? compile_log_len : 1) + 1, '\0');
\t\tGLsizei written = 0;
\t\tif (compile_log_len > 0)
\t\t\tglGetProgramInfoLog(program, (GLsizei)compile_log.size(), &written, compile_log.data());
\t\tWARN_LOG(RENDERER, "Shader linking failed: %s",
\t\t\t\tcompile_log_len > 0 ? compile_log.data() : "driver returned no linker log");
\t\tINFO_LOG(RENDERER, "// VERTEX SHADER\n%s\n// END", vertexShader);
\t\tINFO_LOG(RENDERER, "// FRAGMENT SHADER\n%s\n// END", fragmentShader);
\t\tglDetachShader(program, vs);
\t\tglDetachShader(program, ps);
\t\tglDeleteShader(vs);
\t\tglDeleteShader(ps);
\t\tglDeleteProgram(program);
\t\tthrow RendererException(i18n::T("OpenGL shader compilation failed"));
\t}

\tglDetachShader(program, vs);
\tglDetachShader(program, ps);
\tglDeleteShader(vs);
\tglDeleteShader(ps);
\tglcache.UseProgram(program);
\treturn program;
}
'''
    new_link_function = new_link_function.replace("\\t", "\t")
    source = source[:link_start] + new_link_function + source[link_end:]
    return source


def patch_header(source: str) -> str:
    old = '\t\taddConstant("TARGET_GL", gl.gl_version);\n'
    new = old + '\t\taddConstant("HIGH_PRECISION_FLOAT", gl.highp_float_supported ? 1 : 0);\n'
    return replace_once(source, old, new, "highp capability shader define")


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("flycast_root", type=Path)
    args = parser.parse_args()
    root = args.flycast_root.resolve()

    head = subprocess.check_output(
        ["git", "-C", str(root), "rev-parse", "HEAD"], text=True
    ).strip()
    if head != PINNED_COMMIT:
        raise RuntimeError(f"Expected Flycast {PINNED_COMMIT}, found {head}")

    cpp_path = root / "core/rend/gles/gles.cpp"
    h_path = root / "core/rend/gles/gles.h"
    cpp = patch_cpp(cpp_path.read_text(encoding="utf-8"))
    header = patch_header(h_path.read_text(encoding="utf-8"))
    cpp_path.write_text(cpp, encoding="utf-8", newline="\n")
    h_path.write_text(header, encoding="utf-8", newline="\n")
    print("Applied Mali/GLES2 shader compatibility changes to the pinned Flycast source.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
