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

# This Android TV target is deliberately GLES2/OpenGL-only. Upstream's Android
# JNI source list compiles the Vulkan frame-pacing bridge unconditionally; that
# bridge includes Vulkan-Hpp even when USE_VULKAN=OFF. Exclude it from this build.
android_jni_cmake = CORE / "shell" / "android-studio" / "flycast" / "src" / "main" / "jni" / "src" / "CMakeLists.txt"
replace_once(
    android_jni_cmake,
    '\t\t\tswappygl.cpp\n\t\t\tswappyvk.cpp)',
    '\t\t\tswappygl.cpp)',
    "exclude Vulkan-only Android frame-pacing bridge from GLES2 build",
)

# The RK3528 Mali-450 target is OpenGL ES 2.0 class. Upstream's generic
# Android CMake default selects the ES3 shader path; build an explicit ES2
# renderer and ES2 ImGui backend so the app can run on this target.
cmake_root = CORE / "CMakeLists.txt"
replace_once(
    cmake_root,
    'if(ANDROID)\n\t\ttarget_compile_definitions(${PROJECT_NAME} PRIVATE GLES GLES3)\n\t\tadd_subdirectory(shell/android-studio/flycast/src/main/jni/src)',
    '''if(ANDROID)
        if(USE_GLES2)
            target_compile_definitions(${PROJECT_NAME} PRIVATE GLES GLES2 IMGUI_IMPL_OPENGL_ES2)
        else()
            target_compile_definitions(${PROJECT_NAME} PRIVATE GLES GLES3)
        endif()
        add_subdirectory(shell/android-studio/flycast/src/main/jni/src)''',
    "select GLES2 for RK3528 in the Android native target",
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

# Keep Flycast's upstream presentation shader unchanged.
# The custom post-process shader previously added here failed on some GLES2
# drivers during GlQuadDrawer initialization. Retro filters stay hidden until
# a separately validated GLES2 implementation is available.

print("Android TV shell applied; upstream GLES2 shaders preserved.")
