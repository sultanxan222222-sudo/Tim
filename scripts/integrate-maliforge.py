#!/usr/bin/env python3
"""Integrate the MaliForge GLES2 presentation stage into pinned Flycast."""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

PINNED_COMMIT = "72b8e67141d85ca5c9259f04563b49f7f92f928b"


def replace_once(source: str, old: str, new: str, label: str) -> str:
    count = source.count(old)
    if count != 1:
        raise RuntimeError(f"{label}: expected exactly one anchor, found {count}")
    return source.replace(old, new, 1)


def patch_quad_header(source: str) -> str:
    source = replace_once(
        source,
        "void draw(GLuint texId, bool rotate = false, bool swapY = false, const float *coords = nullptr, const float *color = nullptr);",
        """void draw(GLuint texId, bool rotate = false, bool swapY = false, const float *coords = nullptr, const float *color = nullptr);
\t// MaliForge's GLES2 final-frame compositor. Returns false when its shader
\t// could not be initialized, allowing Flycast's original quad path to run.
\tbool drawMaliForge(GLuint texId, bool rotate, bool swapY, const float *coords,
\t\t\tconst float *color, int sourceWidth, int sourceHeight, float sharpness);""",
        "quad drawer API",
    )
    source = replace_once(
        source,
        "\tGLint rot90TintUniform;\n",
        "\tGLint rot90TintUniform;\n\tGLuint maliforgeShader = 0;\n\tGLuint maliforgeRot90shader = 0;\n\tGLint maliforgeTintUniform = -1;\n\tGLint maliforgeRot90TintUniform = -1;\n\tGLint maliforgeTexelUniform = -1;\n\tGLint maliforgeRot90TexelUniform = -1;\n\tGLint maliforgeSharpnessUniform = -1;\n\tGLint maliforgeRot90SharpnessUniform = -1;\n",
        "MaliForge shader handles",
    )
    return source


