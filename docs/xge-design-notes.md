# XGE Design Notes

> **Status: design.** These notes describe intent and direction. Parts are implemented, parts are partial, and parts are planned. See the status table in the [README](../README.md) for what works today, and the [runtime architecture](xge-architecture.md), [SceneGraph](xge-scenegraph.md) and [components](xge-components.md) docs for the current implementation.

---

## SceneGraph — Scoped, Multi‑Purpose Graph System

### SceneGraph is a Subsystem
A `SceneGraph` is a first‑class **Subsystem**, which means:

- It is **scope‑based**
- It is **registered per ScriptModule**
- It is **resolved hierarchically** through the ScriptTree
- The **closest ancestor SceneGraph** is used when requested
- A ScriptNode can override its parent's SceneGraph by registering its own

This makes SceneGraphs modular, replaceable, and context‑specific.

### Global ActorID Generation
ActorIDs are generated globally by the engine's `ActorRegistry`.

- ActorIDs are **unique across the entire engine**
- ActorIDs are **not owned** by any SceneGraph
- ActorIDs allow the same logical actor to appear in multiple graphs

This enables multi‑representation and multi‑domain processing.

### Actors Can Exist in Multiple SceneGraphs
An `ActorID` can have **multiple representations**, one per SceneGraph:

- Transform hierarchy
- Physics bodies
- ECS entities
- Rendering nodes
- AI nodes
- Metadata nodes

Each SceneGraph stores its own representation of the same ActorID.

### SceneGraphs Are Purpose‑Specific
A SceneGraph is not a single "scene tree." It is a **domain‑specific graph** with a specialized purpose:

- **Physics SceneGraph** — rigid bodies, colliders, constraints
- **ECS SceneGraph** — entity/component relationships
- **Render SceneGraph** — drawable objects, materials, LODs
- **Transform SceneGraph** — parent/child transforms
- **Metadata SceneGraph** — editor properties, tags, names

Each SceneGraph defines its own storage, traversal, and rules.

### ScriptNodes Can Have Multiple SceneGraphs
A ScriptNode may register **multiple SceneGraphs** (via `RegisterSubsystemWithAlias`), each serving a different domain, for example a Physics, Render and Metadata graph. All are scoped to that ScriptNode and inherited by its children unless overridden.

As built so far, hierarchy is one tree graph, and per-actor data that systems iterate (transforms, script data structs) is not a graph of its own but a packed pool per data type in the root `ComponentDataRegistry`, keyed by the same ActorID. Transforms are the engine-owned `"xg.Transform"` pool, with world matrices derived from the tree. See [Native component data](xge-components2.md).

---

## Messenger and Routes

Messenger delivers `ScriptMessage` objects across the ScriptTree using explicit `Route` objects. Routing is handled by ScriptTree; encoding and decoding are handled by the Codec Registry.

```cpp
Messenger* messenger = engine->GetSubsystem<Messenger>(this);

messenger->Send(msg, Route::Direct("game.demo"));      // one module
messenger->Send(msg, Route::ChildrenOf("editor.kernel")); // its subtree (optional depth limit)
messenger->Send(msg, Route::ParentOf("game.demo"));    // bubble upward (optional depth limit)
messenger->Send(msg, Route::SiblingsOf("editor"));     // lateral
messenger->Send(msg, Route::Filtered([](ScriptModule* m) {
    return std::strcmp(m->GetId(), "game.demo") == 0;
}));                                                   // predicate (function pointer)
messenger->SendToAll(msg);                             // global broadcast
```

---

## Codec Registry — Dynamic Message Encoding

The CodecRegistry encodes and decodes all `ScriptMessage` payloads. It supports both **native C++ types** and **dynamic types** (`DynamicObject`), using reflection metadata (`TypeSchema`) from the TypeRegistry. Each script backend has its own registry (`GetCodecRegistry(ScriptBackendType)`).

It provides:

