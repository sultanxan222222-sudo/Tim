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
    frame_gen_h = root / "core/rend/vulkan/frame_gen.h"
    option_cpp = root / "core/cfg/option.cpp"
    option_h = root / "core/cfg/option.h"
    settings_menu_cpp = root / "shell/ps4/launcher/settings_menu.cpp"
    quad_h = root / "core/rend/vulkan/quad.h"
    if not all(p.is_file() for p in (context_cpp, context_h, cmake, shaders_cpp, shaders_h, quad_cpp, quad_h, frame_gen_h, option_cpp, option_h, settings_menu_cpp, root / "core/rend/vulkan/frame_gen.cpp")):
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
    # Mini-DLSS variant of Flycast's ordinary presentation quad. It preserves the
    # existing alpha/colour logic when MINI_DLSS=0. When enabled, it performs
    # edge-adaptive spatial reconstruction followed by conservative sharpening.
    # This is spatial filtering, not neural DLSS. The generated-frame fragment has its corresponding pass above.
    quad_fragment_original = '''#if IGNORE_TEX_ALPHA == 1
\tFragColor.rgb = pushConstants.color.rgb * texture(tex, inUV).rgb;
\tFragColor.a = pushConstants.color.a;
#else
\tFragColor = pushConstants.color * texture(tex, inUV);
#endif'''
    quad_fragment_enhanced = '''vec4 sourceColor = texture(tex, inUV);
#if MINI_DLSS == 1
\t// Edge-adaptive spatial reconstruction for the low-resolution game image.
\t// Sample source texels at their centres and interpolate more cautiously
\t// across strong edges before the finishing sharpen.
\tvec2 sourceSize = vec2(textureSize(tex, 0));
\tvec2 sourcePixel = inUV * sourceSize - vec2(0.5);
\tvec2 sourceBase = floor(sourcePixel);
\tvec2 sourceFrac = fract(sourcePixel);
\tvec2 uv00 = (sourceBase + vec2(0.5, 0.5)) / sourceSize;
\tvec2 uv10 = (sourceBase + vec2(1.5, 0.5)) / sourceSize;
\tvec2 uv01 = (sourceBase + vec2(0.5, 1.5)) / sourceSize;
\tvec2 uv11 = (sourceBase + vec2(1.5, 1.5)) / sourceSize;
\tvec3 c00 = textureLod(tex, uv00, 0.0).rgb;
\tvec3 c10 = textureLod(tex, uv10, 0.0).rgb;
\tvec3 c01 = textureLod(tex, uv01, 0.0).rgb;
\tvec3 c11 = textureLod(tex, uv11, 0.0).rgb;
\tfloat gx = abs(dot(c00 - c10, vec3(0.2126, 0.7152, 0.0722))) + abs(dot(c01 - c11, vec3(0.2126, 0.7152, 0.0722)));
\tfloat gy = abs(dot(c00 - c01, vec3(0.2126, 0.7152, 0.0722))) + abs(dot(c10 - c11, vec3(0.2126, 0.7152, 0.0722)));
\tfloat fx = sourceFrac.x;
\tfloat fy = sourceFrac.y;
\tif (gx > gy * 1.5) fx = (fx < 0.5) ? 0.0 : 1.0;
\tif (gy > gx * 1.5) fy = (fy < 0.5) ? 0.0 : 1.0;
\tvec3 top = mix(c00, c10, fx);
\tvec3 bottom = mix(c01, c11, fx);
\tsourceColor.rgb = mix(top, bottom, fy);
\tvec2 texel = 1.0 / sourceSize;
\tvec3 base = sourceColor.rgb;
\tvec3 n = texture(tex, inUV + vec2(0.0, -texel.y)).rgb;
\tvec3 s = texture(tex, inUV + vec2(0.0,  texel.y)).rgb;
\tvec3 e = texture(tex, inUV + vec2( texel.x, 0.0)).rgb;
\tvec3 west = texture(tex, inUV + vec2(-texel.x, 0.0)).rgb;
\tfloat lumaB = dot(base, vec3(0.2126, 0.7152, 0.0722));
\tfloat edge = max(
\t\tmax(abs(lumaB - dot(n, vec3(0.2126, 0.7152, 0.0722))),
\t\t    abs(lumaB - dot(s, vec3(0.2126, 0.7152, 0.0722)))),
\t\tmax(abs(lumaB - dot(e, vec3(0.2126, 0.7152, 0.0722))),
\t\t    abs(lumaB - dot(west, vec3(0.2126, 0.7152, 0.0722)))));
\tfloat edgeGate = 1.0 - smoothstep(0.025, 0.12, edge);
\tvec3 average = (n + s + e + west) * 0.25;
\tvec3 sharpened = base + (base - average) * (0.12 * edgeGate);
\tvec3 lo = min(base, min(min(n, s), min(e, west)));
\tvec3 hi = max(base, max(max(n, s), max(e, west)));
\tsourceColor.rgb = clamp(sharpened, max(lo - vec3(0.035), vec3(0.0)), min(hi + vec3(0.035), vec3(1.0)));
#endif
#if IGNORE_TEX_ALPHA == 1
\tFragColor.rgb = pushConstants.color.rgb * sourceColor.rgb;
\tFragColor.a = pushConstants.color.a;
#else
\tFragColor = pushConstants.color * sourceColor;
#endif'''


    fine_shader_old = r'''const char *const kFine = R"(
void main()
{
	ivec2 cell = ivec2(gl_GlobalInvocationID.xy);
	if (cell.x >= 160 || cell.y >= 90)
		return;
	vec2 cs = 1.0 / vec2(160.0, 90.0);
	vec2 c0 = (vec2(cell) + 0.5) * cs;
	loadCur(c0, cs);
	// the coarse neighbours' moves, and no move
	ivec2 cc = cell / 2;
	float best = sad(c0, cs, vec2(0.0));
	vec2 bestM = vec2(0.0);
	for (int b = -1; b <= 1; b++)
		for (int a = -1; a <= 1; a++)
		{
			ivec2 n = clamp(cc + ivec2(a, b), ivec2(0), ivec2(79, 44));
			vec2 m = texelFetch(coarseTex, n, 0).xy;
			float s = sad(c0, cs, m) + 0.002;
			if (s < best)
			{
				best = s;
				bestM = m;
			}
		}
	// refined by quarter cells
	vec2 base = bestM;
	vec2 stepv = cs * 0.25;
	for (int dy = -2; dy <= 2; dy++)
		for (int dx = -2; dx <= 2; dx++)
		{
			if (dx == 0 && dy == 0)
				continue;
			vec2 m = base + vec2(dx, dy) * stepv;
			float s = sad(c0, cs, m) + 0.001 * length(vec2(dx, dy));
			if (s < best)
			{
				best = s;
				bestM = m;
			}
		}
	imageStore(outImage, cell, vec4(bestM, best, 0.0));
}
)";'''
    fine_shader_new = r'''const char *const kFine = R"(
void considerMotionCandidate(
	vec2 motion, float score,
	inout vec2 bestMotion, inout float bestScore,
	inout float secondScore, inout bool hasSecond)
{
	// Repeated coarse neighbours must not inflate confidence as independent matches.
	if (length(motion - bestMotion) < 0.00001)
	{
		bestScore = min(bestScore, score);
		return;
	}
	if (score < bestScore)
	{
		secondScore = bestScore;
		hasSecond = true;
		bestScore = score;
		bestMotion = motion;
	}
	else if (!hasSecond || score < secondScore)
	{
		secondScore = score;
		hasSecond = true;
	}
}

void main()
{
	ivec2 cell = ivec2(gl_GlobalInvocationID.xy);
	if (cell.x >= 160 || cell.y >= 90)
		return;
	vec2 cs = 1.0 / vec2(160.0, 90.0);
	vec2 c0 = (vec2(cell) + 0.5) * cs;
	loadCur(c0, cs);

	// Local texture variance helps identify ambiguous motion in flat areas.
	float sum = 0.0;
	float sumSq = 0.0;
	for (int k = 0; k < 64; ++k)
	{
		sum += cur[k];
		sumSq += cur[k] * cur[k];
	}
	float mean = sum / 64.0;
	float variance = max(sumSq / 64.0 - mean * mean, 0.0);

	ivec2 cc = cell / 2;
	float best = sad(c0, cs, vec2(0.0));
	float second = 1e9;
	bool hasSecond = false;
	vec2 bestM = vec2(0.0);

	// Keep a distinct runner-up while searching the coarse neighbourhood.
	for (int b = -1; b <= 1; b++)
		for (int a = -1; a <= 1; a++)
		{
			ivec2 n = clamp(cc + ivec2(a, b), ivec2(0), ivec2(79, 44));
			vec2 m = texelFetch(coarseTex, n, 0).xy;
			float score = sad(c0, cs, m) + 0.002;
			considerMotionCandidate(m, score, bestM, best, second, hasSecond);
		}

	vec2 base = bestM;
	vec2 stepv = cs * 0.25;
	for (int dy = -2; dy <= 2; dy++)
		for (int dx = -2; dx <= 2; dx++)
		{
			if (dx == 0 && dy == 0)
				continue;
			vec2 m = base + vec2(dx, dy) * stepv;
			float score = sad(c0, cs, m) + 0.001 * length(vec2(dx, dy));
			considerMotionCandidate(m, score, bestM, best, second, hasSecond);
		}

	float matchQuality = 1.0 - smoothstep(0.025, 0.12, best);
	float uniqueness = hasSecond
		? smoothstep(0.003, 0.03, max(second - best, 0.0))
		: 0.0;
	float textureConfidence = smoothstep(0.001, 0.015, variance);
	float motionPixels = length(bestM * vec2(textureSize(curTex, 0)));
	float textureGate = mix(0.35 + 0.65 * textureConfidence,
		1.0, smoothstep(0.5, 2.0, motionPixels));
	float confidence = clamp(
		matchQuality * (0.65 + 0.35 * uniqueness) * textureGate,
		0.0, 1.0);

	// xy = motion vector, z = match residual, w = confidence (0..1).
	imageStore(outImage, cell, vec4(bestM, best, confidence));
}
)";'''

    # Validate every source-edit anchor before copying files or changing the build tree.
    preflight_anchors = (
        (cmake, 'target_compile_definitions(${PROJECT_NAME} PRIVATE USE_VULKAN HAVE_VULKAN)\n'),
        (cmake, '            frame_gen.cpp\n'),
        (context_cpp, '#include "frame_gen.h"\n'),
        (context_cpp, 'void VulkanContext::CreateSwapChain()\n{\n\ttry\n\t{'),
        (context_cpp, '#ifdef __ORBIS__\n\t// a new frame from the renderer'),
        (context_cpp, 'fgPrevExtent == extent;'),
        (context_cpp, 'EndFrame(overlayCmdBuffer);\n\t\t\tstatic_cast<BaseVulkanRenderer*>(renderer)->RenderVideoRouting();'),
        (context_cpp, '} catch (const InvalidVulkanContext&) {\n\t\t\t// Re-create swap chain\n\t\t\tresized = true;\n\t\t}'),
        (context_cpp, '\tif (gui_is_open())\n\t\tfgPrevView = vk::ImageView();'),
        (context_h, '#include "vulkan.h"\n'),
        (context_h, '\tbool fgRealPending = false;\n'),
        (frame_gen_cpp, old_fragment),
        (frame_gen_cpp, fine_shader_old),
        (shaders_cpp, quad_fragment_original),
        (shaders_cpp, 'vk::UniqueShaderModule ShaderManager::compileQuadFragmentShader(bool ignoreTexAlpha)\n'),
        (shaders_h, 'vk::ShaderModule GetQuadFragmentShader(bool ignoreTexAlpha)\n'),
        (shaders_h, '\t\tquadNoAlphaFragmentShader.reset();\n'),
        (shaders_h, '\tvk::UniqueShaderModule compileQuadFragmentShader(bool ignoreTexAlpha);\n'),
        (shaders_h, '\tvk::UniqueShaderModule quadNoAlphaFragmentShader;\n'),
        (quad_h, '\tQuadPipeline(bool ignoreTexAlpha, bool rotate = false)\n'),
        (quad_h, '\tbool ignoreTexAlpha;\n'),
        (quad_cpp, 'shaderManager->GetQuadFragmentShader(ignoreTexAlpha), "main")'),
        (context_cpp, 'quadRotateDrawer = std::make_unique<QuadDrawer>();\n'),
        (context_cpp, 'quadRotateDrawer->Init(quadRotatePipeline.get());\n'),
        (context_cpp, '\tif (config::Rotate90)\n\t\tquadRotatePipeline->BindPipeline(commandBuffer);\n\telse\n\t\tquadPipeline->BindPipeline(commandBuffer);\n'),
        (context_cpp, '\tif (fgDraw && frameGen != nullptr)\n\t\tframeGen->Draw(commandBuffer, *renderPass);\n\telse if (config::Rotate90)\n\t\tquadRotateDrawer->Draw(commandBuffer, imageView, vtx, !config::LinearInterpolation);\n\telse\n\t\tquadDrawer->Draw(commandBuffer, imageView, vtx, !config::LinearInterpolation);\n'),
        (context_cpp, '\tquadDrawer.reset();\n'),
        (frame_gen_cpp, ')";\n} // namespace'),
        (frame_gen_cpp, 'if (!drawPipeline || renderPass != drawRenderPass)'),
        (frame_gen_cpp, 'ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, kFragment)'),
        (frame_gen_cpp, '\t\tdrawRenderPass = renderPass;\n'),
        (frame_gen_h, '\tbool ready = false;\n'),
        (option_cpp, 'Option<bool> FrameGeneration("rend.FrameGeneration", false);\n'),
        (option_cpp, 'Option<int> RenderResolution("rend.Resolution", 480);'),
        (option_h, 'extern Option<bool> FrameGeneration;\t// Resonance: in-between pictures for 30 fps games\n'),
        (settings_menu_cpp, 'video.items.push_back(toggle(Tnop("Frame Generation"),'),
        (settings_menu_cpp, 'const float scales[] = { 1.f, 1.5f, 2.f, 3.f, 4.f };'),
        (settings_menu_cpp, 'config::FrameGeneration));'),
        (context_h, '\tstd::unique_ptr<QuadPipeline> quadRotatePipeline;\n'),
        (context_h, '\tstd::unique_ptr<QuadDrawer> quadRotateDrawer;\n'),
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
        module / "src/render_budget_controller.cpp",
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
                 "frame_generation_controller.cpp", "frame_generation_bridge.cpp",
                 "render_budget_controller.cpp"):
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
        '            whats_next/src/render_budget_controller.cpp\n'
    )
    replace_once(
        context_cpp,
        '#include "frame_gen.h"\n',
        '#include "frame_gen.h"\n#include "whats_next/frame_generation_bridge.hpp"\n'
    )
    replace_once(
        context_cpp,
        'void VulkanContext::CreateSwapChain()\n{\n\ttry\n\t{',
        'void VulkanContext::CreateSwapChain()\n{\n#ifdef __ORBIS__\n'
        '\t++fgResourceGeneration;\n'
        '\twhatsNextBridge.reset();\n'
        '\tfgPrevView = vk::ImageView();\n'
        '\tfgPrevExtent = vk::Extent2D();\n'
        '\tfgStreak = 0;\n'
        '#endif\n\ttry\n\t{'
    )

    replace_once(frame_gen_cpp, fine_shader_old, fine_shader_new)

    # The existing frame-generation fragment shader is the only safe insertion point
    # without introducing another offscreen target or a CPU readback.
    new_fragment = '''void main()
{
	vec4 mo = textureLod(motionTex, uv, 0.0);
	vec2 m = mo.xy;
	vec4 a = textureLod(prevTex, uv - m * 0.5, 0.0);
	vec4 b = textureLod(curTex, uv + m * 0.5, 0.0);
	vec3 realFrame = textureLod(curTex, uv, 0.0).rgb;

	// Confidence-aware rejection: ambiguous motion and warped-sample disagreement
	// are treated as possible occlusion/disocclusion, not as valid interpolation.
	float residualReject = smoothstep(0.035, 0.10, mo.z);
	float confidenceReject = 1.0 - clamp(mo.w, 0.0, 1.0);
	float warpedLumaA = dot(a.rgb, vec3(0.2126, 0.7152, 0.0722));
	float warpedLumaB = dot(b.rgb, vec3(0.2126, 0.7152, 0.0722));
	float disocclusionReject = smoothstep(0.08, 0.20, abs(warpedLumaA - warpedLumaB));
	float reject = max(residualReject, max(confidenceReject, disocclusionReject));
	vec3 base = mix(mix(a.rgb, b.rgb, 0.5), realFrame, reject);

	// Mini-DLSS spatial finishing pass: 10% stronger edge-aware sharpening.
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
	vec3 sharpened = base + (base - avg) * (0.28 * edgeGate);
	vec3 lo = min(base, min(min(n, s), min(e, west)));
	vec3 hi = max(base, max(max(n, s), max(e, west)));
	color = vec4(clamp(sharpened, max(lo - vec3(0.035), vec3(0.0)), min(hi + vec3(0.035), vec3(1.0))), 1.0);
}'''
    replace_once(frame_gen_cpp, old_fragment, new_fragment)
    mini_shader_constant = '''const char *const kFragmentNormal = R"(
#version 450
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;
layout(set = 0, binding = 0) uniform sampler2D prevTex;
layout(set = 0, binding = 1) uniform sampler2D curTex;
layout(set = 0, binding = 2) uniform sampler2D motionTex;
void main()
{
    vec4 mo = textureLod(motionTex, uv, 0.0);
    vec2 m = mo.xy;
    vec4 a = textureLod(prevTex, uv - m * 0.5, 0.0);
    vec4 b = textureLod(curTex, uv + m * 0.5, 0.0);
    vec3 realFrame = textureLod(curTex, uv, 0.0).rgb;
    float residualReject = smoothstep(0.035, 0.10, mo.z);
    float confidenceReject = 1.0 - clamp(mo.w, 0.0, 1.0);
    float lumaA = dot(a.rgb, vec3(0.2126, 0.7152, 0.0722));
    float lumaB = dot(b.rgb, vec3(0.2126, 0.7152, 0.0722));
    float disocclusionReject = smoothstep(0.08, 0.20, abs(lumaA - lumaB));
    float reject = max(residualReject, max(confidenceReject, disocclusionReject));
    color = vec4(mix(mix(a.rgb, b.rgb, 0.5), realFrame, reject), 1.0);
}
)";'''
    replace_once(frame_gen_cpp, ')";\n} // namespace', ')";\n' + mini_shader_constant + '\n} // namespace')
    replace_once(frame_gen_cpp,
        '#include "compiler.h"\n',
        '#include "compiler.h"\n#include "cfg/option.h"\n')
    replace_once(frame_gen_h,
        '\tbool ready = false;\n',
        '\tbool ready = false;\n\tbool drawMiniDlss = false;\n')
    replace_once(frame_gen_cpp,
        'if (!drawPipeline || renderPass != drawRenderPass)',
        'if (!drawPipeline || renderPass != drawRenderPass || drawMiniDlss != config::MiniDLSS)')
    replace_once(frame_gen_cpp,
        'ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, kFragment)',
        'ShaderCompiler::Compile(vk::ShaderStageFlagBits::eFragment, config::MiniDLSS ? kFragment : kFragmentNormal)')
    replace_once(frame_gen_cpp,
        '\t\tdrawRenderPass = renderPass;\n',
        '\t\tdrawRenderPass = renderPass;\n\t\tdrawMiniDlss = config::MiniDLSS;\n')
    replace_once(
        context_h,
        '#include "vulkan.h"\n',
        '#include "vulkan.h"\n#include "whats_next/frame_generation_bridge.hpp"\n'
    )

    replace_once(
        context_cpp,
        '#ifdef __ORBIS__\n\t// a new frame from the renderer',
        '#ifdef __ORBIS__\n\t// a new frame from the renderer'
    )

    bridge_gate = '''&& fgPrevExtent == extent;
		const bool eligible = interpolate;
		if ((extent.width != fgPrevExtent.width || extent.height != fgPrevExtent.height) && fgPrevExtent.width != 0)
		{
			++fgResourceGeneration;
			whatsNextBridge.reset();
		}
		const auto decision = whatsNextBridge.begin_real_frame(
				eligible, extent.width, extent.height, whats_next::PixelFormat::RGBA8,
				fgResourceGeneration, fgGameSession, whats_next::PresentationCadence{
					(gap > 2 ? 3U : static_cast<std::uint32_t>(gap))});
		interpolate = eligible && decision == whats_next::FrameGenerationDecision::GenerateIntermediate;
'''
    replace_once(context_cpp, 'fgPrevExtent == extent;', 'fgPrevExtent == extent;\n' + '\n'.join(bridge_gate.splitlines()[1:]))
    replace_once(
        context_cpp,
        'EndFrame(overlayCmdBuffer);\n\t\t\tstatic_cast<BaseVulkanRenderer*>(renderer)->RenderVideoRouting();',
        'EndFrame(overlayCmdBuffer);\n\t\t\twhatsNextBridge.commit_real_frame(); // command buffer accepted for submission\n\t\t\tstatic_cast<BaseVulkanRenderer*>(renderer)->RenderVideoRouting();'
    )
    replace_once(
        context_cpp,
        '} catch (const InvalidVulkanContext&) {\n\t\t\t// Re-create swap chain\n\t\t\tresized = true;\n\t\t}',
        '} catch (const InvalidVulkanContext&) {\n\t\t\twhatsNextBridge.abort_real_frame();\n\t\t\t// Re-create swap chain\n\t\t\tresized = true;\n\t\t}'
    )

    replace_once(
        context_cpp,
        '\tif (gui_is_open())\n\t\tfgPrevView = vk::ImageView();',
        '\tif (gui_is_open())\n\t{\n\t\tif (!fgWasGuiOpen)\n\t\t{\n\t\t\t++fgGameSession;\n\t\t\twhatsNextBridge.reset();\n\t\t}\n\t\tfgPrevView = vk::ImageView();\n\t}\n\tfgWasGuiOpen = gui_is_open();'
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

    # Independent switches: WHAT'S NEXT controls generated frames; Mini-DLSS controls
    # spatial sharpening on ordinary and generated frames. Keep the default internal
    # resolution at the upstream 480p baseline to avoid silently increasing GPU load.
    replace_once(option_cpp,
        'Option<bool> FrameGeneration("rend.FrameGeneration", false);\n',
        'Option<bool> FrameGeneration("rend.FrameGeneration", false);\n'
        'Option<bool> MiniDLSS("rend.MiniDLSS", false);\n')
    replace_once(option_cpp,
        'Option<int> RenderResolution("rend.Resolution", 480);',
        'Option<int> RenderResolution("rend.Resolution", 480);')
    replace_once(option_h,
        'extern Option<bool> FrameGeneration;\t// Resonance: in-between pictures for 30 fps games\n',
        'extern Option<bool> FrameGeneration;\t// Resonance: in-between pictures for 30 fps games\nextern Option<bool> MiniDLSS;\n')
    # Move both experimental controls into a dedicated WHAT'S NEXT group.
    # Keep Resonance's existing settings panel and all other groups untouched.
    replace_once(settings_menu_cpp,
        'video.items.push_back(toggle(Tnop("Frame Generation"),\n'
        '\t\t\t\tTnop("Experimental: for games that run at 30 fps, a picture made between each two frames, so the screen shows 60. Can wobble around moving edges and adds a little input lag"),\n'
        '\t\t\t\tconfig::FrameGeneration));',
        '// WHAT\'S NEXT controls are placed in their own group below.')
    replace_once(settings_menu_cpp,
        'groups.push_back(std::move(video));',
        '\t\tvideo.items.push_back(toggle(Tnop("Генерация промежуточных кадров"), Tnop("Пытается вставлять дополнительные кадры между настоящими. Может повысить плавность, но возможны задержка управления и артефакты."), config::FrameGeneration));\n'
        '\t\tvideo.items.push_back(toggle(Tnop("Мини-DLSS — повышение чёткости"), Tnop("Подчёркивает контуры и повышает визуальную чёткость. Это пространственная обработка, а не настоящий нейросетевой DLSS."), config::MiniDLSS));\n'
        '\t\tgroups.push_back(std::move(video));')
    replace_once(settings_menu_cpp,
        'const float scales[] = { 1.f, 1.5f, 2.f, 3.f, 4.f };',
        'const float scales[] = { 1.f, 1.1f, 1.5f, 2.f, 3.f, 4.f };')
    replace_once(context_cpp,
        'const bool miniDlssActive = config::FrameGeneration && !settings.input.fastForwardMode',
        'const bool miniDlssActive = config::MiniDLSS && !settings.input.fastForwardMode')
    # Use a conservative, locally bounded spatial finish; do not raise the default render resolution.
    shaders_cpp_text = shaders_cpp.read_text(encoding="utf-8")
    strength_anchors = ("0.12 * edgeGate", "0.132 * edgeGate", "0.28 * edgeGate")
    strength_matches = [anchor for anchor in strength_anchors if shaders_cpp_text.count(anchor) == 1]
    if len(strength_matches) != 1:
        raise SystemExit("Expected exactly one ordinary-frame Mini-DLSS strength constant; found: " + ", ".join(strength_matches))
    shaders_cpp_text = shaders_cpp_text.replace(strength_matches[0], "0.12 * edgeGate", 1)
    shaders_cpp.write_text(shaders_cpp_text, encoding="utf-8")

    # Fail closed if the bridge was copied but its lifecycle hooks did not land.
    integrated_cmake = cmake.read_text(encoding="utf-8")
    if "${CMAKE_CURRENT_SOURCE_DIR}/whats_next/include" not in integrated_cmake or (chr(92) + "${CMAKE_CURRENT_SOURCE_DIR}/whats_next/include") in integrated_cmake:
        raise SystemExit("Mini-DLSS CMake include path is malformed.")

    integrated_context = context_cpp.read_text(encoding="utf-8")
    required_hooks = (
        "whatsNextBridge.begin_real_frame",
        "whatsNextBridge.commit_real_frame",
        "whatsNextBridge.abort_real_frame",
        "whatsNextBridge.reset()",
    )
    missing_hooks = [hook for hook in required_hooks if hook not in integrated_context]
    if missing_hooks:
        raise SystemExit("Renderer integration incomplete; missing hooks: " + ", ".join(missing_hooks))
    integrated_frame_gen = frame_gen_cpp.read_text(encoding="utf-8")
    if "Mini-DLSS spatial finishing pass" not in integrated_frame_gen:
        raise SystemExit("Mini-DLSS spatial finishing pass was not installed.")
    for marker in ("considerMotionCandidate", "textureConfidence", "disocclusionReject", "mo.w"):
        if marker not in integrated_frame_gen:
            raise SystemExit("ANR-inspired confidence-aware frame generation is incomplete: " + marker)
    if "EndFrame(overlayCmdBuffer);\n\t\t\twhatsNextBridge.commit_real_frame();" not in integrated_context:
        raise SystemExit("Source-frame history commit is not after successful command-buffer submission.")
    if "whatsNextBridge.abort_real_frame();" not in integrated_context:
        raise SystemExit("Renderer abort path does not close the pending source-frame transaction.")
    if "quadMiniDlssDrawer->Draw" not in integrated_context:
        raise SystemExit("Mini-DLSS source-frame finishing path was not installed.")
    if "MINI_DLSS" not in shaders_cpp.read_text(encoding="utf-8"):
        raise SystemExit("Mini-DLSS quad shader variant was not installed.")
    integrated_options = option_cpp.read_text(encoding="utf-8")
    if 'Option<bool> FrameGeneration("rend.FrameGeneration", false);' not in integrated_options:
        raise SystemExit("Frame generation must remain disabled by default until PS4 runtime validation passes.")
    if 'Option<bool> MiniDLSS("rend.MiniDLSS", false);' not in integrated_options:
        raise SystemExit("Mini-DLSS must remain opt-in until PS4 image-quality and performance validation passes.")
    integrated_menu = settings_menu_cpp.read_text(encoding="utf-8")
    for label in ("Генерация промежуточных кадров", "Мини-DLSS — повышение чёткости"):
        if label not in integrated_menu:
            raise SystemExit("Graphics feature toggle missing from the Flycast Video settings menu: " + label)
    if "config::FrameGeneration" not in integrated_menu or "config::MiniDLSS" not in integrated_menu:
        raise SystemExit("Graphics menu entries are not bound to their runtime settings.")

    print(f"Русское меню улучшения изображения подключено к рендереру Flycast: {root}")


if __name__ == "__main__":
    main()
