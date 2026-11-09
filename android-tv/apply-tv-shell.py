#!/usr/bin/env python3
"""Apply the maintained Android TV shell, settings bridge and GLES post-processors."""
from pathlib import Path
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "third_party" / "flycast"
ANDROID_NS = "http://schemas.android.com/apk/res/android"
ET.register_namespace("android", ANDROID_NS)
ET.register_namespace("tools", "http://schemas.android.com/tools")
A = "{" + ANDROID_NS + "}"

def patch_text(relative_path, replacements):
    path = CORE / relative_path
    if not path.is_file():
        raise SystemExit("Required Flycast file not found: " + str(path))
    source = path.read_text(encoding="utf-8")
    for old, new in replacements:
        if new.strip() in source:
            continue
        if old not in source:
            raise SystemExit("Patch anchor not found in " + relative_path + ": " + old[:100])
        source = source.replace(old, new, 1)
    path.write_text(source, encoding="utf-8")

# 1. Install the maintained Russian Android TV launcher.
source = ROOT / "android-tv" / "src" / "main" / "java" / "com" / "flycast" / "emulator" / "TvHomeActivity.java"
destination = CORE / "shell" / "android-studio" / "flycast" / "src" / "main" / "java" / "com" / "flycast" / "emulator" / "TvHomeActivity.java"
manifest = CORE / "shell" / "android-studio" / "flycast" / "src" / "main" / "AndroidManifest.xml"
if not source.is_file() or not manifest.is_file():
    raise SystemExit("Required Android source or manifest not found")
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_text(source.read_text(encoding="utf-8"), encoding="utf-8")

tree = ET.parse(manifest)
root = tree.getroot()
application = root.find("application")
if application is None:
    raise SystemExit("Flycast AndroidManifest.xml has no application element")
application.set(A + "label", "Dreamcast TV")
activity_name = "com.flycast.emulator.TvHomeActivity"
activity = next((child for child in application.findall("activity")
                 if child.get(A + "name") in (activity_name, ".TvHomeActivity")), None)
if activity is None:
    activity = ET.Element("activity", {
        A + "name": activity_name,
        A + "exported": "false",
        A + "screenOrientation": "landscape",
        A + "theme": "@style/Theme.AppCompat.NoActionBar",
    })
    native_index = next((i for i, child in enumerate(list(application))
                         if child.tag == "activity" and child.get(A + "name", "").endswith("NativeGLActivity")),
                        len(application))
    application.insert(native_index, activity)
launcher_alias = next((alias for alias in application.findall("activity-alias")
                       if any(category.get(A + "name") == "android.intent.category.LEANBACK_LAUNCHER"
                              for intent_filter in alias.findall("intent-filter")
                              for category in intent_filter.findall("category"))), None)
if launcher_alias is None:
    raise SystemExit("Could not find the Leanback launcher activity-alias")
launcher_alias.set(A + "targetActivity", activity_name)
ET.indent(tree, space="    ")
tree.write(manifest, encoding="utf-8", xml_declaration=True)

# 2. Expose the real Flycast graphics config bridge to Java.
patch_text(
    Path("shell/android-studio/flycast/src/main/java/com/flycast/emulator/emu/JNIdc.java"),
    [(
        "public static native void postRestore(String filesDir);",
        "public static native void postRestore(String filesDir);\n"
        "\tpublic static native void applyTvGraphicsSettings(int profile, int renderHeight, int retroFilter);"
    )]
)

# Apply TV preferences after Flycast has loaded its config and before a selected game starts.
patch_text(
    Path("shell/android-studio/flycast/src/main/java/com/flycast/emulator/BaseGLActivity.java"),
    [(
        'Log.i("flycast", "Environment initialized");',
        'Log.i("flycast", "Environment initialized");\n'
        '        JNIdc.applyTvGraphicsSettings(\n'
        '                prefs.getInt("tv_graphics_profile", 1),\n'
        '                prefs.getInt("tv_render_height", 720),\n'
        '                prefs.getInt("tv_retro_filter", 0));'
    )]
)

# Actual core config option for one optional full-screen GLES shader filter.
patch_text(
    Path("core/cfg/option.h"),
    [(
        "extern Option<bool> FramePacing;",
        "extern Option<bool> FramePacing;\n"
        "extern Option<int> AndroidRetroFilter; // 0 off, 1 CRT, 2 old TV, 3 composite, 4 RGB mask, 5 VHS"
    )]
)
patch_text(
    Path("core/cfg/option.cpp"),
    [(
        'Option<bool> FramePacing("rend.FramePacing", true);',
        'Option<bool> FramePacing("rend.FramePacing", true);\n'
        'Option<int> AndroidRetroFilter("rend.AndroidRetroFilter", 0);'
    )]
)

