# ANR research and adaptation plan for WHAT'S NEXT

## Research snapshot

Reference project: [Apex Compute / ANR](https://github.com/apex-compute/ANR); pinned research snapshot: [FG-V.B19 release](https://github.com/apex-compute/ANR/releases/tag/FG-V.B19).

The latest public release checked for this adaptation is **FG-V.B19**, published 17 November 2026. The upstream README describes ANR FG as beta, while ANR SR and RR are listed as upcoming betas and the project's own source code is still marked as coming soon. The release notes list Windows 10/11 and Linux/SteamOS with Proton support on NVIDIA RTX 20+, GTX 16, AMD RX 5000+ and Intel Arc A/B GPUs.

ANR FG is not a portable PS4 SDK or renderer library. Its game integration relies on OptiScaler and DLSS-G-compatible provider interfaces on D3D12/Vulkan. The public repository does not currently publish ANR's frame-generation implementation source. Therefore this project must not vendor ANR binaries, NGX DLLs, OptiScaler runtime hooks, or claim direct ANR compatibility with PS4.

## What is reusable

1. **Confidence as a first-class signal.** A best motion vector is not sufficient: match residual, local texture information and ambiguity between the best and next-best candidate must influence whether a generated sample is trusted.
2. **Reject bad temporal inputs.** Scene changes, occlusion/disocclusion, ambiguous motion and history discontinuities should fall back to a real source frame rather than forcing an interpolated result.
3. **Repeatable comparison.** ANR's public [`nr-record` tools](https://github.com/apex-compute/ANR/tree/main/record) record feature inputs losslessly and replay different implementations on identical frames. This specific capture DLL is built around OptiScaler/provider interfaces and is not a PS4 runtime dependency. We adopt the reproducibility principle for our own renderer captures and shader tests, not its DLL protocol.
4. **Measure the result.** Compare image error/artifacts, GPU time, frame pacing and input latency with the effect disabled and enabled. A higher displayed refresh count alone is not evidence that emulation became faster.

## First code adaptation in this branch

The existing Resonance/Flycast GPU motion search is retained. No second frame generator is added.

- The fine motion pass now records a distinct runner-up candidate instead of treating duplicate coarse-neighbour vectors as independent evidence.
- It computes a confidence value from residual quality, candidate uniqueness and local texture variance. The motion image channels are now `xy = motion`, `z = match residual`, `w = confidence`.
- The interpolation fragment pass rejects unreliable vectors and warped samples that disagree strongly (a conservative disocclusion proxy), falling back toward the current real frame.
- The normal-frame Mini-DLSS spatial finish remains a separate stage and retains its own toggle. It is not neural super-resolution.

## Platform boundary

Our target is Resonance's existing Flycast Vulkan renderer on PS4 (RADV/GNM and VideoOut), not a Windows PC game using NGX. Do not bring OptiScaler, D3D12 code, ANR model weights, or PC-only DLLs into the PS4 package. The reusable portion is the image-analysis methodology; the PS4 implementation must remain native Vulkan and be validated against the pinned Resonance/Flycast source.

## Validation still required

- Run the integration script against the exact Resonance-pinned Flycast revision.
- Compile both the frame-generation compute shaders and interpolation fragment shader with glslang.
- Run host CMake/CTest and sanitizer jobs.
- Complete the PS4 toolchain build.
- On a real PS4 Pro, capture representative static scenes, camera pans, thin geometry, particles, transparency and scene cuts. Compare generated frames, GPU cost, memory, frame pacing and controller latency. Keep the feature opt-in until these measurements pass.

No PS4 PKG or on-console runtime validation is claimed by this research note.
