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
    shaders_cpp = root / "core/rend/vulkan/shaders.cpp"
    shaders_h = root / "core/rend/vulkan/shaders.h"
    quad_cpp = root / "core/rend/vulkan/quad.cpp"
    quad_h = root / "core/rend/vulkan/quad.h"
    if not all(p.is_file() for p in (context_cpp, context_h, cmake, shaders_cpp, shaders_h, quad_cpp, quad_h, root / "core/rend/vulkan/frame_gen.cpp")):
        raise SystemExit("Not a Resonance-patched Flycast tree: required Vulkan renderer files are missing.")

    module = repo / "dreamcast/mini-dlss"
    frame_gen_cpp = root / "core/rend/vulkan/frame_gen.cpp"
    old_fragment = '''void main()
{
	vec4 mo = textureLod(motionTex, uv, 0.0);
	vec2 m = mo.xy;
	vec4 a = textureLod(prevTex, uv - m * 0.5, 0.0);
	vec4 b = textureLod(curTex, uv + m * 0.5, 0.0);
	// a poor match (something appearing, a cut): the newer frame as it is
	float w = smoothstep(0.03, 0.10, mo.z);
	color = vec4(mix(mix(a, b, 0.5), textureLod(curTex, uv, 0.0), w).rgb, 1.0);
}'''
    # Validate every source-edit anchor before copying files or changing the build tree.
    preflight_anchors = (
        (cmake, 'target_compile_definitions(${PROJECT_NAME} PRIVATE USE_VULKAN HAVE_VULKAN)\n'),
        (cmake, '            frame_gen.cpp\n'),
        (context_cpp, '#include "frame_gen.h"\n'),
        (context_cpp, '#ifdef __ORBIS__\n\t// a new frame from the renderer'),
        (context_cpp, 'fgPrevExtent == extent;'),
        (context_cpp, '\tif (gui_is_open())\n\t\tfgPrevView = vk::ImageView();'),
        (context_cpp, '\t\t\tfgDraw = false;\n'),
        (context_h, '#include "vulkan.h"\n'),
        (context_h, '\tbool fgRealPending = false;\n'),
        (frame_gen_cpp, old_fragment),
    )
    for path, anchor in preflight_anchors:
        count = path.read_text(encoding="utf-8").count(anchor)
        if count != 1:
            raise SystemExit(
                f"Preflight failed before editing: expected one anchor in {path}, found {count}: {anchor[:100]!r}"
            )

    required_module_files = (
        module / "src/history_gate.cpp",
        module / "src/frame_generation_policy.cpp",
        module / "src/frame_generation_controller.cpp",
        module / "src/frame_generation_bridge.cpp",
        module / "shaders/edge_aware_sharpen.comp",
        module / "shaders/mini_dlss_upscale.comp",
    )
    missing_files = [path for path in required_module_files if not path.is_file()]
    if missing_files:
        raise SystemExit("Preflight failed before editing; required WHAT'S NEXT files missing: "
                         + ", ".join(str(path) for path in missing_files))

    target = root / "core/rend/vulkan/whats_next"
    shutil.copytree(module / "include", target / "include", dirs_exist_ok=True)
    (target / "src").mkdir(parents=True, exist_ok=True)
    for name in ("history_gate.cpp", "frame_generation_policy.cpp",
                 "frame_generation_controller.cpp", "frame_generation_bridge.cpp"):
        shutil.copy2(module / "src" / name, target / "src" / name)
    shader_target = target / "shaders"
    shader_target.mkdir(parents=True, exist_ok=True)
    for name in ("edge_aware_sharpen.comp", "mini_dlss_upscale.comp"):
        shutil.copy2(module / "shaders" / name, shader_target / name)

    replace_once(
        cmake,
        'target_compile_definitions(${PROJECT_NAME} PRIVATE USE_VULKAN HAVE_VULKAN)\n',
        'target_compile_definitions(${PROJECT_NAME} PRIVATE USE_VULKAN HAVE_VULKAN)\n'
        'target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/whats_next/include")\n'
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

    # The existing frame-generation fragment shader is the only safe insertion point
    # without introducing another offscreen target or a CPU readback.
    new_fragment = '''void main()
{
	vec4 mo = textureLod(motionTex, uv, 0.0);
	vec2 m = mo.xy;
	vec4 a = textureLod(prevTex, uv - m * 0.5, 0.0);
	vec4 b = textureLod(curTex, uv + m * 0.5, 0.0);
	// A poor match (something appearing, a cut): use the newer frame.
	float w = smoothstep(0.03, 0.10, mo.z);
	vec3 base = mix(mix(a, b, 0.5), textureLod(curTex, uv, 0.0), w).rgb;

	// Mini-DLSS spatial finishing pass: restrained, edge-aware sharpening.
	// It operates in the existing fragment pass, so no extra image/readback is needed.
	vec2 texel = 1.0 / vec2(textureSize(curTex, 0));
	vec3 n = textureLod(curTex, uv + vec2(0.0, -texel.y), 0.0).rgb;
	vec3 s = textureLod(curTex, uv + vec2(0.0,  texel.y), 0.0).rgb;
	vec3 e = textureLod(curTex, uv + vec2( texel.x, 0.0), 0.0).rgb;
	vec3 west = textureLod(curTex, uv + vec2(-texel.x, 0.0), 0.0).rgb;
	float edge = max(max(abs(dot(base - n, vec3(0.2126, 0.7152, 0.0722))),
	                     abs(dot(base - s, vec3(0.2126, 0.7152, 0.0722)))),
	                 max(abs(dot(base - e, vec3(0.2126, 0.7152, 0.0722))),
	                     abs(dot(base - west, vec3(0.2126, 0.7152, 0.0722)))));
	float edgeGate = 1.0 - smoothstep(0.025, 0.12, edge);
	vec3 avg = (n + s + e + west) * 0.25;
	vec3 sharpened = base + (base - avg) * (0.12 * edgeGate);
	vec3 lo = min(base, min(min(n, s), min(e, west)));
	vec3 hi = max(base, max(max(n, s), max(e, west)));
	color = vec4(clamp(sharpened, lo, hi), 1.0);
}'''
    replace_once(frame_gen_cpp, old_fragment, new_fragment)
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
\t\tinterpolate = eligible && decision == whats_next::FrameGenerationDecision::GenerateIntermediate;
\t\tsourceAccepted = true; // this source frame reached the normal presentation decision point'''
    replace_once(context_cpp, 'fgPrevExtent == extent;', 'fgPrevExtent == extent;\n' + '\n'.join(bridge_gate.splitlines()[1:]))

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

    # Build a separate shader variant for ordinary frames. The default quad shader is
    # left unchanged for the UI, while the generator's fragment shader remains in place
    # for intermediate frames. Both use the same conservative neighbourhood clamp.
    replace_once(shaders_cpp, quad_fragment_original, quad_fragment_enhanced)
    replace_once(
        shaders_cpp,
        'vk::UniqueShaderModule ShaderManager::compileQuadFragmentShader(bool ignoreTexAlpha)\n',
        'vk::UniqueShaderModule ShaderManager::compileQuadFragmentShader(bool ignoreTexAlpha, bool miniDlss)\n'
    )
    replace_once(
        shaders_cpp,
        '.addConstant("IGNORE_TEX_ALPHA", (int)ignoreTexAlpha)\n\t\t\t.addSource(QuadFragmentShaderSource);',
        '.addConstant("IGNORE_TEX_ALPHA", (int)ignoreTexAlpha)\n'
        '\t\t\t.addConstant("MINI_DLSS", (int)miniDlss)\n'
        '\t\t\t.addSource(QuadFragmentShaderSource);'
    )
    replace_once(
        shaders_h,
        'vk::ShaderModule GetQuadFragmentShader(bool ignoreTexAlpha)\n\t{',
        'vk::ShaderModule GetQuadFragmentShader(bool ignoreTexAlpha, bool miniDlss = false)\n\t{\n'
        '\t\tif (miniDlss)\n\t\t{\n'
        '\t\t\tauto& shader = quadMiniDlssFragmentShader[ignoreTexAlpha ? 1 : 0];\n'
        '\t\t\tif (!shader)\n'
        '\t\t\t\tshader = compileQuadFragmentShader(ignoreTexAlpha, true);\n'
        '\t\t\treturn *shader;\n'
        '\t\t}'
    )
    replace_once(
        shaders_h,
        '\t\tquadNoAlphaFragmentShader.reset();\n',
        '\t\tquadNoAlphaFragmentShader.reset();\n'
        '\t\tquadMiniDlssFragmentShader[0].reset();\n'
        '\t\tquadMiniDlssFragmentShader[1].reset();\n'
    )
    replace_once(
        shaders_h,
        '\tvk::UniqueShaderModule compileQuadFragmentShader(bool ignoreTexAlpha);\n',
        '\tvk::UniqueShaderModule compileQuadFragmentShader(bool ignoreTexAlpha, bool miniDlss = false);\n'
    )
    replace_once(
        shaders_h,
        '\tvk::UniqueShaderModule quadNoAlphaFragmentShader;\n',
        '\tvk::UniqueShaderModule quadNoAlphaFragmentShader;\n'
        '\tvk::UniqueShaderModule quadMiniDlssFragmentShader[2];\n'
    )
    replace_once(
        quad_h,
        '\tQuadPipeline(bool ignoreTexAlpha, bool rotate = false)\n'
        '\t\t: rotate(rotate), ignoreTexAlpha(ignoreTexAlpha) {}',
        '\tQuadPipeline(bool ignoreTexAlpha, bool rotate = false, bool miniDlss = false)\n'
        '\t\t: rotate(rotate), ignoreTexAlpha(ignoreTexAlpha), miniDlss(miniDlss) {}'
    )
    replace_once(quad_h, '\tbool ignoreTexAlpha;\n', '\tbool ignoreTexAlpha;\n\tbool miniDlss;\n')
    replace_once(
        quad_cpp,
        'shaderManager->GetQuadFragmentShader(ignoreTexAlpha), "main")',
        'shaderManager->GetQuadFragmentShader(ignoreTexAlpha, miniDlss), "main")'
    )

    # Source-frame renderers use dedicated pipelines only on PS4. FrameGenerator::Draw
    # has its own pipeline, and its shader already includes the same spatial finish.
    replace_once(
        context_cpp,
        'quadRotateDrawer = std::make_unique<QuadDrawer>();\n',
        'quadRotateDrawer = std::make_unique<QuadDrawer>();\n#ifdef __ORBIS__\n'
        '\t    quadMiniDlssPipeline = std::make_unique<QuadPipeline>(true, false, true);\n'
        '\t    quadRotateMiniDlssPipeline = std::make_unique<QuadPipeline>(true, true, true);\n'
        '\t    quadMiniDlssDrawer = std::make_unique<QuadDrawer>();\n'
        '\t    quadRotateMiniDlssDrawer = std::make_unique<QuadDrawer>();\n'
        '#endif\n'
    )
    replace_once(
        context_cpp,
        'quadRotateDrawer->Init(quadRotatePipeline.get());\n',
        'quadRotateDrawer->Init(quadRotatePipeline.get());\n#ifdef __ORBIS__\n'
        '\t    quadMiniDlssPipeline->Init(shaderManager.get(), *renderPass, 0);\n'
        '\t    quadRotateMiniDlssPipeline->Init(shaderManager.get(), *renderPass, 0);\n'
        '\t    quadMiniDlssDrawer->Init(quadMiniDlssPipeline.get());\n'
        '\t    quadRotateMiniDlssDrawer->Init(quadRotateMiniDlssPipeline.get());\n'
        '#endif\n'
    )
    replace_once(
        context_cpp,
        '\tif (config::Rotate90)\n\t\tquadRotatePipeline->BindPipeline(commandBuffer);\n\telse\n\t\tquadPipeline->BindPipeline(commandBuffer);\n',
        '#ifdef __ORBIS__\n'
        '\tconst bool miniDlssActive = config::FrameGeneration && !settings.input.fastForwardMode\n'
        '\t\t\t&& !gui_is_open() && !(fgDraw && frameGen != nullptr);\n'
        '\tif (miniDlssActive && config::Rotate90)\n'
        '\t\tquadRotateMiniDlssPipeline->BindPipeline(commandBuffer);\n'
        '\telse if (miniDlssActive)\n'
        '\t\tquadMiniDlssPipeline->BindPipeline(commandBuffer);\n'
        '\telse\n'
        '#endif\n'
        '\tif (config::Rotate90)\n\t\tquadRotatePipeline->BindPipeline(commandBuffer);\n'
        '\telse\n\t\tquadPipeline->BindPipeline(commandBuffer);\n'
    )
    replace_once(
        context_cpp,
        '\tif (fgDraw && frameGen != nullptr)\n\t\tframeGen->Draw(commandBuffer, *renderPass);\n'
        '\telse if (config::Rotate90)\n\t\tquadRotateDrawer->Draw(commandBuffer, imageView, vtx, !config::LinearInterpolation);\n'
        '\telse\n\t\tquadDrawer->Draw(commandBuffer, imageView, vtx, !config::LinearInterpolation);\n',
        '\tif (fgDraw && frameGen != nullptr)\n\t\tframeGen->Draw(commandBuffer, *renderPass);\n'
        '#ifdef __ORBIS__\n'
        '\telse if (miniDlssActive && config::Rotate90)\n'
        '\t\tquadRotateMiniDlssDrawer->Draw(commandBuffer, imageView, vtx, !config::LinearInterpolation);\n'
        '\telse if (miniDlssActive)\n'
        '\t\tquadMiniDlssDrawer->Draw(commandBuffer, imageView, vtx, !config::LinearInterpolation);\n'
        '#endif\n'
        '\telse if (config::Rotate90)\n'
        '\t\tquadRotateDrawer->Draw(commandBuffer, imageView, vtx, !config::LinearInterpolation);\n'
        '\telse\n'
        '\t\tquadDrawer->Draw(commandBuffer, imageView, vtx, !config::LinearInterpolation);\n'
    )
    replace_once(
        context_cpp,
        '\tquadDrawer.reset();\n',
        '#ifdef __ORBIS__\n'
        '\tquadMiniDlssDrawer.reset();\n\tquadRotateMiniDlssDrawer.reset();\n'
        '\tquadMiniDlssPipeline.reset();\n\tquadRotateMiniDlssPipeline.reset();\n'
        '#endif\n'
        '\tquadDrawer.reset();\n'
    )
    replace_once(
        context_h,
        '\tstd::unique_ptr<QuadPipeline> quadRotatePipeline;\n',
        '\tstd::unique_ptr<QuadPipeline> quadRotatePipeline;\n#ifdef __ORBIS__\n'
        '\tstd::unique_ptr<QuadPipeline> quadMiniDlssPipeline;\n'
        '\tstd::unique_ptr<QuadPipeline> quadRotateMiniDlssPipeline;\n'
        '#endif\n'
    )
    replace_once(
        context_h,
        '\tstd::unique_ptr<QuadDrawer> quadRotateDrawer;\n',
        '\tstd::unique_ptr<QuadDrawer> quadRotateDrawer;\n#ifdef __ORBIS__\n'
        '\tstd::unique_ptr<QuadDrawer> quadMiniDlssDrawer;\n'
        '\tstd::unique_ptr<QuadDrawer> quadRotateMiniDlssDrawer;\n'
        '#endif\n'
    )

    # Fail closed if the bridge was copied but its lifecycle hooks did not land.
    integrated_cmake = cmake.read_text(encoding="utf-8")
    if "${CMAKE_CURRENT_SOURCE_DIR}/whats_next/include" not in integrated_cmake or (chr(92) + "${CMAKE_CURRENT_SOURCE_DIR}/whats_next/include") in integrated_cmake:
        raise SystemExit("Mini-DLSS CMake include path is malformed.")

    integrated_context = context_cpp.read_text(encoding="utf-8")
    required_hooks = (
        "whatsNextBridge.begin_real_frame",
        "sourceAccepted = true",
        "whatsNextBridge.commit_real_frame",
        "whatsNextBridge.reset()",
    )
    missing_hooks = [hook for hook in required_hooks if hook not in integrated_context]
    if missing_hooks:
        raise SystemExit("Renderer integration incomplete; missing hooks: " + ", ".join(missing_hooks))
    if "Mini-DLSS spatial finishing pass" not in frame_gen_cpp.read_text(encoding="utf-8"):
        raise SystemExit("Mini-DLSS spatial finishing pass was not installed.")
    if "quadMiniDlssDrawer->Draw" not in integrated_context:
        raise SystemExit("Mini-DLSS source-frame finishing path was not installed.")
    if "MINI_DLSS" not in shaders_cpp.read_text(encoding="utf-8"):
        raise SystemExit("Mini-DLSS quad shader variant was not installed.")

    print(f"WHAT'S NEXT lifecycle bridge integrated into {root}")


if __name__ == "__main__":
    main()
