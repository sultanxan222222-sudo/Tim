#!/usr/bin/env python3
"""Build the same Russian Dreamcast shell with the Android GLES3 + Vulkan engine."""
from pathlib import Path
import subprocess
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "third_party" / "flycast"

# Apply the shared Russian launcher, library, safe settings and controller fixes.
subprocess.run(
    [sys.executable, str(ROOT / "android-tv" / "apply-tv-shell.py")],
    check=True,
)

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

# The TV image is intentionally GLES2-only for Mali-450. This phone image
# instead keeps Flycast's GLES3 renderer and Vulkan backend available.
android_jni_cmake = CORE / "shell" / "android-studio" / "flycast" / "src" / "main" / "jni" / "src" / "CMakeLists.txt"
replace_once(
    android_jni_cmake,
    "\t\t\tswappygl.cpp)",
    "\t\t\tswappygl.cpp\n\t\t\tswappyvk.cpp)",
    "restore Android Vulkan frame-pacing bridge",
)

cmake_root = CORE / "CMakeLists.txt"
replace_once(
    cmake_root,
    """        if(USE_GLES2)
            target_compile_definitions(${PROJECT_NAME} PRIVATE GLES GLES2 IMGUI_IMPL_OPENGL_ES2)
        else()
            target_compile_definitions(${PROJECT_NAME} PRIVATE GLES GLES3)
        endif()
""",
    """        target_compile_definitions(${PROJECT_NAME} PRIVATE GLES GLES3)
""",
    "select GLES3 for the Android phone target",
)

gradle_file = CORE / "shell" / "android-studio" / "flycast" / "build.gradle.kts"
gradle_content = gradle_file.read_text(encoding="utf-8")
for line in (
    '                arguments += "-DUSE_GLES2=ON"\n',
    '                arguments += "-DUSE_OPENGL=ON"\n',
    '                arguments += "-DUSE_VULKAN=OFF"\n',
):
    gradle_content = gradle_content.replace(line, "")
gradle_file.write_text(gradle_content, encoding="utf-8")

# Make the launcher visible in ordinary Android phone app drawers as well as TV launchers.
manifest = CORE / "shell" / "android-studio" / "flycast" / "src" / "main" / "AndroidManifest.xml"
ANDROID_NS = "http://schemas.android.com/apk/res/android"
ET.register_namespace("android", ANDROID_NS)
A = "{" + ANDROID_NS + "}"
tree = ET.parse(manifest)
root = tree.getroot()
application = root.find("application")
if application is None:
    raise SystemExit("Flycast AndroidManifest.xml has no application element")
application.set(A + "label", "Dreamcast")
for alias in application.findall("activity-alias"):
    for intent_filter in alias.findall("intent-filter"):
        categories = {c.get(A + "name") for c in intent_filter.findall("category")}
        if "android.intent.category.LEANBACK_LAUNCHER" in categories:
            if "android.intent.category.LAUNCHER" not in categories:
                ET.SubElement(intent_filter, "category", {A + "name": "android.intent.category.LAUNCHER"})
ET.indent(tree, space="    ")
tree.write(manifest, encoding="utf-8", xml_declaration=True)

print("Android phone shell applied: GLES3 + Vulkan enabled; GLES2 TV build remains unchanged.")
