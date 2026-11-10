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

# Undo the forced Per-Strip workaround. Flycast documents Per-Strip as faster
# but less reliable for geometry ordering; let its default Per-Triangle mode run.
replace_once(
    destination,
    'putIniValue(lines, "config", "rend.PerStripSorting", "yes");',
    'removeIniValue(lines, "config", "rend.PerStripSorting");',
    "restore Flycast default Per-Triangle polygon sorting",
)
replace_once(
    destination,
    '"Включён режим Per Strip вместо Per Triangle: он меняет порядок отрисовки прозрачных полигонов и может убрать сильные наложения и растяжения моделей на Mali-450. Это совместимый компромисс; точная сортировка Per Pixel требует OpenGL ES 3.2 и на GLES2 недоступна.");',
    '"Возвращён штатный режим Per Triangle. Принудительный Per Strip отключён: на графике Mali он может нарушать геометрию моделей.");',
    "correct Russian graphics explanation",
)

# Register already-connected and newly-connected Android HID/Bluetooth gamepads.
# Flycast's BaseGLActivity already forwards key and joystick-axis events to this
# manager; the missing step on some Android TV builds is creating the native
# Maple device before the first event arrives (and refreshing hot-plugged devices).
input_manager_java = CORE / "shell" / "android-studio" / "flycast" / "src" / "main" / "java" / "com" / "flycast" / "emulator" / "periph" / "InputDeviceManager.java"
replace_once(
    input_manager_java,
    '''        inputManager = (InputManager)applicationContext.getSystemService(Context.INPUT_SERVICE);
        inputManager.registerInputDeviceListener(this, null);''',
    '''        inputManager = (InputManager)applicationContext.getSystemService(Context.INPUT_SERVICE);
        inputManager.registerInputDeviceListener(this, null);
        // Pairing may happen before Flycast starts. Register every currently
        // attached HID gamepad/joystick immediately instead of waiting for a
        // device-added callback that will never arrive for an existing device.
        for (int deviceId : inputManager.getInputDeviceIds())
            createDevice(deviceId);''',
    "register already-connected Android game controllers",
)
replace_once(
    input_manager_java,
    '''    @Override
    public void onInputDeviceAdded(int i) {
    }

    @Override
    public void onInputDeviceRemoved(int i) {''',
    '''    @Override
    public void onInputDeviceAdded(int i) {
        createDevice(i);
    }

    @Override
    public void onInputDeviceRemoved(int i) {''',
    "register Bluetooth controllers when hot-plugged",
)
replace_once(
    input_manager_java,
    '''    @Override
    public void onInputDeviceChanged(int i) {
    }''',
    '''    @Override
    public void onInputDeviceChanged(int i) {
        if (knownDevices.remove(i))
            joystickRemoved(i);
        createDevice(i);
    }''',
    "refresh changed or re-paired Bluetooth controllers",
)



replace_once(
    input_manager_java,
    '''        if (device == null || (device.getSources() & InputDevice.SOURCE_CLASS_BUTTON) != InputDevice.SOURCE_CLASS_BUTTON)
            return false;''',
    '''        if (device == null
                || ((device.getSources() & InputDevice.SOURCE_CLASS_BUTTON) != InputDevice.SOURCE_CLASS_BUTTON
                && (device.getSources() & InputDevice.SOURCE_CLASS_JOYSTICK) != InputDevice.SOURCE_CLASS_JOYSTICK))
            return false;''',
    "accept both button-only and joystick-class Android HID controllers",
)


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

# Preserve the exact upstream Android renderer and native build configuration.
# The first device-tested APK used Flycast's unmodified Android GLES path.
# Do not force GLES2, disable Vulkan, or reload emu.cfg through a custom JNI hook
# until each change has been independently verified on the RK3528 box.

print("Android TV shell applied; upstream Flycast renderer configuration preserved.")
