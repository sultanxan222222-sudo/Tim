#!/usr/bin/env python3
"""Apply the Android TV shell and carefully verified Flycast TV patches."""
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "third_party" / "flycast"
ANDROID_NS = "http://schemas.android.com/apk/res/android"
ET.register_namespace("android", ANDROID_NS)
ET.register_namespace("tools", "http://schemas.android.com/tools")
A = "{" + ANDROID_NS + "}"


def replace_once(path: Path, old: str, new: str, description: str) -> None:
    if not path.is_file():
        raise SystemExit("Required Flycast source not found: " + str(path))
    content = path.read_text(encoding="utf-8")
    if new in content:
        return
    count = content.count(old)
    if count != 1:
        raise SystemExit(
            f"Cannot apply {description}: expected one source anchor in {path}, found {count}"
        )
    path.write_text(content.replace(old, new, 1), encoding="utf-8")


# Copy the TV entry point.
source = ROOT / "android-tv" / "src" / "main" / "java" / "com" / "flycast" / "emulator" / "TvHomeActivity.java"
destination = CORE / "shell" / "android-studio" / "flycast" / "src" / "main" / "java" / "com" / "flycast" / "emulator" / "TvHomeActivity.java"
manifest = CORE / "shell" / "android-studio" / "flycast" / "src" / "main" / "AndroidManifest.xml"
for required in (source, manifest):
    if not required.is_file():
        raise SystemExit("Required Android source not found: " + str(required))
destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_text(source.read_text(encoding="utf-8"), encoding="utf-8")

# Register the TV launcher while retaining the original native activity.
tree = ET.parse(manifest)
root = tree.getroot()
application = root.find("application")
if application is None:
    raise SystemExit("Flycast AndroidManifest.xml has no application element")
application.set(A + "label", "Dreamcast TV")
activity_name = "com.flycast.emulator.TvHomeActivity"
activity = next(
    (child for child in application.findall("activity")
     if child.get(A + "name") in (activity_name, ".TvHomeActivity")),
    None
)
if activity is None:
    activity = ET.Element("activity", {
        A + "name": activity_name,
        A + "exported": "false",
        A + "screenOrientation": "landscape",
        A + "theme": "@style/Theme.AppCompat.NoActionBar",
    })
    native_index = next(
        (i for i, child in enumerate(list(application))
         if child.tag == "activity" and child.get(A + "name", "").endswith("NativeGLActivity")),
        len(application)
    )
    application.insert(native_index, activity)
launcher_alias = next(
    (alias for alias in application.findall("activity-alias")
     if any(
         category.get(A + "name") == "android.intent.category.LEANBACK_LAUNCHER"
         for intent_filter in alias.findall("intent-filter")
         for category in intent_filter.findall("category")
     )),
    None
)
if launcher_alias is None:
    raise SystemExit("Could not find the Leanback launcher activity-alias")
launcher_alias.set(A + "targetActivity", activity_name)
ET.indent(tree, space="    ")
tree.write(manifest, encoding="utf-8", xml_declaration=True)

# Expose one Flycast-owned option for the actual OpenGL post-process shader.
replace_once(
    CORE / "core" / "cfg" / "option.h",
    'extern Option<int> RenderResolution;\n',
    'extern Option<int> RenderResolution;\nextern Option<int> VideoFilter;\n',
    "VideoFilter option declaration",
)
replace_once(
    CORE / "core" / "cfg" / "option.cpp",
    'Option<int> RenderResolution("rend.Resolution", 480);\n',
    'Option<int> RenderResolution("rend.Resolution", 480);\nOption<int> VideoFilter("rend.VideoFilter", 0);\n',
    "VideoFilter option definition",
)

# Reload emu.cfg whenever the TV frontend launches the native activity. This makes
# changed profile/filter values take effect even if Android kept the Application alive.
jni_java = CORE / "shell" / "android-studio" / "flycast" / "src" / "main" / "java" / "com" / "flycast" / "emulator" / "emu" / "JNIdc.java"
replace_once(
    jni_java,
    '\tpublic static native void setGameUri(String fileName);\n',
    '\tpublic static native void setGameUri(String fileName);\n\tpublic static native void applyTvSettings();\n',
    "TV settings JNI declaration",
)
base_activity = CORE / "shell" / "android-studio" / "flycast" / "src" / "main" / "java" / "com" / "flycast" / "emulator" / "BaseGLActivity.java"
replace_once(
    base_activity,
    '        if (result != null) {\n            showAlertDialog(result);\n            return;\n        }\n        Log.i("flycast", "Environment initialized");',
    '        if (result != null) {\n            showAlertDialog(result);\n            return;\n        }\n        JNIdc.applyTvSettings();\n        Log.i("flycast", "Environment initialized");',
    "apply saved TV settings before surface creation",
)
android_cpp = CORE / "shell" / "android-studio" / "flycast" / "src" / "main" / "jni" / "src" / "Android.cpp"
replace_once(
    android_cpp,
    'extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_emu_JNIdc_setGameUri(JNIEnv *env, jobject obj, jstring jfileName)',
    'extern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_emu_JNIdc_applyTvSettings(JNIEnv *env, jobject obj)\n{\n\tif (config::open())\n\t\tconfig::Settings::instance().load(false);\n}\n\nextern "C" JNIEXPORT void JNICALL Java_com_flycast_emulator_emu_JNIdc_setGameUri(JNIEnv *env, jobject obj, jstring jfileName)',
    "native Flycast settings reload",
)