- native encoders/decoders, registered per type name (with a `"*"` generic JSON or binary fallback)
- dynamic JSON encode/decode for `DynamicObject`
- unified `Encode` / `Decode` entry points

```cpp
registry->RegisterEncoder("PlayerDamaged", Encode_PlayerDamaged);
registry->RegisterDecoder("PlayerDamaged", Decode_PlayerDamaged);

bool Encode(const char* typeName, const void* src, const TypeSchema* schema, ScriptMessage& outMessage) const;
bool Decode(const char* typeName, const ScriptMessage& message, const TypeSchema* schema, void* dst) const;
```

Dynamic types do not register encoders; they fall back to `EncodeDynamic` / `DecodeDynamic`. This allows editor inspection, flexible message formats, runtime‑generated message types and, later, network serialization.

Flow:

1. The sender encodes with `CodecRegistry::Encode`
2. ScriptTree routes the encoded message
3. The receiving ScriptModule decodes with `CodecRegistry::Decode`
4. The decoded object is passed to the handler (`OnMessage`)

---

## Dispatch Groups — Thread Affinity (not wired yet)

Dispatch groups define execution domains:

- Input
- Gameplay
- Animation
- RenderPrep
- Async
- Custom

Modules declare dispatch groups via attributes (C#) or macros (C++), and the engine handles scheduling and thread affinity. Not wired yet: every module still updates on the main thread. Parallel work goes through the `JobSystem` (`xgBase/public/xgJobs.h`), which replaced the old dispatch system: worker and main-thread jobs with dependencies, used by asset builds, resource loads and large data updates.

---

## Scripting Runtimes

### CoreCLR (editor mode)
- hot reload (planned)
- reflection
- dynamic assembly loading
- debugging
- full .NET ecosystem

### NativeAOT (runtime)
- zero dependencies
- instant startup
- no JIT
- native debugging
- predictable performance

Both runtimes sit behind the same `ScriptModule` interface, so a module can move between them. The interface is backend‑agnostic and could host other languages (Lua, Squirrel, etc.).

---

## Rendering

Two backends, `xgRendererDX12` and `xgRendererVulkan` (Vulkan 1.3, dynamic rendering, loaded through volk), implement the same `xg::Renderer` interface and have the same features. They link xgBase only and are picked by `RendererModule` in the config.

- **UI:** `xgImUIKit`'s render backend turns ImGui output into the engine-neutral `UIDrawList` and texture calls on `xg::UIRenderTarget` (`xgBase/public/xgUIDraw.h`). `xg::Renderer` derives from it and draws the lists with its own small UI pipeline. No ImGui code lives in a renderer and no graphics API code lives in the UI kit.
- **Multi-viewport:** floating UI windows get their own swap chain through `CreateUISurface`.
- **Render views:** `CreateRenderView` gives offscreen color and depth targets (linear color through an sRGB target) that the UI shows as textures. Scene and Game panels and editor previews use them; `ReadRenderView` reads one back (thumbnails).
- **Content:** BC1/3/4/5 and RGBA8 textures with mips, meshes with a 52-byte lit vertex, project surface shaders and materials (`CreateShader`, `CreateMaterial`), scene draws shared by every view. One directional light and ambient for now. See [Data and assets](xge-data.md).
- **No renderer:** with `RendererModule` empty, xgPlatform's SDL GPU presenter implements the UI part of the interface, without 3D views.

Planned: a FrameGraph and GPU resource abstractions, shadows, post-processing, instancing.

---

## Network Serialization (planned)

### Reflection‑Driven Replication
- enumerate fields
- identify replicated fields
- serialize only required fields
- apply deltas deterministically

### Delta Compression
- compare previous vs current state
- emit only changed fields

### Bit‑Packed Serialization
- primitive packing
- boolean compression
- enum packing
- optional string hashing

### Replication Policies

```cpp
XG_FIELD(meta=Replicated)
XG_FIELD(meta=Replicated | Interpolated)
```

### RPCs (future)
- method lookup
- parameter serialization
- stub generation