def patch_quad_cpp(source: str) -> str:
    source = replace_once(
        source,
        '#include "quad.h"\n',
        '#include "quad.h"\n#include <algorithm>\n',
        "algorithm include",
    )

    fragment_anchor = """)";

GlQuadDrawer::GlQuadDrawer()"""
    maliforge_shader = """)";

static const char* MaliForgeFragmentShader = R"(
in mediump vec2 vtx_uv;

uniform sampler2D tex;
uniform mediump vec4 tint;
uniform mediump vec2 texelSize;
uniform lowp float sharpness;

void main()
{
\tmediump vec4 center = texture(tex, vtx_uv);
\tmediump vec3 neighbours =
\t\t\ttexture(tex, vtx_uv + vec2(0.0, -texelSize.y)).rgb +
\t\t\ttexture(tex, vtx_uv + vec2(0.0,  texelSize.y)).rgb +
\t\t\ttexture(tex, vtx_uv + vec2(-texelSize.x, 0.0)).rgb +
\t\t\ttexture(tex, vtx_uv + vec2( texelSize.x, 0.0)).rgb;
\tneighbours *= 0.25;

\tmediump float centerLuma = dot(center.rgb, vec3(0.299, 0.587, 0.114));
\tmediump float neighbourLuma = dot(neighbours, vec3(0.299, 0.587, 0.114));
\tlowp float edge = clamp(abs(centerLuma - neighbourLuma) * 5.0, 0.0, 1.0);
\tmediump vec3 detail = center.rgb - neighbours;
\tmediump vec3 result = clamp(center.rgb + detail * (sharpness * edge), 0.0, 1.0);
\tgl_FragColor = tint * vec4(result, center.a);
}
)";

GlQuadDrawer::GlQuadDrawer()"""
    source = replace_once(source, fragment_anchor, maliforge_shader, "MaliForge GLES2 shader source")

    constructor_anchor = '\trot90TintUniform = glGetUniformLocation(rot90shader, "tint");\n'
    constructor_addition = constructor_anchor + """
\t// MaliForge is a conservative GPU-side finishing pass. Its shader is
\t// optional: a driver/compiler rejection leaves the stock Flycast path intact.
\ttry
\t{
\t\tOpenGlSource mfFragmentShader;
\t\tmfFragmentShader.addSource(PixelCompatShader)
\t\t\t\t.addSource(MaliForgeFragmentShader);
\t\tconst std::string mfFragmentGlsl = mfFragmentShader.generate();

\t\tOpenGlSource mfVertexShader;
\t\tmfVertexShader.addConstant("ROTATE", 0)
\t\t\t\t.addSource(VertexCompatShader)
\t\t\t\t.addSource(VertexShader);
\t\tconst std::string mfVertexGlsl = mfVertexShader.generate();
\t\tmaliforgeShader = gl_CompileAndLink(mfVertexGlsl.c_str(), mfFragmentGlsl.c_str());

\t\tmfVertexShader.setConstant("ROTATE", 1);
\t\tconst std::string mfRotatedVertexGlsl = mfVertexShader.generate();
\t\tmaliforgeRot90shader = gl_CompileAndLink(mfRotatedVertexGlsl.c_str(), mfFragmentGlsl.c_str());

\t\tglcache.UseProgram(maliforgeShader);
\t\tglUniform1i(glGetUniformLocation(maliforgeShader, "tex"), 0);
\t\tmaliforgeTintUniform = glGetUniformLocation(maliforgeShader, "tint");
\t\tmaliforgeTexelUniform = glGetUniformLocation(maliforgeShader, "texelSize");
\t\tmaliforgeSharpnessUniform = glGetUniformLocation(maliforgeShader, "sharpness");

\t\tglcache.UseProgram(maliforgeRot90shader);
\t\tglUniform1i(glGetUniformLocation(maliforgeRot90shader, "tex"), 0);
\t\tmaliforgeRot90TintUniform = glGetUniformLocation(maliforgeRot90shader, "tint");
\t\tmaliforgeRot90TexelUniform = glGetUniformLocation(maliforgeRot90shader, "texelSize");
\t\tmaliforgeRot90SharpnessUniform = glGetUniformLocation(maliforgeRot90shader, "sharpness");
\t}
\tcatch (...)
\t{
\t\tif (maliforgeShader != 0)
\t\t\tglcache.DeleteProgram(maliforgeShader);
\t\tif (maliforgeRot90shader != 0)
\t\t\tglcache.DeleteProgram(maliforgeRot90shader);
\t\tmaliforgeShader = maliforgeRot90shader = 0;
\t\tmaliforgeTintUniform = maliforgeRot90TintUniform = -1;
\t\tmaliforgeTexelUniform = maliforgeRot90TexelUniform = -1;
\t\tmaliforgeSharpnessUniform = maliforgeRot90SharpnessUniform = -1;
\t}
"""
    source = replace_once(source, constructor_anchor, constructor_addition, "MaliForge program initialization")

    destructor_anchor = """\tglcache.DeleteProgram(rot90shader);
\trot90shader = 0;
}"""
    destructor_addition = """\tglcache.DeleteProgram(rot90shader);
\trot90shader = 0;
\tif (maliforgeShader != 0)
\t\tglcache.DeleteProgram(maliforgeShader);
\tmaliforgeShader = 0;
\tif (maliforgeRot90shader != 0)
\t\tglcache.DeleteProgram(maliforgeRot90shader);
\tmaliforgeRot90shader = 0;
}"""
    source = replace_once(source, destructor_anchor, destructor_addition, "MaliForge program cleanup")

    if "bool GlQuadDrawer::drawMaliForge(" in source:
        raise RuntimeError("MaliForge draw function already exists; refusing duplicate integration")
    method = """

bool GlQuadDrawer::drawMaliForge(GLuint texId, bool rotate, bool swapY, const float *coords,
\t\tconst float *color, int sourceWidth, int sourceHeight, float sharpness)
{
\tconst GLuint program = rotate ? maliforgeRot90shader : maliforgeShader;
\tconst GLint tintLocation = rotate ? maliforgeRot90TintUniform : maliforgeTintUniform;
\tconst GLint texelLocation = rotate ? maliforgeRot90TexelUniform : maliforgeTexelUniform;
\tconst GLint sharpnessLocation = rotate ? maliforgeRot90SharpnessUniform : maliforgeSharpnessUniform;
\tif (program == 0 || tintLocation < 0 || texelLocation < 0 || sharpnessLocation < 0)
\t\treturn false;

\tglcache.Disable(GL_SCISSOR_TEST);
\tglcache.Disable(GL_DEPTH_TEST);
\tglcache.Disable(GL_STENCIL_TEST);
\tglcache.Disable(GL_CULL_FACE);
\tglcache.UseProgram(program);

\tglActiveTexture(GL_TEXTURE0);
\tglcache.BindTexture(GL_TEXTURE_2D, texId);

\tif (color == nullptr) {
\t\tstatic constexpr float white[4] { 1.f, 1.f, 1.f, 1.f };
\t\tcolor = white;
\t}
\tglUniform4fv(tintLocation, 1, color);
\tglUniform2f(texelLocation,
\t\t\t1.f / (float)std::max(1, sourceWidth),
\t\t\t1.f / (float)std::max(1, sourceHeight));
\tglUniform1f(sharpnessLocation, std::clamp(sharpness, 0.f, 0.25f));

\tif (coords == nullptr)
\t{
\t\tif (swapY)
\t\t\tquadVertexArraySwapY.bind(quadBufferSwapY.get(), quadIndexBuffer.get());
\t\telse
\t\t\tquadVertexArray.bind(quadBuffer.get(), quadIndexBuffer.get());
\t}
\telse
\t{
\t\tquadBufferCustom->update(coords, sizeof(float) * 4 * 5);
\t\tquadVertexArrayCustom.bind(quadBufferCustom.get(), quadIndexBuffer.get());
\t}

\tglDrawElements(GL_TRIANGLE_STRIP, 5, GL_UNSIGNED_SHORT, (GLvoid *)0);
\tGlVertexArray::unbind();
\tglCheck();
\treturn true;
}
"""
    source = source.rstrip() + method + "\n"
    return source


