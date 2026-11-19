# Garlic LLM: PS4 GCN compute research

Research date: 19 November 2026  
Reference project: [leonardo-alegria/garlic-llm](https://github.com/leonardo-alegria/garlic-llm)  
Reviewed files: `README.md`, `pc/gpu.py`, `chat/engine.c`, `chat/engine_spec.inc`, `shaders/qwen35.cl`, `shaders/spec.h`, and `LICENSE`.

## Executive assessment

Garlic LLM is a useful primary-source engineering reference for compute on the PS4's GCN GPU. Its strongest contribution to our work is not the language model itself; it is the concrete, tested account of GPU-visible memory, GCN kernel compilation, PM4 dispatch, cache-coherency boundaries, completion signalling, and timing.

It is **not** a drop-in renderer or a Vulkan compute library. The native inference path takes ownership of a low-level GNM submission path and constructs PM4 packets itself. We must not paste those packets into Flycast's Vulkan command buffer or independently manipulate resources owned by the Vulkan renderer. That would bypass the renderer's resource-state, queue, lifetime, and synchronization model.

## What the source actually does

### 1. Native PS4 memory path

- The engine reserves a large direct-memory region using PS4 kernel APIs, maps it for CPU/GPU access, and uses the PS4's write-combined Garlic memory type for GPU-visible weights, data, kernel arguments, and command buffers.
- The source uses 2 MiB direct-memory alignment and a simple 256-byte-aligned linear allocator within the reserved block.
- It deliberately leaves a system-memory margin (64 MiB in the main path; the UI can change the budget) rather than consuming every available byte.
- A GPU-visible region is not the same thing as ordinary cached CPU heap memory. CPU writes, GPU reads, and GPU writes, CPU reads have explicit ordering and cache-visibility steps.

Garlic is the GPU-oriented Radeon Memory Bus route; Onion is the other PS4 APU memory route. The names describe memory paths/attributes, not two independent physical RAM pools. The console has unified GDDR5, but the allocation type changes how the processors access it.

### 2. GCN kernel compilation and metadata

The runtime does not call an OpenCL runtime API. OpenCL C is used as a kernel language; Zig's bundled LLVM/Clang compiles kernels for the GCN target selected in the project (`amdgcn-amd-amdhsa`, `bonaire`). The app loads the resulting machine-code text and kernel descriptors.

The descriptor supplies resource-register values and the user-SGPR layout expected by each kernel. The loader rejects unsupported kernel metadata and scratch spills rather than guessing register values. That is a strong practice: compiler metadata is part of the executable contract between a GCN kernel and the command processor.

The build also records two hardware-specific constraints observed by the author: global memory must use buffer/address instructions rather than FLAT instructions on this target, and certain 128-bit LDS operations produced incorrect results. These are target-specific findings, not universal GCN or Vulkan rules.

### 3. PM4 and GNM dispatch

The app writes PM4 type-3 packets by hand. For a compute dispatch it sets the program address, resource registers, thread dimensions and user data, then emits `DISPATCH_DIRECT`. It submits the command buffer through `sceGnmSubmitCommandBuffers` and calls `sceGnmSubmitDone`.

So the phrase “bypasses drivers” needs a precise reading: it avoids a conventional high-level compute API and builds the GPU command stream itself, but it still uses PS4 kernel/GNM submission and hardware-initialization entry points. It is not a user process writing arbitrary GPU registers without the platform submission mechanism.

### 4. The synchronization details are especially valuable

The production engine establishes a real lifecycle:

1. The CPU builds a command buffer and kernel-argument data in GPU-visible memory.
2. A CPU `sfence` publishes those writes before command submission.
3. Each compute dispatch is followed by a compute partial flush and a memory-acquire/cache operation before dependent work reads the output.
4. At the beginning of a later submission, the engine invalidates GPU L2 state so that CPU changes to reused argument/staging addresses are visible.
5. An end-of-pipe event writes a completion label and performs the required L1/L2 write-back/invalidation before the CPU is allowed to consume GPU results.
6. The CPU waits for that specific label with a timeout, then applies CPU memory fences before reading the result.

The code distinguishes “submission accepted” from “GPU work completed”. It also splits submit and wait so the CPU can prepare the next prompt batch in a separate staging buffer while the GPU executes the current batch. The command buffer itself is not rewritten while the GPU is reading it.

This maps directly to our existing correctness rules: explicit ownership, ordered dependencies, a real completion signal, no false-success state, and no reuse of in-flight buffers. The actual PM4 packet sequence and cache-control constants must not be transplanted into Vulkan.

### 5. A valuable GPU self-test before expensive work

Before loading the multi-gigabyte model, Garlic runs a small compute test. It chains several dispatches where each consumes the previous dispatch's output, reads the results back after the completion label, then rewrites the CPU input at the same addresses and submits again. This tests both inter-dispatch visibility and visibility of fresh CPU data across submissions.

That test-first principle is directly reusable: our PS4 graphics work should have a tiny, deterministic device-side smoke test for buffer upload, dispatch dependency, completion, and readback before we enable an optional effect or allocate large temporal resources. The test must run through the same backend that the feature will use; a GNM test does not certify a Vulkan path.

### 6. Quantization and speculative execution

Garlic's Qwen3.5 model uses custom low-bit tensor layouts: mostly 4-bit weights, 3-bit layouts for selected large matrices, and a compressed attention cache. The MTP head drafts future tokens; the main model verifies them using an acceptance rule that preserves the target sampling distribution. Recurrent DeltaNet state is only committed for accepted tokens.

The low-bit matrix kernels are not relevant to our current frame reconstruction. The speculative-state design is relevant as a general transactional pattern: compute a candidate, verify it, and only then commit persistent state. That is the same discipline we want for temporal frame history and other asynchronous renderer state.

## What transfers to our repositories

| Garlic technique | Relevance | Safe use |
|---|---|---|
| GPU-visible memory budgeting and an explicit OS reserve | High | Apply the budgeting discipline; continue using the renderer's allocator for Vulkan-owned resources |
| Explicit dispatch dependencies and cache visibility | High | Preserve the principle; use Vulkan barriers/fences/queries in the Vulkan path |
| Submit versus GPU-complete lifecycle | High | Model both states separately; never call CPU submission time GPU time |
| Small on-device GPU self-test before enabling a feature | High | Add backend-specific smoke tests and a safe fallback |
| Separate CPU staging from in-flight GPU buffers | High | Use double-buffered staging where profiling justifies it |
| Per-kernel GPU timing and correctness comparisons | High | Use supported Vulkan timestamp queries on the actual PS4 backend, after verifying support |
| GCN kernel compilation and metadata inspection | Medium | Useful only for an isolated native-GNM experiment or compiler research |
| Q3/Q4 model packing and MTP token decoding | Low for rendering | Keep as a reference for a separate on-device AI project, not Mini-DLSS |

### Boundary with WHAT'S NEXT / Mini-DLSS

Our current target is Resonance/Flycast's existing Vulkan renderer. Garlic's direct GNM path is a separate low-level execution route. We should not issue independent PM4 submissions from inside the live Vulkan frame path until device/queue ownership, submission ordering, cache visibility, and resource lifetime have been designed and tested together. A test that passes in a standalone GNM app is not proof that it is safe to mix GNM with Flycast's Vulkan renderer.

For the next graphics step, the safest route is to keep the existing Vulkan integration, verify whether the PS4 Vulkan device exposes valid timestamp queries, and add a very small renderer-owned compute smoke test with explicit barriers and a fence. The render-budget controller must remain disconnected until those measurements are real.

### Boundary with Game Blender

Garlic does not provide PS3 PPU/SPU, RSX, SPURS, or WWS emulation code. Its PM4 packets and PS4 GCN kernels cannot be used as PS3 instruction translations. The transferable part is the disciplined handling of asynchronous work, memory visibility, queue completion, bounded waits, and state commit/rollback.

## Attribution and licence

Garlic LLM's own code is GPL-3.0; the model weights and bundled third-party components have separate licences. This note records architecture and lessons, not copied source. Any future source-code reuse must be reviewed file-by-file for licence compatibility and preserve required notices. The PS4 renderer integration should prefer an independent implementation of the relevant ideas rather than importing Garlic's GNM runtime.

## Source links

- [Garlic LLM README and build notes](https://github.com/leonardo-alegria/garlic-llm/blob/main/README.md)
- [GPU experiment and PM4 builder](https://github.com/leonardo-alegria/garlic-llm/blob/main/pc/gpu.py)
- [Native inference engine and GNM submission lifecycle](https://github.com/leonardo-alegria/garlic-llm/blob/main/chat/engine.c)
- [Qwen3.5 GCN kernels](https://github.com/leonardo-alegria/garlic-llm/blob/main/shaders/qwen35.cl)
- [MTP/speculative decoding and packed low-bit kernels](https://github.com/leonardo-alegria/garlic-llm/blob/main/chat/engine_spec.inc)
- [Project licence](https://github.com/leonardo-alegria/garlic-llm/blob/main/LICENSE)
- [PS4 Onion/Garlic memory-path background](https://pc.watch.impress.co.jp/docs/column/kaigai/621178.html)


## Applied to WHAT'S NEXT: Vulkan GPU timing

The first practical adaptation is now implemented as an independent Vulkan-side helper and renderer overlay, rather than copying Garlic's PM4/GNM submission code:

- `gpu_timestamp_math.hpp/.cpp` converts timestamp deltas using the Vulkan queue family's `timestampValidBits` and the physical device's `timestampPeriod`. It rejects invalid periods, unsupported counters, zero deltas and implausible values, and handles counter wraparound.
- The conversion helper has host regression tests for ordinary values, 32-bit wraparound, 64-bit wraparound and invalid measurements. The local strict-warning host test run passed.
- The overlay allocates two Vulkan timestamp queries per swapchain image, records TOP_OF_PIPE and BOTTOM_OF_PIPE timestamps in the renderer's own command buffer, and marks a pair pending only after the graphics queue accepts that command buffer.
- Query results are read only when Flycast reacquires that same swapchain image after its associated fence wait. No separate wait-for-query or device-wide idle is added to the frame loop.
- Query-pool allocation/result errors disable measurement and leave the original renderer path available. The timestamp getter exposes completed samples to a future adaptive controller.
- The adaptive render-budget controller is **not yet driven** by these samples, and no quality/resolution changes are enabled automatically.

This is an implementation of Garlic's useful *measurement and completion discipline* using the renderer's existing Vulkan queue and fences. It deliberately does not execute Garlic PM4 packets in the Vulkan frame path. The overlay still needs a fresh CI run and validation on an actual PS4 Pro; host tests do not prove console timestamp support or correct GPU results.