# The RK3528 Mali-450 target is OpenGL ES 2.0 class. Upstream's generic
# Android CMake default selects the ES3 shader path; build an explicit ES2
# renderer and ES2 ImGui backend so the app can run on this target.
cmake_root = CORE / "CMakeLists.txt"
replace_once(
    cmake_root,
    'target_compile_definitions(${PROJECT_NAME} PRIVATE GLES GLES3)',
    '''if(USE_GLES2)
            target_compile_definitions(${PROJECT_NAME} PRIVATE GLES GLES2 IMGUI_IMPL_OPENGL_ES2)
        else()
            target_compile_definitions(${PROJECT_NAME} PRIVATE GLES GLES3)
        endif()''',
    "select GLES2 for RK3528 when requested",
)
gradle_file = CORE / "shell" / "android-studio" / "flycast" / "build.gradle.kts"
replace_once(
    gradle_file,
    '                arguments += "-DANDROID_ARM_MODE=arm"\n',
    '                arguments += "-DANDROID_ARM_MODE=arm"\n'
    '                arguments += "-DUSE_GLES2=ON"\n'
    '                arguments += "-DUSE_OPENGL=ON"\n'
    '                arguments += "-DUSE_VULKAN=OFF"\n',
    "configure the Android TV build for the RK3528 GLES2 GPU",
)
replace_once(
    gradle_file,
    '        minSdk = 21\n        targetSdk = 36\n',
    '        minSdk = 21\n'
    '        targetSdk = 36\n'
    '        ndk { abiFilters += setOf("arm64-v8a", "armeabi-v7a") }\n',
    "package both 64-bit and 32-bit ARM TV boxes",
)