def patch_gldraw(source: str) -> str:
    source = replace_once(
        source,
        "if (gl.bogusBlitFramebuffer || config::Rotate90)",
        "if (gl.bogusBlitFramebuffer || config::Rotate90 || (gl.mali && gl.is_gles))",
        "use MaliForge when the GLES Mali presenter is available",
    )
    old = "\t\tgl.quadDrawer->draw(framebuffer->getTexture(), config::Rotate90, true, vertices);"
    new = """\t\t// MaliForge runs after the Dreamcast PVR frame has been rendered by
\t\t// Flycast. It only touches the final presentation quad; OSD/UI is drawn later.
\t\tconst bool maliForgePresented = gl.mali && gl.is_gles
\t\t\t\t&& gl.quadDrawer->drawMaliForge(framebuffer->getTexture(), config::Rotate90, true,
\t\t\t\t\t\tvertices, nullptr, framebuffer->getWidth(), framebuffer->getHeight(), 0.14f);
\t\tif (!maliForgePresented)
\t\t\tgl.quadDrawer->draw(framebuffer->getTexture(), config::Rotate90, true, vertices);"""
    return replace_once(source, old, new, "MaliForge final-frame hook")


def patch_branding(source: str) -> str:
    return replace_once(
        source,
        '<string name="app_name" translatable="false">Flycast</string>',
        '<string name="app_name" translatable="false">MaliForge Dreamcast</string>',
        "Android launcher branding",
    )


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

    targets = {
        root / "core/rend/gles/quad.h": patch_quad_header,
        root / "core/rend/gles/quad.cpp": patch_quad_cpp,
        root / "core/rend/gles/gldraw.cpp": patch_gldraw,
        root / "shell/android-studio/flycast/src/main/res/values/donottranslate.xml": patch_branding,
    }

    for path, patcher in targets.items():
        original = path.read_text(encoding="utf-8")
        updated = patcher(original)
        path.write_text(updated, encoding="utf-8", newline="\n")

    print("MaliForge GLES2 final-frame compositor integrated into Flycast.")
    print("Fallback: stock Flycast quad renderer remains active if MaliForge shader setup fails.")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        raise SystemExit(1)
