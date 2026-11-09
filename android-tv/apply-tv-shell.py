#!/usr/bin/env python3
"""Apply the maintained Android TV shell to the pinned Flycast submodule."""
from pathlib import Path
import sys
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[1]
CORE = ROOT / "third_party" / "flycast"
ANDROID_NS = "http://schemas.android.com/apk/res/android"
ET.register_namespace("android", ANDROID_NS)
ET.register_namespace("tools", "http://schemas.android.com/tools")
A = "{" + ANDROID_NS + "}"

source = ROOT / "android-tv" / "src" / "main" / "java" / "com" / "flycast" / "emulator" / "TvHomeActivity.java"
destination = CORE / "shell" / "android-studio" / "flycast" / "src" / "main" / "java" / "com" / "flycast" / "emulator" / "TvHomeActivity.java"
manifest = CORE / "shell" / "android-studio" / "flycast" / "src" / "main" / "AndroidManifest.xml"

for path in (source, manifest):
    if not path.is_file():
        raise SystemExit("Required Android source not found: " + str(path))

destination.parent.mkdir(parents=True, exist_ok=True)
destination.write_text(source.read_text(encoding="utf-8"), encoding="utf-8")

tree = ET.parse(manifest)
root = tree.getroot()
application = root.find("application")
if application is None:
    raise SystemExit("Flycast AndroidManifest.xml has no application element")

application.set(A + "label", "Dreamcast TV")

activity_name = "com.flycast.emulator.TvHomeActivity"
activity = None
for child in application.findall("activity"):
    if child.get(A + "name") in (activity_name, ".TvHomeActivity"):
        activity = child
        break

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

aliases = application.findall("activity-alias")
launcher_alias = next(
    (alias for alias in aliases
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
print("Android TV shell applied to the Flycast Android module.")