# Add a lightweight post-process pass to Flycast's final GLES presentation quad.
quad_h = CORE / "core" / "rend" / "gles" / "quad.h"
replace_once(
    quad_h,
    'void draw(GLuint texId, bool rotate = false, bool swapY = false, const float *coords = nullptr, const float *color = nullptr);',
    'void draw(GLuint texId, bool rotate = false, bool swapY = false, const float *coords = nullptr, const float *color = nullptr, bool applyVideoFilter = false);',
    "quad draw filter argument",
)
replace_once(
    quad_h,
    '\tGLint tintUniform;\n',
    '\tGLint tintUniform;\n\tGLint videoFilterUniform;\n\tGLint outputSizeUniform;\n',
    "quad shader uniform fields",
)
quad_cpp = CORE / "core" / "rend" / "gles" / "quad.cpp"
replace_once(
    quad_cpp,
    '#include "quad.h"\n',
    '#include "quad.h"\n#include "cfg/option.h"\n',
    "quad Flycast config include",
)
replace_once(
    quad_cpp,
    'uniform mediump vec4 tint;\n\nvoid main()\n{\n\tgl_FragColor = tint * texture(tex, vtx_uv);\n}',
    '''uniform mediump vec4 tint;
uniform int videoFilter;
uniform mediump vec2 outputSize;

void main()
{
    vec2 uv = vtx_uv;
    vec4 pixel = texture(tex, uv);
    vec2 texel = vec2(1.0) / max(outputSize, vec2(1.0));

    if (videoFilter == 1) {
        // CRT: scan lines and a soft edge falloff.
        float scan = 0.91 + 0.09 * cos(gl_FragCoord.y * 3.14159265);
        vec2 centered = uv * 2.0 - 1.0;
        float vignette = max(0.72, 1.0 - 0.12 * dot(centered, centered));
        pixel.rgb *= scan * vignette;
    } else if (videoFilter == 2) {
        // Old television: warm, slightly desaturated phosphor and scan lines.
        float luma = dot(pixel.rgb, vec3(0.299, 0.587, 0.114));
        vec3 warm = vec3(luma * 1.05, luma * 0.99, luma * 0.88);
        pixel.rgb = mix(pixel.rgb, warm, 0.42);
        pixel.rgb *= 0.94 + 0.06 * cos(gl_FragCoord.y * 3.14159265);
    } else if (videoFilter == 3) {
        // Composite: a small horizontal chroma bleed, retaining most luminance detail.
        vec3 leftColor = texture(tex, uv - vec2(texel.x, 0.0)).rgb;
        vec3 rightColor = texture(tex, uv + vec2(texel.x, 0.0)).rgb;
        vec3 bleed = (leftColor + rightColor) * 0.5;
        pixel.rgb = mix(pixel.rgb, bleed, 0.18);
        pixel.r = mix(pixel.r, texture(tex, uv + vec2(texel.x * 0.75, 0.0)).r, 0.08);
        pixel.b = mix(pixel.b, texture(tex, uv - vec2(texel.x * 0.75, 0.0)).b, 0.08);
    } else if (videoFilter == 4) {
        // RGB mask: a repeating triad of sub-pixels.
        float phase = mod(floor(gl_FragCoord.x), 3.0);
        vec3 mask = vec3(0.72);
        if (phase < 1.0) mask.r = 1.0;
        else if (phase < 2.0) mask.g = 1.0;
        else mask.b = 1.0;
        pixel.rgb *= mask;
    } else if (videoFilter == 5) {
        // VHS: chroma separation and mild horizontal tracking bands.
        float shift = texel.x * 1.35;
        vec3 chroma = vec3(
            texture(tex, uv - vec2(shift, 0.0)).r,
            pixel.g,
            texture(tex, uv + vec2(shift, 0.0)).b
        );
        float tracking = 0.94 + 0.06 * sin(gl_FragCoord.y * 0.23);
        pixel.rgb = mix(pixel.rgb, chroma, 0.55) * tracking;
    }
    gl_FragColor = tint * pixel;
}''',
    "retro shader implementations",
)
replace_once(
    quad_cpp,
    '\ttintUniform = glGetUniformLocation(shader, "tint");\n',
    '\ttintUniform = glGetUniformLocation(shader, "tint");\n\tvideoFilterUniform = glGetUniformLocation(shader, "videoFilter");\n\toutputSizeUniform = glGetUniformLocation(shader, "outputSize");\n',
    "primary shader filter uniforms",
)
replace_once(
    quad_cpp,
    '\trot90TintUniform = glGetUniformLocation(rot90shader, "tint");\n',
    '\trot90TintUniform = glGetUniformLocation(rot90shader, "tint");\n\trot90VideoFilterUniform = glGetUniformLocation(rot90shader, "videoFilter");\n\trot90OutputSizeUniform = glGetUniformLocation(rot90shader, "outputSize");\n',
    "rotated shader filter uniforms",
)
replace_once(
    quad_h,
    '\tGLint rot90TintUniform;\n',
    '\tGLint rot90TintUniform;\n\tGLint rot90VideoFilterUniform;\n\tGLint rot90OutputSizeUniform;\n',
    "rotated shader uniform fields",
)
replace_once(
    quad_cpp,
    'void GlQuadDrawer::draw(GLuint texId, bool rotate, bool swapY, const float *coords, const float *color)\n',
    'void GlQuadDrawer::draw(GLuint texId, bool rotate, bool swapY, const float *coords, const float *color, bool applyVideoFilter)\n',
    "quad draw implementation signature",
)
replace_once(
    quad_cpp,
    '\tglUniform4fv(rotate ? rot90TintUniform : tintUniform, 1, color);\n',
    '\tglUniform4fv(rotate ? rot90TintUniform : tintUniform, 1, color);\n\tGLint viewport[4] = { 0, 0, 1, 1 };\n\tglGetIntegerv(GL_VIEWPORT, viewport);\n\tglUniform1i(rotate ? rot90VideoFilterUniform : videoFilterUniform, applyVideoFilter ? config::VideoFilter.get() : 0);\n\tglUniform2f(rotate ? rot90OutputSizeUniform : outputSizeUniform, viewport[2] > 0 ? (float)viewport[2] : 1.f, viewport[3] > 0 ? (float)viewport[3] : 1.f);\n',
    "per-frame filter uniforms",
)
gldraw = CORE / "core" / "rend" / "gles" / "gldraw.cpp"
replace_once(
    gldraw,
    'if (gl.bogusBlitFramebuffer || config::Rotate90)\n',
    'if (gl.bogusBlitFramebuffer || config::Rotate90 || config::VideoFilter != 0)\n',
    "route filtered final frame through GLES shader",
)
replace_once(
    gldraw,
    'gl.quadDrawer->draw(framebuffer->getTexture(), config::Rotate90, true, vertices);',
    'gl.quadDrawer->draw(framebuffer->getTexture(), config::Rotate90, true, vertices, nullptr, config::VideoFilter != 0);',
    "apply filter to final screen output only",
)

print("Android TV shell and Flycast settings/shader patches applied.")