# JNI config bridge: presets alter real Flycast options; 50/60 Hz emulated timing stays native.
android_cpp = CORE / "shell" / "android-studio" / "flycast" / "src" / "main" / "jni" / "src" / "Android.cpp"
cpp = android_cpp.read_text(encoding="utf-8")
bridge_marker = "Java_com_flycast_emulator_emu_JNIdc_applyTvGraphicsSettings"
if bridge_marker not in cpp:
    cpp += r"""

extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_emu_JNIdc_applyTvGraphicsSettings(
        JNIEnv *env, jclass clazz, jint profile, jint renderHeight, jint retroFilter)
{
    int safeHeight = 480;
    switch (renderHeight)
    {
    case 480: case 720: case 960: case 1200: case 1440: case 1800:
        safeHeight = renderHeight;
        break;
    default:
        safeHeight = 480;
        break;
    }

    int safeFilter = (retroFilter >= 0 && retroFilter <= 5) ? retroFilter : 0;

    switch (profile)
    {
    case 0: // Performance
        config::AutoSkipFrame = 1;
        config::AnisotropicFiltering = 1;
        break;
    case 2: // Quality
        config::AutoSkipFrame = 0;
        config::AnisotropicFiltering = 4;
        break;
    case 1: // Balanced
    default:
        config::AutoSkipFrame = 1;
        config::AnisotropicFiltering = 2;
        break;
    }

    // These options keep presentation synchronized without changing Dreamcast game time.
    config::RenderResolution = safeHeight;
    config::AndroidRetroFilter = safeFilter;
    config::VSync = true;
    config::FramePacing = true;
    config::ThreadedRendering = true;
    config::SkipFrame = 0;

    // Let the core choose its native game region and PAL/NTSC cadence.
    config::Region = 3;
    config::Broadcast = 4;
    config::Settings::instance().save();
}
"""
    android_cpp.write_text(cpp, encoding="utf-8")

# 3. Add a real 1800-line internal resolution to Flycast's own video settings.
patch_text(
    Path("core/ui/settings_video.cpp"),
    [(
        "const std::array<float, 13> scalings{ 0.5f, 1.f, 1.5f, 2.f, 2.5f, 3.f, 4.f, 4.5f, 5.f, 6.f, 7.f, 8.f, 9.f };",
        "const std::array<float, 14> scalings{ 0.5f, 1.f, 1.5f, 2.f, 2.5f, 3.f, 3.75f, 4.f, 4.5f, 5.f, 6.f, 7.f, 8.f, 9.f };"
    ),(
        "const std::array<std::string, 13> scalingsText{ T(\"Half\"), T(\"Native\"), \"x1.5\", \"x2\", \"x2.5\", \"x3\", \"x4\", \"x4.5\", \"x5\", \"x6\", \"x7\", \"x8\", \"x9\" };",
        "const std::array<std::string, 14> scalingsText{ T(\"Half\"), T(\"Native\"), \"x1.5\", \"x2\", \"x2.5\", \"x3\", \"x3.75 (1800p)\", \"x4\", \"x4.5\", \"x5\", \"x6\", \"x7\", \"x8\", \"x9\" };"
    )]
)

# 4. Make shader variants part of Flycast's real final GLES blit path.
patch_text(
    Path("core/rend/gles/quad.h"),
    [(
        "void draw(GLuint texId, bool rotate = false, bool swapY = false, const float *coords = nullptr, const float *color = nullptr);",
        "void draw(GLuint texId, bool rotate = false, bool swapY = false, const float *coords = nullptr, const float *color = nullptr, int filterMode = 0, int textureWidth = 640, int textureHeight = 480);"
    ),(
        "GLuint shader;\n\tGLint tintUniform;\n\tGLuint rot90shader;\n\tGLint rot90TintUniform;",
        "GLuint shader[6] {};\n\tGLint tintUniform[6] {};\n"
        "\tGLuint rot90shader[6] {};\n\tGLint rot90TintUniform[6] {};\n"
        "\tGLint textureSizeUniform[6] {};"
    )]
)

