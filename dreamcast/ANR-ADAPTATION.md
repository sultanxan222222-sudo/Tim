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


## Transferable engineering lessons from the Game Blender PS3-to-PS4 research

The previous PS3 porting investigation is relevant as an engineering discipline and as a source of failure cases. It is **not** a source of PS3 GPU code that can be copied into this Flycast/PS4 renderer.

### Lessons that transfer directly

1. **Never report success before the operation has a real completion meaning.** The PS3 research identified HLE stubs that returned `CELL_OK` without performing the operation or issuing a required callback; examples included asynchronous filesystem completion and SPURS queue/job operations. The graphics equivalent is not to mark a temporal frame accepted, a GPU pass completed, or a quality mode active merely because a command was requested or recorded. Each stage needs an explicit outcome and a real fallback path.
2. **Model asynchronous work as a lifecycle, not a boolean.** Preserve the distinction between a completed emulator source frame, a generated presentation frame, a vblank repeat, a submitted GPU command buffer, and a GPU-completed resource. Only real source frames advance temporal history. Generated frames and duplicated vblanks must never be mistaken for new source history.
3. **Make ownership and ordering explicit.** The PS3 DMA/MFC analysis emphasized tag/status completion, queue ownership and ordering. The Vulkan analogue is explicit image/resource lifetime, pipeline barriers, queue-family ownership where applicable, and fences/semaphores before reusing or destroying GPU resources. PS3 MFC instructions and RSX-specific details are not portable to AMD GCN/Vulkan.
4. **Do not replace missing behavior with optimistic stubs.** SPURS/WWS findings showed that an empty queue push, omitted callback, or fake job completion can produce a permanent wait or a false-success path. Mini-DLSS must similarly propagate shader compilation, descriptor setup, dispatch, and resource errors; on failure it must show the original source frame and keep the optional effect disabled.
5. **Keep CPU and accelerator responsibilities separate.** The PPU/SPU investigation required distinct execution models rather than treating all work as ordinary CPU code. Here the CPU should schedule and validate work; pixel-heavy motion estimation/reconstruction belongs in GPU compute where it is cheaper. The existing CPU block-motion estimator remains a reference/test implementation, not a per-frame PS4 production path.
6. **Measure bottlenecks independently.** Record CPU emulation time, GPU execution time, presentation cadence, median frame time and 1% lows. Do not use CPU submission duration as a substitute for GPU time, and do not add per-frame `WaitIdle()` merely to make measurements easy.

### Lessons that do not transfer as implementation code

- PS3 SELF/SPRX/ELF parsing, PPU/SPU binary translation, syscall/HLE compatibility and game-specific recompilation are relevant to Game Blender's executable-loading/recompilation pipeline. They do not implement image reconstruction in the Flycast Vulkan renderer.
- PS3 RSX command formats, DMA/MFC opcodes, SPU local-store rules, SPURS binary structures and WWS job ABI must not be copied into this PS4 AMD/Vulkan graphics module. Only the general correctness principles above transfer.

### Review points for the current renderer adapter

- The current bridge allocates source-frame IDs and its history gate rejects session, resource, extent, format and sequence discontinuities.
- The adapter currently calls `commit_real_frame()` in the `PresentFrame` path after the completed emulator image is handed to presentation but before the command buffer is finally ended/submitted. This is a deliberate current integration point, **not yet proof of GPU completion**. We must verify that a failed/aborted present cannot leave a committed frame as history; if it can, move the commit to a post-recording acceptance point or add an explicit abort/reset path. Do not add a GPU-wide wait to solve this.
- Renderer repeats and generated frames must not call `begin_real_frame()`; only `PresentFrame` calls carrying a new real image may advance history.
- The render-budget governor is currently a host-tested policy only. It must remain disconnected from automatic runtime switching until PS4 GPU timestamp-query support and its availability on the actual renderer/driver are confirmed.
- Every optional pass must be independently bypassable. On any invalid input, unsupported format, history discontinuity, or shader/backend failure, preserve the original renderer output.

### Validation cases required before enabling the stack by default

Static scene; slow camera pan; fast pan; thin geometry; particles/transparency; HUD; scene cut; resize/swapchain recreation; menu open/close; frame skip; GPU overload; interrupted/aborted present; and repeated vblank with no new source image. Capture image artifacts, GPU time, memory use, frame pacing and controller latency for each case.
