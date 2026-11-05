# XGE Design Notes

> **Status: design.** These notes describe intent and direction. Parts are implemented, parts are partial, and parts are planned. See the status table in the [README](../README.md) for what works today, and the [runtime architecture](xge-architecture.md), [SceneGraph](xge-scenegraph.md) and [components](xge-components.md) docs for the current implementation.

---

## SceneGraph — Scoped, Multi‑Purpose Graph System

### SceneGraph is a System
A `SceneGraph` is a first‑class **System**, which means:

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
A ScriptNode may register **multiple SceneGraphs** (via `RegisterSystemWithAlias`), each serving a different domain, for example a Transform, Physics, Render and Metadata graph. All are scoped to that ScriptNode and inherited by its children unless overridden.

---

## Messenger and Routes

Messenger delivers `ScriptMessage` objects across the ScriptTree using explicit `Route` objects. Routing is handled by ScriptTree; encoding and decoding are handled by the Codec Registry.

```cpp
Messenger* messenger = engine->GetSystem<Messenger>(this);

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

Modules declare dispatch groups via attributes (C#) or macros (C++), and the engine handles scheduling and thread affinity. `ThreadDomain` and a `DispatchSystem` exist, but every module currently runs on the main thread.

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

## Rendering (very rough / planned)

### DirectX 12 (very rough)
The `xgRendererDX12` module creates a device, command queue, swap chain and render targets, and clears and presents. It is not yet called from the main loop; the editor draws through SDL GPU. The next step is to let the editor UI draw through the engine renderer while keeping all DX12 code inside its module.

Intended to validate:
- command queue + allocator
- descriptor heaps
- swap chain
- GPU/CPU sync
- render targets

### Vulkan (planned)
Will validate:
- cross‑platform rendering
- explicit sync
- descriptor sets
- pipeline layout abstraction
- multi‑queue support

Both backends will share a unified renderer interface, a FrameGraph and resource abstractions.

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