quad_cpp = CORE / "core" / "rend" / "gles" / "quad.cpp"
qs = quad_cpp.read_text(encoding="utf-8")
frag_start = qs.index('static const char* FragmentShader = R"(')
frag_end = qs.index(')";', frag_start) + 3
fragment = r'''static const char* FragmentShader = R"(
in mediump vec2 vtx_uv;
uniform sampler2D tex;
uniform mediump vec4 tint;
uniform mediump vec2 texSize;

void main()
{
    vec2 uv = vtx_uv;
#if FILTER_MODE == 5
    float warp = sin(uv.y * texSize.y * 0.043) * 0.22 / texSize.x;
    uv.x = clamp(uv.x + warp, 0.0, 1.0);
#endif
    vec4 color = texture(tex, uv);
#if FILTER_MODE == 1
    float row = mod(floor(uv.y * texSize.y), 2.0);
    float scan = mix(0.86, 1.0, row);
    float vignette = clamp(16.0 * uv.x * uv.y * (1.0 - uv.x) * (1.0 - uv.y), 0.0, 1.0);
    color.rgb *= scan * (0.88 + 0.12 * vignette);
#elif FILTER_MODE == 2
    vec2 px = vec2(0.0, 1.0 / texSize.y);
    vec3 soft = texture(tex, uv - px).rgb + color.rgb * 2.0 + texture(tex, uv + px).rgb;
    float row = mod(floor(uv.y * texSize.y), 2.0);
    color.rgb = mix(color.rgb, soft * 0.25, 0.18) * mix(0.90, 1.0, row);
#elif FILTER_MODE == 3
    vec2 px = vec2(1.0 / texSize.x, 0.0);
    vec3 leftColor = texture(tex, clamp(uv - px, 0.0, 1.0)).rgb;
    vec3 rightColor = texture(tex, clamp(uv + px, 0.0, 1.0)).rgb;
    vec3 chromaSoft = (leftColor + color.rgb * 2.0 + rightColor) * 0.25;
    color.rgb = mix(color.rgb, chromaSoft, 0.28);
#elif FILTER_MODE == 4
    float triad = mod(floor(uv.x * texSize.x), 3.0);
    vec3 mask = vec3(0.78);
    if (triad < 1.0) mask.r = 1.0;
    else if (triad < 2.0) mask.g = 1.0;
    else mask.b = 1.0;
    color.rgb *= mix(vec3(1.0), mask, 0.24);
#elif FILTER_MODE == 5
    vec2 px = vec2(1.0 / texSize.x, 0.0);
    vec4 leftColor = texture(tex, clamp(uv - px, 0.0, 1.0));
    vec4 rightColor = texture(tex, clamp(uv + px, 0.0, 1.0));
    vec3 chromaBleed = vec3(rightColor.r, color.g, leftColor.b);
    color.rgb = mix(color.rgb, chromaBleed, 0.22);
#endif
    gl_FragColor = tint * color;
}
)";'''
qs = qs[:frag_start] + fragment + qs[frag_end:]

