#!/usr/bin/env python3
"""Apply WHAT'S NEXT lifecycle gating to a Resonance-patched Flycast tree.

Run after Resonance's patches/flycast-ps4.patch. Every source edit is guarded by
an exact-match check so upstream drift fails loudly instead of half-applying.
"""
from __future__ import annotations

import re
import shutil
import sys
from pathlib import Path


def replace_once(path: Path, old: str, new: str) -> None:
    data = path.read_text(encoding="utf-8")
    count = data.count(old)
    if count != 1:
        raise SystemExit(f"Expected one anchor in {path}, found {count}: {old[:100]!r}")
    path.write_text(data.replace(old, new, 1), encoding="utf-8")


def main() -> None:
    if len(sys.argv) != 2:
        raise SystemExit(f"Usage: {sys.argv[0]} /path/to/flycast-after-resonance-patch")

    repo = Path(__file__).resolve().parents[1]
    root = Path(sys.argv[1]).resolve()
    context_cpp = root / "core/rend/vulkan/vulkan_context.cpp"
    context_h = root / "core/rend/vulkan/vulkan_context.h"
    cmake = root / "core/rend/vulkan/CMakeLists.txt"
    if not all(p.is_file() for p in (context_cpp, context_h, cmake, root / "core/rend/vulkan/frame_gen.cpp")):
        raise SystemExit("Not a Resonance-patched Flycast tree: required Vulkan renderer files are missing.")

    module = repo / "dreamcast/mini-dlss"
    target = root / "core/rend/vulkan/whats_next"
    shutil.copytree(module / "include", target / "include", dirs_exist_ok=True)
    for name in ("history_gate.cpp", "frame_generation_policy.cpp",
                 "frame_generation_controller.cpp", "frame_generation_bridge.cpp"):
        shutil.copy2(module / "src" / name, target / "src" / name) if (target / "src").mkdir(parents=True, exist_ok=True) is None else None

    replace_once(
        cmake,
        'target_compile_definitions(${PROJECT_NAME} PRIVATE USE_VULKAN HAVE_VULKAN)\n',
        'target_compile_definitions(${PROJECT_NAME} PRIVATE USE_VULKAN HAVE_VULKAN)\n'
        'target_include_directories(${PROJECT_NAME} PRIVATE "\${CMAKE_CURRENT_SOURCE_DIR}/whats_next/include")\n'
    )
    replace_once(
        cmake,
        '            frame_gen.cpp\n',
        '            frame_gen.cpp\n'
        '            whats_next/src/history_gate.cpp\n'
        '            whats_next/src/frame_generation_policy.cpp\n'
        '            whats_next/src/frame_generation_controller.cpp\n'
        '            whats_next/src/frame_generation_bridge.cpp\n'
    )
    replace_once(
        context_cpp,
        '#include "frame_gen.h"\n',
        '#include "frame_gen.h"\n#include "whats_next/frame_generation_bridge.hpp"\n'
    )
    replace_once(
        context_h,
        '#include "vulkan.h"\n',
        '#include "vulkan.h"\n#include "whats_next/frame_generation_bridge.hpp"\n'
    )

    replace_once(
        context_cpp,
        '#ifdef __ORBIS__\n\t// a new frame from the renderer',
        '#ifdef __ORBIS__\n\tbool sourceAccepted = false;\n\t// a new frame from the renderer'
    )

    bridge_gate = '''&& fgPrevExtent == extent;
\t\tconst bool eligible = interpolate;
\t\tif ((extent.width != fgPrevExtent.width || extent.height != fgPrevExtent.height) && fgPrevExtent.width != 0)
\t\t{
\t\t\t++fgResourceGeneration;
\t\t\twhatsNextBridge.reset();
\t\t}
\t\tconst auto decision = whatsNextBridge.begin_real_frame(
\t\t\t\teligible, extent.width, extent.height, whats_next::PixelFormat::RGBA8,
\t\t\t\tfgResourceGeneration, fgGameSession, whats_next::PresentationCadence{
\t\t\t\t\t(gap > 2 ? 3U : static_cast<std::uint32_t>(gap))});
\t\tinterpolate = eligible && decision == whats_next::FrameGenerationDecision::GenerateIntermediate;'''
    replace_once(context_cpp, 'fgPrevExtent == extent;', 'fgPrevExtent == extent;\\n' + bridge_gate.split('\\n').slice(1).join('\\n'))

    replace_once(
        context_cpp,
        '\tif (gui_is_open())\n\t\tfgPrevView = vk::ImageView();',
        '\tif (gui_is_open())\n\t{\n\t\tif (!fgWasGuiOpen)\n\t\t{\n\t\t\t++fgGameSession;\n\t\t\twhatsNextBridge.reset();\n\t\t}\n\t\tfgPrevView = vk::ImageView();\n\t}\n\tfgWasGuiOpen = gui_is_open();'
    )
    replace_once(
        context_cpp,
        '\t\t\tfgDraw = false;\n',
        '\t\t\tfgDraw = false;\n#ifdef __ORBIS__\n\t\t\tif (sourceAccepted)\n\t\t\t\twhatsNextBridge.commit_real_frame();\n#endif\n'
    )
    replace_once(
        context_h,
        '\tbool fgRealPending = false;\n',
        '\tbool fgRealPending = false;\n\twhats_next::FrameGenerationBridge whatsNextBridge;\n'
        '\tu64 fgResourceGeneration = 1;\n\tu64 fgGameSession = 1;\n\tbool fgWasGuiOpen = false;\n'
    )
    print(f"WHAT'S NEXT lifecycle bridge integrated into {root}")


if __name__ == "__main__":
    main()
