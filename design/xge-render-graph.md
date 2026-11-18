# XGE Render Graph

> Status: design (2026-11). Not implemented. The DX12 renderer draws only the editor UI, with hand-written barriers and a GPU wait every frame. See [Current state](#current-state) for what the code has today and [Prerequisites](#prerequisites) for what has to exist first.

The render graph separates **what** a frame draws (passes and the resources they read and write) from **how** it runs on the GPU (order, barriers, memory, queues and threads). Render features describe passes. The graph compiles them into a schedule and executes it against the renderer's backend.

It builds on the [Dispatch Graph](xge-dispatch-group.md): the Dispatch Graph decides when CPU work runs, and the render graph decides when GPU work runs and how it is recorded.

---

## Contents

1. [Overview](#overview)
2. [Layers](#layers)
3. [Resources](#resources)
4. [Passes](#passes)
5. [Compilation](#compilation)
6. [Execution](#execution)
7. [Draw queues and sorting](#draw-queues-and-sorting)
8. [Frame model in XGE](#frame-model-in-xge)
9. [API and ABI](#api-and-abi)
10. [C# interop](#c-interop)
11. [Editor and UI integration](#editor-and-ui-integration)
12. [Debugging](#debugging)
13. [Benefits and costs](#benefits-and-costs)
14. [Current state](#current-state)
15. [Prerequisites](#prerequisites)
16. [Open design questions](#open-design-questions)
17. [Implementation phases](#implementation-phases)

---

## Overview

A frame goes through three steps.

1. **Record.** Render features add passes to the graph. Each pass declares the resources it creates, reads and writes. Resources are virtual: a description (size, format, usage), not GPU memory. Nothing is recorded into a command list yet.
2. **Compile.** The graph orders the passes, removes passes whose output nobody uses, works out when each resource is first and last used, assigns memory (sharing it between transient resources whose lifetimes don't overlap) and plans every barrier.
3. **Execute.** Each surviving pass records its commands. The graph inserts the planned barriers between passes, then submits the command lists in order.

This is how explicit APIs (DX12, Vulkan) want to be driven: the information the driver used to guess (resource states, lifetimes, dependencies) is known up front.

---

## Layers

The original draft mixed two systems: the graph (passes, resources, barriers) and the draw queue (sorting and batching draws inside one pass). They're separate layers.

```
Scene graphs, actors, components         gameplay data (owned by Dispatch groups)
          │  extract
          ▼
Render world                             read-only snapshot for this frame: proxies, cameras, lights
          │
          ▼
Render features                          Mesh, UI, Post, Debug ... each adds passes
          │
          ▼
Render graph                             passes + virtual resources → compile → execute
          │  per pass
          ▼
Draw queues                              sort keys, batching, instancing (inside one pass)
          │
          ▼
RHI                                      device, resources, descriptors, command lists, barriers
          │
          ▼
Backend                                  xgRendererDX12 (later Vulkan)
```

| Layer | Knows about | Doesn't know about |
|---|---|---|
| Render world | Proxies (mesh, material, transform, bounds), cameras, lights | Scene graph internals, GPU objects |
| Render features | Which passes a technique needs | Barriers, memory placement |
| Render graph | Resources, passes, access, queues | What a pass draws |
| Draw queues | Draw items and sort keys | Other passes |
| RHI | GPU objects and commands | Passes, scenes |

C# enters only at the render world and resource records ([C# interop](#c-interop)). Every layer below is native.

---

## Resources

### Kinds

| Kind | Lifetime | Examples |
|---|---|---|
| **Transient** | Created and discarded inside one frame. Memory may be aliased. | G-buffer, depth, bloom chain, shadow maps rebuilt each frame |
| **Imported** | Owned outside the graph. The graph tracks its state only for this frame. | Swap chain back buffer, mesh vertex buffers, material textures |
| **Persistent** | Owned by the graph across frames, never aliased. | TAA history, auto-exposure, editor viewport texture |

### Descriptions

```cpp
struct TextureDesc
{
    uint32_t Width, Height, DepthOrLayers;
    uint16_t MipLevels;
    uint16_t SampleCount;
    Format   Format;
    TextureDimension Dimension;
    const char* DebugName;
};

struct BufferDesc
{
    uint64_t Size;
    uint32_t Stride;
    const char* DebugName;
};
```

Sizes can be relative to the output (`ScaleOfBackBuffer(0.5f)`), resolved at compile time, so a resize doesn't need features to rebuild their descriptions.

### Versioned handles

A handle names one **version** of a resource. Writing a resource returns a new handle. A pass that reads `depth_v1` depends on the pass that produced `depth_v1`, and not on a later pass that produced `depth_v2`.

This is what keeps the graph acyclic and gives every edge without the user writing any: edges come from versions, never from declaration order.

### Access

Every declared use has an access type. It decides the barrier, the view and whether the use is a read or a write.

| Access | Read/Write | DX12 legacy state |
|---|---|---|
| `ShaderRead` | R | `PIXEL_SHADER_RESOURCE` / `NON_PIXEL_SHADER_RESOURCE` (by pass stage) |
| `ShaderWrite` (UAV) | W | `UNORDERED_ACCESS` |
| `ColorTarget` | W | `RENDER_TARGET` |
| `DepthWrite` / `DepthRead` | W / R | `DEPTH_WRITE` / `DEPTH_READ` |
| `CopySource` / `CopyDest` | R / W | `COPY_SOURCE` / `COPY_DEST` |
| `IndirectArgs` | R | `INDIRECT_ARGUMENT` |
| `Present` | R | `PRESENT` |

A use can name a subresource range (mip, array slice), so a mip-chain downsample doesn't serialize on the whole texture.

---

## Passes

```cpp
enum class PassType : uint8_t { Raster, Compute, Copy };
enum class QueueHint : uint8_t { Graphics, AsyncCompute, Copy };
```

A pass has:

- a **setup** step that runs while recording and declares access (`Create`, `Read`, `Write`, color and depth targets with load/store ops);
- an **execute** step that runs later and records commands through the RHI, using only the resources it declared;
- a type and a queue hint;
- an optional **side effect** flag. Passes that write to the back buffer, a readback buffer or a persistent resource are never culled.

Load and store ops are part of the target declaration (`Clear`, `Load`, `DontCare` / `Store`, `Discard`). They map to DX12 render passes (`BeginRenderPass`) and later to Vulkan, and they let the compiler skip a clear or a resolve nobody reads.

---

## Compilation

Compilation is CPU-only and deterministic, so it can be tested without a GPU ([phase 2](#implementation-phases)).

### Rule 1: edges come from versions

A pass that reads version `n` of a resource depends on the pass that wrote version `n`. Reading and writing the same version in one pass is an error unless the access allows it (UAV read-write, depth test and write).

### Rule 2: unused passes are culled

Starting from side-effect passes, the compiler walks backwards through the edges. Passes it never reaches are removed, and so are resources only they used. A debug overlay that is switched off costs nothing, without the feature checking a flag.

### Rule 3: the order is deterministic

The topological sort breaks ties by declaration order, the same rule the Dispatch Graph uses ([question 3, rule 4](xge-dispatch-group.md#rule-4-delivery-order)). The same frame always compiles to the same schedule, whatever the thread timing.

### Rule 4: lifetimes and aliasing

- Each transient resource gets an interval: its first and last pass in the sorted order.
- Resources whose intervals don't overlap can share memory. They're created as placed resources in a heap, and the first use after a switch gets an aliasing barrier and a discard (or a clear).
- Aliasing between queues is only allowed when a fence orders the two uses. Otherwise the async-compute overlap would read memory another pass is writing.
- The first version is simpler: every transient gets its own committed resource from a pool keyed by description, reused across frames. Aliasing comes in [phase 5](#implementation-phases).

### Rule 5: barriers are planned, not written

- The compiler knows every resource's access in every pass, so it emits the transitions between them. Features never call `ResourceBarrier`.
- Barriers between two passes are batched into one call.
- Where there's a gap between the last write and the next read, the compiler can split the barrier (begin after the writer, end before the reader).
- The baseline is DX12 legacy resource states. Enhanced Barriers (`ID3D12GraphicsCommandList7::Barrier`) are an optional backend path later, see [question 2](#2-barrier-model).
- Imported resources start and end the frame in a known state (the back buffer starts as `PRESENT` and returns to it).

### Rule 6: compiled graphs are cached

Most frames build the same graph. The compiler hashes the structure (passes, declared access, resource descriptions) and reuses the previous schedule, lifetimes and memory layout when the hash matches. Setup still runs every frame; only compilation is skipped.

---

## Execution

### Frames in flight

The renderer keeps 2 or 3 frames in flight. Each frame has its own command allocators, upload ring region, transient resource set and fence value. The CPU waits only when it is about to reuse a frame's resources that the GPU hasn't finished with. Today the renderer waits for the GPU after every present, so this is a [prerequisite](#prerequisites).

### Recording

- In the first version, passes record in order on one command list on the main thread.
- Later, consecutive passes are grouped into chunks, and each chunk records into its own command list on a worker from the Dispatch Graph scheduler ([dispatch phase 5](xge-dispatch-group.md#implementation-phases)). The render graph doesn't create threads.
- Barriers are recorded at the start of the chunk that needs them, so chunk boundaries never lose a transition.
- Submission stays single-threaded and in compiled order: `ExecuteCommandLists` with the chunks in sequence.

### Queues

- Graphics only at first.
- Async compute later: passes with `QueueHint::AsyncCompute` and no graphics dependency in between run on a compute queue. The compiler inserts a fence signal and wait at each cross-queue edge.
- A copy queue for uploads is a renderer concern, outside the graph. Uploaded resources enter the graph as imported resources once their copy fence has passed.

---

## Draw queues and sorting

Draw sorting happens inside a pass at execution time, not in the graph compiler. A pass that draws the render world builds a draw queue from the visible proxies and sorts it.

### Sort key

One 64-bit key per draw item, sorted with a radix sort:

```
63      60 59             40 39             20 19              0
┌────────┬─────────────────┬─────────────────┬─────────────────┐
│ layer  │ pipeline (PSO)  │ material        │ depth           │
└────────┴─────────────────┴─────────────────┴─────────────────┘
```

- **Opaque:** layer, then pipeline, then material, then depth front-to-back. Grouping by pipeline cuts state changes; depth within a group helps early-Z.
- **Transparent:** layer, then depth back-to-front (inverted bits), then pipeline. Correct blending wins over state changes.
- **Depth prepass:** when present, the opaque pass can sort purely by pipeline and material, because early-Z already comes from the prepass.

### Batching and instancing

- Consecutive items with the same mesh, pipeline and material can become one instanced draw, but only once there is a per-instance data path (an instance buffer or a structured buffer indexed by `SV_InstanceID`). Without it there is nothing to fold into.
- Later, GPU-driven culling and `ExecuteIndirect` replace CPU batching for large scenes. Not part of this design's first phases.

---

## Frame model in XGE

The Dispatch Graph splits a frame into frame start, the group graph and frame end ([question 2, rule 1](xge-dispatch-group.md#rule-1-the-frame-has-three-stages)). The render graph fits in like this:

```
Frame start   (main)        poll events, structural commands
Graph
  Gameplay, Physics, Animation ...
  TransformPropagation      world transforms
  RenderPrep                extract: scene → render world (proxies, cameras, lights), frustum cull
  Render (main, optional)   user render hooks: debug draw, UI building
Frame end     (main)        Renderer::Render
                              features add passes → compile → execute → present
```

### Rule 1: extraction is the only scene access

- `RenderPrep` copies what rendering needs (transform, mesh id, material id, bounds) into the render world. After it, the renderer never reads scene graphs or components.
- `RenderPrep` declares reads of the scene graphs it extracts from, under the Dispatch Graph's data-hazard rules ([question 4, rule 2](xge-dispatch-group.md#rule-2-reads-from-other-groups-are-declared-and-the-builder-checks-them)).
- The render world is owned by the renderer. It's written only during `RenderPrep` and read only during frame end.

### Rule 2: one frame of pipelining is a later option

Because the renderer reads only the render world, the next frame's groups could run while the renderer still records the current one, with a double-buffered render world. That changes input-to-photon latency and isn't part of the first phases. The design keeps it possible by never letting the renderer reach back into scene data.

### What this changes in existing code

- `Engine::Run` keeps calling `Renderer::Render()` at the end of the frame. Its body becomes "build graph, compile, execute, present".
- Until the Dispatch Graph exists, extraction runs inline in `Engine::Run` before `Renderer::Render()` (or in `game.kernel`'s update).

---

## API and ABI

The renderer is a DLL (`xgRendererDX12`), loaded through `RendererModule`. The graph builder takes callbacks, and the CLAUDE.md rule forbids STL types across DLL boundaries, so `std::function` setup and execute lambdas can only live on one side.

### Rule 1: the graph lives inside the renderer DLL

- The graph, the compiler and the built-in features (UI, mesh, post) are code inside the renderer module. Internally they use a normal C++ API:

```cpp
graph.AddPass("ForwardOpaque", PassType::Raster,
    [&](PassBuilder& b)
    {
        _color = b.ColorTarget(_color, LoadOp::Clear, StoreOp::Store);
        _depth = b.DepthTarget(_depth, LoadOp::Clear, StoreOp::Store);
        b.Read(_shadowMap, Access::ShaderRead);
    },
    [=](PassContext& ctx)
    {
        DrawQueue& queue = ctx.Queue(DrawQueueKind::Opaque);
        queue.Sort();
        queue.Execute(ctx.Commands());
    });
```

- The CPU-only compiler can live in a static lib linked by the renderer and by the `verify/` tests. It holds no globals, so the per-DLL copy problem with static libs doesn't apply.

### Rule 2: the engine talks to the renderer through a stable interface

- New calls on `xg::Renderer` are appended pure virtuals with C types only: render world updates (`SubmitProxies(const RenderProxy*, uint32_t)`), cameras, viewport textures, feature toggles.
- POD structs (`RenderProxy`, `CameraDesc`) are declared in a public header next to `xgRenderer.h`, with explicit sizes so the managed side can mirror them like the math types (`MathLayout.Validate()`).

### Rule 3: external passes use C callbacks

If modules need their own passes later (a custom post effect from a game module), they register through a C ABI: a POD pass description plus `setup` and `execute` function pointers with a `void* user`. Execute receives an opaque command context with `XG_Render*` exports. Not part of the first phases.

### Rule 4: managed code uses data, not passes

C# code submits render data through the same C exports as native modules. See [C# interop](#c-interop).

---

## C# interop

> Proposed decision (2026-11): rendering stays native. C# binds at the render world and resource level (L1–L3). Custom passes from C# (L4) are deferred, and raw GPU access (L5) is never bound.

The graph, the compiler, barriers, draw queues, extraction and every built-in feature are native code in the renderer and xgCore. C# describes what is in the world and how rendering is configured. It doesn't record GPU work.

### Rule 1: the binding level

| Level | What C# can do | Decision |
|---|---|---|
| L0 | Nothing | — |
| **L1 Render data** | Create, update and destroy render proxies (mesh + material on an actor), cameras and lights. Immediate debug draw (lines, boxes, spheres, text). | **Bound.** The core of the API. |
| **L2 Control and queries** | Feature toggles, render settings (clear color, exposure, quality), which camera feeds which viewport. Read-only frame stats and per-pass timings. | **Bound.** |
| **L3 Resources** | Create meshes, textures and materials from data (later from asset ids). Set material parameters by interned name id. | **Bound.** |
| L4 Custom passes | Declare a pass through the same C callback ABI as native external passes ([API rule 3](#rule-3-external-passes-use-c-callbacks)). The execute step gets a narrow command context: set material, draw mesh, fullscreen triangle, dispatch. | **Deferred.** Opt-in API version. Fullscreen and post-process passes first. |
| L5 Raw RHI | Command lists, barriers, descriptors, pipelines | **Never.** |

Why the cuts fall there:

- **L5** would expose the barriers and lifetimes the graph exists to hide, and a wrong managed call would corrupt the GPU state of every native feature.
- **L4** callbacks run during execution, which moves to worker threads in [phase 6](#implementation-phases). Managed code there must be thread-safe, must not allocate, and pays a GC transition per pass. That's worth it only once the native feature set is stable.
- **L1–L3** are data. They need no knowledge of passes, are easy to batch, and work unchanged when the backend changes.

### Rule 2: per-frame cost follows changes, not object count

- Transforms never cross the boundary per frame. Extraction in `RenderPrep` reads world transforms natively from the scene graph by ActorID.
- C# sends only changes: a proxy created, a mesh or material swapped, visibility toggled, a camera moved. Changes are batched into spans:

```c
XG_MODULE_EXPORT int32_t XG_RenderUpdateProxies(
    xg::ScriptEngine* engine, uint32_t moduleId,
    const XG_RenderProxyUpdate* updates, uint32_t count);
```

- Debug draw accumulates in a managed buffer and is submitted once per frame per module.
- The target is a handful of calls per module per frame, whatever the scene size.

### Rule 3: handles, not pointers

- Every render object is an opaque 64-bit handle (index + generation). C# wraps them in `readonly struct` types: `MeshHandle`, `TextureHandle`, `MaterialHandle`, `ProxyHandle`, `CameraHandle`, `LightHandle`.
- A stale handle is detected by its generation, logged once and ignored. It never crashes.
- Managed code holds no native pointers and uses no finalizers. Objects are destroyed explicitly, and native code cleans up the rest:
  - proxies attached to an actor are released when the actor is destroyed;
  - everything a module created is tracked by module id and released when the module unloads, which keeps hot reload safe.

### Rule 4: calls are deferred commands

- Exports write into a command buffer per dispatch group. The buffers are applied at the start of `RenderPrep`, which is the `Deferred` policy of the Dispatch Graph ([question 4, rule 3](xge-dispatch-group.md#rule-3-a-system-declares-a-concurrency-policy)). C# can call them from any group and never touches renderer state directly.
- Creating a handle returns at once: the id is allocated atomically, like `ActorRegistry::GenerateID`. The object becomes real when its buffer is applied.
- Queries (L2 stats and timings) return the values of the last completed frame.

### Rule 5: the exports are backend-neutral

- Managed code never imports `xgRendererDX12`. The backend is chosen in `gameconfig.json`, may be absent (SDL GPU fallback) and will gain a Vulkan sibling.
- The exports live in `xgScriptNative` (`interop/xgRenderInterop.cpp`), take `ScriptEngine*` and resolve a `RenderWorld` system with `engine->GetSystem<RenderWorld>(module)`, the same way `XG_SendMessageToEngine` resolves the Messenger.
- `RenderWorld` is a renderer-neutral system in xgCore. It owns proxies, cameras, lights and resource records. The active renderer reads it at frame end and keeps the GPU objects.
- Without a renderer, `RenderWorld` still accepts the data and nothing draws. Game code doesn't branch on it, and a headless server gets this for free.
- Names follow the C ABI rule: prefix `XG_Render` with no underscore after it (`XG_RenderCreateMesh`, `XG_RenderUpdateProxies`, `XG_RenderGetApiVersion`).

### Rule 6: ABI hygiene, as in xgEditorUI

- `XG_RenderGetApiVersion()` is checked once at startup against the managed `ApiVersion`, like `XG_UIGetApiVersion`.
- Every descriptor struct starts with `uint32_t Size`, and new fields are appended.
- `XG_RenderSizeOf(typeId)` plus `RenderLayout.Validate()` in `ScriptRuntime.Initialize`, like `XG_MathSizeOf` and `MathLayout.Validate()`.
- Strings cross only at creation (debug names, material parameter names, feature names) and are interned to `uint32_t` ids. Per-frame calls use ids.
- Blittable types only: `[assembly: DisableRuntimeMarshalling]`, `LibraryImport`, `delegate* unmanaged` callbacks with `nint User`. `[SuppressGCTransition]` only on trivial, non-blocking queries. CoreCLR and NativeAOT (`GameDemoModule`) use the same bindings.
- Math crosses as the existing blittable mirrors (`Matrix4x4`, 16-byte `Vector3`, `Color`).
- No exceptions cross the boundary. Exports return status codes, and the managed wrappers report misuse through `Debug.LogError`.
- `verify/RenderParity.ps1`, modeled on `verify/EditorUIParity.ps1`, checks that every `XG_Render*` export is bound.

### Managed layout

Namespace `xg.Rendering` in `xgSharedKit.Managed`, because it needs `Actor` and the component types. Every new file needs a `<Compile Include>`. It moves to its own assembly only if it grows.

| Type | Level | Role |
|---|---|---|
| `RenderNative` (internal) | — | 1:1 `LibraryImport` bindings, like `xgEditorUI.Managed/Native.cs` |
| `Mesh`, `Texture`, `Material` | L3 | Create from data, set parameters, destroy |
| `MeshRenderer` | L1 | Component holding a `ProxyHandle`. Mesh, material and visibility changes set a dirty flag, flushed in one batch per frame |
| `Camera`, `Light` | L1 | Camera and light proxies attached to actors |
| `DebugDraw` | L1 | `Line`, `Box`, `Sphere`, `Text` into a per-frame buffer |
| `RenderSettings`, `RenderStats` | L2 | Toggles and settings; last-frame stats and pass timings |

```csharp
var mesh     = Mesh.Create(vertices, indices, "Crate");
var material = Material.Create("Lit", "CrateMat");
material.SetColor("BaseColor", Color.White);

var crate = Actor.Create("Crate");
crate.AddComponent<MeshRenderer>().Set(mesh, material);

DebugDraw.Line(Vector3.Zero, new Vector3(0, 2, 0), Color.Green);
```

### What this changes in existing code

- New `RenderWorld` system in xgCore, with its interface in `xgCore/public` under the appended-virtual rules.
- New `xgScriptNative/interop/xgRenderInterop.cpp` and a public C header for the `XG_Render*` exports and their POD structs.
- New `xg.Rendering` files in `xgSharedKit.Managed`.
- `ScriptRuntime.Initialize` adds `RenderLayout.Validate()` and the API version check.

---

## Editor and UI integration

### The UI is the first pass

- Today `SubmitUI` stores the ImGui draw data and `RecordUI` draws it straight to the back buffer, with manual transitions.
- In the graph, the UI becomes a raster pass writing the imported back buffer (side effect: present). It reads any viewport textures the editor shows.
- `xgEditorUI` keeps using `UIRenderTarget`. No ImGui code moves into the renderer, and no DX12 code into the editor.

### The editor viewport is a persistent texture

- The scene renders into a persistent color texture sized to the viewport panel, not the window.
- The renderer exposes it to the UI as an ordinary UI texture id, and the panel draws it with `XG_UIDrawImage`. The UI pass declares a `ShaderRead` of it.
- Today `XG_UICreateTexture` only creates a texture from RGBA pixels. xgEditorUI needs a new export that wraps an existing `UIRenderTarget` texture id in an `XG_UITexture` (prerequisite E7).
- Resizing the panel resizes the texture at the next frame start.

### The SDL GPU fallback stays UI-only

When `RendererModule` is empty, `xgPlatform`'s `UIRenderTargetSDLGPU` presents the editor UI. It does its own synchronization and has no scene rendering, so it gets no graph. Decided here so the graph never has to target two backends at once.

---

## Debugging

- **Graph dump:** the compiled schedule (passes, resources, versions, lifetimes, barriers, culled passes) printed to the log on request, through `xg::Log`.
- **Graphviz export:** the same data as a `.dot` file. Later shown in the editor ([phase 8](#implementation-phases)).
- **PIX markers:** `BeginEvent`/`EndEvent` around every pass with its name. Every resource gets `SetName` from its `DebugName`.
- **Access validation (debug builds):** the RHI command context checks that a pass binds only resources it declared, with the declared access. A missing declaration would otherwise show up as a missing barrier and a random corruption.
- **Aliasing off switch:** a config flag that gives every transient its own memory, to tell aliasing bugs from other bugs.
- **GPU timings:** timestamp queries per pass, shown with the Dispatch Graph's per-group timings.

---

## Benefits and costs

| Area | Without a graph | With the graph | Cost |
|---|---|---|---|
| Memory | Every intermediate target allocated for the whole run | Transients share heap memory when lifetimes don't overlap | Aliasing bugs are hard to see; needs placed resources and the validation switch |
| Synchronization | Hand-written barriers in each feature; easy to miss or over-sync | Planned from declared access; batched and split | Wrong declarations still break it; validation needed |
| Features | Each feature knows the frame layout | Features declare inputs and outputs; unused ones are culled | One more level of indirection when debugging |
| State changes | Draw order is whatever the scene gives | Sort keys group by pipeline and material | Sorting cost per pass |
| Threads | One command list | Chunks recorded in parallel on scheduler workers | Needs the Dispatch Graph worker pool |
| CPU | No compile step | Compile step per frame, cached when the structure is unchanged | Hashing and setup every frame |

Numbers (VRAM saved, barrier count, CPU time) are measured and logged in each phase, not assumed.

---

## Current state

| Piece | Where | State |
|---|---|---|
| Renderer interface | `xgCore/public/xgRenderer.h` | `Renderer : UIRenderTarget` with `Render()` and `GetNativeHandle()`. No resource, shader, camera or pass API. |
| UI draw API | `xgBase/public/xgUIDraw.h` | `UIDrawList` and `UIRenderTarget` (create, update and destroy texture, `SubmitUI`, clear color). Implemented by DX12 and by SDL GPU. |
| DX12 backend | `xgRendererDX12/RendererDX12.*` | About 720 lines. Adapter selection, debug layer, swap chain with resize. One UI pipeline compiled at run time with `D3DCompile` (vs_5_0, ps_5_0). One 256-slot SRV heap. Committed resources only. Manual `ResourceBarrier` transitions. One command allocator, and `WaitForGPU()` after every present and every texture upload, so there are no frames in flight. |
| SDL GPU fallback | `xgPlatform/UIRenderTargetSDLGPU.cpp` | UI-only presenter used when `RendererModule` is empty. Supports multi-viewport. |
| Frame loop | `Engine::Run` in `xgCore/xgEngine.cpp` | Delta time, `PollEvents`, ScriptTree, `UpdateActors`, `Renderer->Render()`. |
| Scene data | `xgBase/public/xgSceneNodeEngine.h` | `SceneNodeTransform` holds a local `Transform` but nothing uses it. No transform propagation, camera, mesh or material components. |
| Component storage | `xgCore` `CompositeSceneGraph` / `StridePool` | Pointers into storage are invalidated by growth and swap-and-pop (ENGINE_STATUS §4 P1). |
| Assets | — | None. Only `FileWin` file I/O. No mesh, texture or shader assets. |
| Math | `xgBase/math/` | Ready: `Mat4x4`, `Frustum` (D3D 0..1 depth), `AABB`, `Sphere`, `OBB`, culling helpers including a batch `XG_FrustumCullSpheres`. |
| Scheduler | [xge-dispatch-group.md](xge-dispatch-group.md) | Design only. Everything runs on the main thread. |

---

## Prerequisites

### Renderer side (needed before the graph)

| # | Prerequisite | Why | Hard? |
|---|---|---|---|
| R1 | **Frames in flight:** per-frame command allocators and fence values; remove the per-present and per-upload `WaitForGPU`; a deferred-release queue that frees resources after the GPU is done | Transient pools and per-frame data are meaningless while the CPU waits for the GPU every frame | Hard |
| R2 | **Upload ring:** a per-frame staging allocator for buffers and textures instead of a blocking upload per texture | Meshes, constants and per-instance data need cheap uploads | Hard |
| R3 | **Minimal RHI** inside the renderer: buffers, textures, views, samplers, shaders, pipelines, root signature, command context | The graph and features record through it; also keeps a Vulkan backend possible | Hard |
| R4 | **Descriptor management:** a growable shader-visible heap with an allocator (bindless indices), plus RTV and DSV heaps | 256 fixed slots for UI textures won't hold scene resources | Hard |
| R5 | **Shader toolchain:** DXC (SM 6.x), compiled at build time, replacing run-time `D3DCompile` SM 5.0 | Bindless and modern features need SM 6; run-time compile hides errors until launch | Hard |
| R6 | **Placed resources and heaps** | Only needed for aliasing | Soft (phase 5) |
| R7 | **Debug support:** PIX markers, object names, GPU-based validation toggle in config | Every later phase depends on being able to read a capture | Hard |

### Engine side

| # | Prerequisite | Why | Hard? |
|---|---|---|---|
| E1 | **Renderer ABI** for render data: appended virtuals on `xg::Renderer` or C exports, POD descriptors, no STL | Modules and C# must feed the renderer across DLLs | Hard |
| E2 | **Something to draw:** mesh, material and texture data, procedural or from a minimal loader; asset ids later | A scene pass has no input otherwise | Hard for phase 4 |
| E3 | **Transforms and a camera:** world transform propagation from `SceneNodeTransform`, a camera component or system | Needed to build view and projection, and proxy transforms | Hard for phase 4 |
| E4 | **Render world extraction:** a proxy store fed from scene graphs, after the `StridePool` pointer fixes | Proxies must not point into storage that moves | Hard for phase 4 |
| E5 | **Dispatch Graph phases 1–2** (frame stages, `RenderPrep` group) | The natural home for extraction | Soft: extraction can run inline in `Engine::Run` first |
| E6 | **Dispatch Graph phase 5** (worker pool) | Parallel command list recording | Soft (render phase 6) |
| E7 | **Editor viewport plumbing:** an `XG_UI*` export that wraps a renderer-owned texture id in an `XG_UITexture`, so a panel can show it with `XG_UIDrawImage` | The editor viewport is the first visible 3D output | Hard for phase 4 |
| E8 | **Headless test harness** in `verify/` for the compiler | Ordering, culling, lifetimes, aliasing and barrier plans are pure logic and must be tested without a GPU | Hard for phase 2 |
| E9 | **Engine P0 fixes** from ENGINE_STATUS §4 (shutdown order, module removal wiping root systems) | The renderer and render world are systems; their teardown must be reliable | Soft but recommended |
| E10 | **`RenderWorld` system and the `XG_Render*` C ABI** in xgScriptNative, with layout and version checks ([C# interop](#c-interop)). Depends on E1, E3 and E4 | C# code can't create or place anything to draw | Hard for C# in phase 4 |

### Not prerequisites

Async compute, Enhanced Barriers, Vulkan, multi-viewport on DX12, GPU-driven rendering and managed pass authoring are later phases, not entry conditions.

---

## Open design questions

### 1. Where the graph code lives

Proposed: compiler in a GPU-free static lib (linked by the renderer and the tests), execution and features in the renderer DLL. Alternative: everything in the renderer DLL, tests through an exported test entry. The static lib is simpler to test and holds no globals.

### 2. Barrier model

Legacy resource states work on every DX12 device. Enhanced Barriers are closer to Vulkan (separate sync, access and layout), allow finer barriers and drop some implicit promotions, but need a recent Agility SDK and driver. Proposed: legacy states in the planner's backend first, with the planner's internal model expressed as sync/access/layout so Enhanced Barriers and Vulkan are a backend change only.

### 3. Rebuild every frame or cache

Proposed in [rule 6](#rule-6-compiled-graphs-are-cached): setup every frame, compile only when the structure hash changes. Needs a decision on what goes into the hash (relative sizes resolve after resize, so the resolved sizes must be in it).

### 4. Bindless from day one

Bindless (descriptor indices in constants, one big heap) simplifies root signatures and fits a graph well, but needs SM 6.6 for `ResourceDescriptorHeap`. Proposed: yes, if DXC and the target hardware allow it; otherwise a small fixed root signature with descriptor tables.

### 5. Shader assets

Where compiled shaders live (`gameroot` folder, embedded resource), how pipelines are keyed and cached (`ID3D12PipelineLibrary` or a hash map), and whether hot reload of shaders is in scope for the editor.

### 6. Vulkan readiness of the RHI

How much of the RHI shape to fix now so a Vulkan backend fits later (render pass objects, descriptor sets vs bindless, queue families). Proposed: design RHI types against both APIs, implement DX12 only.

### 7. Managed pass authoring

> Proposed decision (2026-11): L1–L3 bound, L4 deferred, L5 never. See [C# interop](#c-interop), rule 1.

Still open for L4: which command context operations to expose, and whether managed execute callbacks may run on worker threads or are pinned to the main thread.

### 8. Render world ownership

Which ScriptNode or Dispatch group owns the render world system. Proposed: registered by the renderer module's owner (the kernel), written by `RenderPrep`, `MainThreadOnly` or `Owned` under the Dispatch Graph's concurrency policies ([question 4, rule 3](xge-dispatch-group.md#rule-3-a-system-declares-a-concurrency-policy)).

### 9. Naming

"Render graph" (the structure), "pass", "transient / imported / persistent resource", "render feature", "render world", "draw queue". Avoid "frame graph" in code so it isn't confused with the Dispatch Graph's frame stages.

---

## Implementation phases

Each phase leaves `xge.exe` running with the editor UI on DX12, and testable with the existing run steps.

| # | Phase | Result |
|---|---|---|
| 0 | Renderer foundations: frames in flight, upload ring, deferred release, PIX markers, object names (R1, R2, R7) | Same output, no GPU wait every frame |
| 1 | RHI inside `xgRendererDX12`, descriptor allocator, DXC shaders; port the UI pass onto it (R3, R4, R5) | UI renders through the RHI |
| 2 | GPU-free graph compiler: resources, versions, passes, culling, deterministic sort, lifetimes, barrier plan; `verify/` tests (E8) | Graph logic tested headless |
| 3 | Graph execution on DX12: pooled committed transients, planned legacy-state barriers, back buffer as an imported resource; the UI is the first pass | Editor drawn through the graph, no hand-written barriers |
| 4 | Scene path: transforms and camera, render world extraction, a forward pass with depth, draw queues and sort keys, editor viewport texture (E1–E4, E7). `RenderWorld` and the C# L1 and L3 bindings: `MeshRenderer`, `Camera`, `DebugDraw`, `Mesh`, `Material` (E10) | First 3D in the editor viewport, placed from C# |
| 5 | Transient aliasing with placed resources, compiled-graph cache (R6). C# L2: `RenderSettings`, `RenderStats`, `verify/RenderParity.ps1` | VRAM saving measured and logged |
| 6 | Parallel recording in chunks on Dispatch Graph workers (E6) | Multi-threaded command lists |
| 7 | Async compute queue with cross-queue fences, Enhanced Barriers path, instancing | Overlap and fewer barriers |
| 8 | Tooling: graph dump, Graphviz export, per-pass GPU timings and a graph view in the editor | The frame is visible and debuggable |
| 9 | Optional: C# L4, fullscreen and post-process passes through the C callback ABI and a narrow command context | Game modules add simple effects in C# |

Phases 0 to 3 change only the renderer and add tests. They give the graph a working backend before any scene rendering exists, so phase 4 is about scene data, not renderer plumbing.