ctor_start = qs.index("GlQuadDrawer::GlQuadDrawer()")
draw_start = qs.index("void GlQuadDrawer::draw(", ctor_start)
ctor_and_dtor = r'''GlQuadDrawer::GlQuadDrawer()
{
    for (int filter = 0; filter < 6; ++filter)
    {
        try
        {
            OpenGlSource fragmentShader;
            fragmentShader.addConstant("FILTER_MODE", filter)
                    .addSource(PixelCompatShader)
                    .addSource(FragmentShader);
            OpenGlSource vertexShader;
            vertexShader.addConstant("ROTATE", 0)
                    .addSource(VertexCompatShader)
                    .addSource(VertexShader);
            const std::string fragmentGlsl = fragmentShader.generate();
            const std::string vertexGlsl = vertexShader.generate();
            shader[filter] = gl_CompileAndLink(vertexGlsl.c_str(), fragmentGlsl.c_str());
            GLint tex = glGetUniformLocation(shader[filter], "tex");
            glUniform1i(tex, 0);
            tintUniform[filter] = glGetUniformLocation(shader[filter], "tint");
            textureSizeUniform[filter] = glGetUniformLocation(shader[filter], "texSize");

            vertexShader.setConstant("ROTATE", 1);
            const std::string rotatedVertexGlsl = vertexShader.generate();
            rot90shader[filter] = gl_CompileAndLink(rotatedVertexGlsl.c_str(), fragmentGlsl.c_str());
            tex = glGetUniformLocation(rot90shader[filter], "tex");
            glUniform1i(tex, 0);
            rot90TintUniform[filter] = glGetUniformLocation(rot90shader[filter], "tint");
        }
        catch (...)
        {
            if (filter == 0)
                throw;
            shader[filter] = 0;
            rot90shader[filter] = 0;
        }
    }

    quadIndexBuffer = std::make_unique<GlBuffer>(GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW);
    static const GLushort indices[] = { 0, 1, 2, 1, 3 };
    quadIndexBuffer->update(indices, sizeof(indices));

    quadBuffer = std::make_unique<GlBuffer>(GL_ARRAY_BUFFER, GL_STATIC_DRAW);
    {
        float vertices[4][5] = {
            { -1.f,  1.f, 1.f, 0.f, 1.f },
            { -1.f, -1.f, 1.f, 0.f, 0.f },
            {  1.f,  1.f, 1.f, 1.f, 1.f },
            {  1.f, -1.f, 1.f, 1.f, 0.f },
        };
        quadBuffer->update(vertices, sizeof(vertices));
    }
    quadBufferSwapY = std::make_unique<GlBuffer>(GL_ARRAY_BUFFER, GL_STATIC_DRAW);
    {
        float vertices[4][5] = {
            { -1.f,  1.f, 1.f, 0.f, 0.f },
            { -1.f, -1.f, 1.f, 0.f, 1.f },
            {  1.f,  1.f, 1.f, 1.f, 0.f },
            {  1.f, -1.f, 1.f, 1.f, 1.f },
        };
        quadBufferSwapY->update(vertices, sizeof(vertices));
    }
    quadBufferCustom = std::make_unique<GlBuffer>(GL_ARRAY_BUFFER);
    glCheck();
}

GlQuadDrawer::~GlQuadDrawer()
{
    quadIndexBuffer.reset();
    quadBuffer.reset();
    quadBufferSwapY.reset();
    quadBufferCustom.reset();

    quadVertexArray.term();
    quadVertexArraySwapY.term();
    quadVertexArrayCustom.term();

    for (int filter = 0; filter < 6; ++filter)
    {
        if (shader[filter] != 0)
            glcache.DeleteProgram(shader[filter]);
        if (rot90shader[filter] != 0)
            glcache.DeleteProgram(rot90shader[filter]);
        shader[filter] = rot90shader[filter] = 0;
    }
}

'''
qs = qs[:ctor_start] + ctor_and_dtor + qs[draw_start:]
draw = r'''void GlQuadDrawer::draw(GLuint texId, bool rotate, bool swapY, const float *coords,
        const float *color, int filterMode, int textureWidth, int textureHeight)
{
    glcache.Disable(GL_SCISSOR_TEST);
    glcache.Disable(GL_DEPTH_TEST);
    glcache.Disable(GL_STENCIL_TEST);
    glcache.Disable(GL_CULL_FACE);

    if (filterMode < 0 || filterMode >= 6)
        filterMode = 0;
    if (shader[filterMode] == 0 || (rotate && rot90shader[filterMode] == 0))
        filterMode = 0;

    glcache.UseProgram(rotate ? rot90shader[filterMode] : shader[filterMode]);

    glActiveTexture(GL_TEXTURE0);
    glcache.BindTexture(GL_TEXTURE_2D, texId);

    if (color == nullptr) {
        static constexpr float white[4] { 1.f, 1.f, 1.f, 1.f };
        color = white;
    }
    glUniform4fv(rotate ? rot90TintUniform[filterMode] : tintUniform[filterMode], 1, color);
    if (textureSizeUniform[filterMode] != -1)
        glUniform2f(textureSizeUniform[filterMode],
                (GLfloat)std::max(textureWidth, 1), (GLfloat)std::max(textureHeight, 1));

    if (coords == nullptr)
    {
        if (swapY)
            quadVertexArraySwapY.bind(quadBufferSwapY.get(), quadIndexBuffer.get());
        else
            quadVertexArray.bind(quadBuffer.get(), quadIndexBuffer.get());
    }
    else
    {
        quadBufferCustom->update(coords, sizeof(float) * 4 * 5);
        quadVertexArrayCustom.bind(quadBufferCustom.get(), quadIndexBuffer.get());
    }

    glDrawElements(GL_TRIANGLE_STRIP, 5, GL_UNSIGNED_SHORT, (GLvoid *)0);
    GlVertexArray::unbind();
    glCheck();
}
'''
qs = qs[:draw_start] + draw
quad_cpp.write_text(qs, encoding="utf-8")

# Force the final texture pass when a selected shader filter is active.
patch_text(
    Path("core/rend/gles/gldraw.cpp"),
    [(
        "if (gl.bogusBlitFramebuffer || config::Rotate90)",
        "if (gl.bogusBlitFramebuffer || config::Rotate90 || config::AndroidRetroFilter > 0)"
    ),(
        "gl.quadDrawer->draw(framebuffer->getTexture(), config::Rotate90, true, vertices);",
        "gl.quadDrawer->draw(framebuffer->getTexture(), config::Rotate90, true, vertices, nullptr,\n"
        "\t\t\t\tconfig::AndroidRetroFilter, framebuffer->getWidth(), framebuffer->getHeight());"
    )]
)

print("Android TV launcher, native graphics presets, SSAA resolution and GLES retro filters applied.")
