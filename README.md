![xfg xge](docs/xge-logo.png#gh-dark-mode-only)
![xfg xge](docs/xge-logo-white.png#gh-dark-mode-only#gh-light-mode-only)

# 🎮 XGE — Cross‑Game Engine

A work‑in‑progress, modular C++20 engine focused on **foundational architecture**, **technical design**, and **clean systems engineering** rather than commercial features or production‑ready tooling.

XGE is an exploration of modern engine architecture: reflection‑driven data, modular runtime systems, dual‑runtime scripting (CoreCLR + NativeAOT), and strict layering. The goal is to build a **solid, extensible foundation** that can grow into a full engine over time.

---

## 📘 Overview

**XGE** is a modern C++20 game engine built around three pillars:

- **Modularity** — every major subsystem (renderer, scripting runtime, editor) is a dynamically loaded module.
- **Reflection‑Driven Data** — the engine uses a custom Tree‑sitter–based reflection generator to produce metadata for serialization, editor tooling, and configuration.
- **Strict Layering** — platform, core, renderer, and scripting runtimes are cleanly separated with zero STL exposure in public headers.

XGE is designed for both runtime execution and editor workflows, with deterministic behavior, clean abstractions, and long‑term maintainability.

---

## 🧭 Design Philosophy

### Platform‑Agnostic Core
The engine core (`xgCore`) contains no platform code, no renderer code, and no scripting runtime.  
It consumes abstract interfaces and streams, making it portable and testable.

**xgCore** is isolated from OS APIs, graphics APIs, and scripting runtimes.
All integration happens through dynamically loaded modules.

### Strict Layering
```
xgBase        → Public interfaces, POD types, reflection metadata
xgPlatform    → Platform backends (Win32, SDL, etc.)
xgCore        → Engine runtime (world, systems, update loop)
Renderers     → DX12, Vulkan (modules)
Script Hosts  → CoreCLR, NativeAOT (modules)
Editor        → Managed editor runtime (CoreCLR)
```

### Reflection Everywhere
XGE uses a custom reflection generator (`xgReflectGen`) built on **Tree‑sitter**.  
It produces `.generated.h` files containing:

- Field metadata  
- Offsets  
- Type names  
- Serialization descriptors  

This powers:

- JSON serialization  
- Editor property panels  
- Hot‑reloadable configuration  
- Deterministic data pipelines  

## 💡 Inspiration Behind xgReflectGen

`xgReflectGen` is inspired by several proven systems, distilled into a lightweight, modern tool:

- **Unreal Header Tool (UHT)** — annotation‑driven metadata generation for reflection and serialization.  
- **Unity IL2CPP** — deterministic metadata tables and codegen pipelines.  
- **Rust Procedural Macros** — attribute‑based code generation with minimal boilerplate.  
- **Tree‑sitter** — fast, robust C++ parsing without relying on fragile regex or compiler plugins.

The result is a **small, predictable, engine‑friendly reflection pipeline** tailored for XGE’s modular architecture.

---

### **Procedural Macros**
Macros generate code at compile time based on annotated structs.  
xgReflectGen mirrors this with:

- `XG_SERIALIZABLE()`  
- `XG_FIELD(meta="data")`  

### Deterministic, Stream‑Based IO
All serialization uses the engine’s `Stream` abstraction:

- FileStream  
- MemoryStream  
- NetworkStream  

This keeps the engine portable and testable.

---

## 🎨 Rendering Architecture  
### DirectX 12 (Current) — Vulkan (Planned)

XGE’s renderer layer is fully modular. Each backend is a dynamically loaded module implementing the same abstract interface.

### DirectX 12 Renderer (xgRendererDX12)
The DX12 backend is the **current primary renderer**, designed to validate:

- Command queue + allocator model  
- Descriptor heaps  
- Swap chain management  
- GPU/CPU synchronization  
- Render target management  
- Future FrameGraph integration  

It serves as the baseline for the engine’s rendering architecture.

### Vulkan Renderer (Planned)
A Vulkan backend is planned to validate:

- Cross‑platform rendering  
- Explicit synchronization  
- Descriptor set model  
- Pipeline layout abstraction  
- Multi‑queue support  
- GPU‑driven workflows  

The DX12 and Vulkan backends will share:

- A unified renderer interface  
- A common FrameGraph  
- Shared resource abstractions (textures, buffers, pipelines)  

This ensures the engine remains renderer‑agnostic and portable.

---

## 🌐 Network Serialization (Planned)
XGE’s reflection system will be extended to support **network serialization**, enabling deterministic, bandwidth‑efficient replication of engine state.

Planned features:

### Reflection‑Driven Replication
Reflection metadata will allow the engine to:

- Enumerate fields at runtime  
- Identify which fields are network‑replicated  
- Serialize only the required fields  
- Apply deltas deterministically  

### Delta Compression
Using reflection metadata, the engine can:

- Compare previous vs current state  
- Emit only changed fields  
- Encode changes in a compact binary format  

### Bit‑Packed Serialization
Inspired by engines like Unreal:

- Primitive types packed into minimal bits  
- Booleans compressed  
- Enums encoded as small integers  
- Strings optionally hashed  

### Replication Policies
Fields will support metadata such as:

```cpp
XG_FIELD(meta=Replicated)
XG_FIELD(meta=Replicated | Interpolated)
XG_FIELD()
```

### Deterministic Apply
Incoming network data will be applied using:

- Field offsets  
- Type metadata  
- Reflection‑driven deserialization  

This ensures:

- No pointer‑to‑member hacks  
- No virtual dispatch  
- No per‑type boilerplate  

### Future: RPCs via Reflection
Reflection metadata will also enable:

- RPC method lookup  
- Parameter serialization  
- Automatic stub generation  

This keeps the networking layer **engine‑agnostic and data‑driven**, just like the rest of XGE.

---

## 🧩 Scripting Architecture  
### CoreCLR for Editor — NativeAOT for Runtime

XGE uses a **dual‑runtime scripting model**:

---

### 🖥️ CoreCLR (Editor Mode)

The editor runs on **CoreCLR**, the full .NET runtime.

Benefits:
- Hot reload  
- Full reflection  
- Dynamic assembly loading  
- Rich debugging  
- Full .NET ecosystem  

Editor assemblies:
```csharp
Editor.CoreCLR.dll
```

Loaded via:
```csharp
engine.AddScriptModule("editor", "Editor.CoreCLR.dll");
```

---

### ⚡ NativeAOT (Game Runtime)

Shipping builds use **NativeAOT**, a fully ahead‑of‑time compiled runtime.

Benefits:
- Zero dependencies (no CoreCLR folder)
- Instant startup
- No JIT cost
- Native debugging symbols
- Predictable performance

Runtime assemblies:
```csharp
Game.NativeAOT.dll
```

Both runtimes implement the same interface:

```csharp
class ScriptModule
{
public:
    virtual bool Init(Engine* engine) = 0;
    virtual void Update(float dt) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsValid() const = 0;
};
```

This gives XGE:
- Editor flexibility  
- Runtime performance  
- A unified scripting API  

---

## Repository Structure

```
XGE/
│
├── xgEngine/                 # Main engine source
│   ├── xgBase/               # Public interfaces, POD types, reflection metadata
│   ├── xgCore/               # Engine runtime
│   ├── xgPlatform/           # Platform layer
│   ├── xgRendererDX12/       # DirectX 12 renderer module
│   ├── xgScriptCoreCLR/      # CoreCLR scripting runtime module
│   ├── Editor.CoreCLR/       # Managed editor runtime
│   ├── 3rdparty/             # JSON, SDL3, TreeSitter
│   ├── Generated/            # Auto‑generated reflection headers
│   ├── tools/
│   │   └── xgReflectGen/     # Reflection generator tool
│   └── xge/                  # Engine executable
│
└── gameroot/                 # Runtime root (bin, CoreCLR runtime, assets)
    ├── bin/
    └── coreclr/
        └── Microsoft.NETCore.App/
```

---

## Reflection System

- Built using **Tree‑sitter C++**
- Generates `.generated.h` files
- Produces `RawFieldInfo` arrays
- Supports:
  - `bool`
  - `int`
  - `float`
  - `const char*`
- Uses `offsetof()` for deterministic field access
- No STL in public headers

Example:

```cpp
XG_SERIALIZABLE()
struct EngineConfig
{

    XG_FIELD()
    const char* RendererModule = nullptr;
};
```

Generated:

```cpp
{
    "RendererModule", "RendererModule",
    offsetof(EngineConfig, RendererModule),
    "const char*"
}
```

---

## JSON Serialization

- Custom `xg::Json` wrapper around `nlohmann::json`
- STL only inside `.cpp`
- Public API returns `const char*`
- Supports:
  - Save to file
  - Load from file
  - Reflection‑driven serialization/deserialization

Example:

```json
{
    "RendererModule": "xgRendererDX12.dll"
}
```

---

## Runtime Flow

### Startup
- Load gameconfig.json  
- Load renderer module  
- Load scripting module  
- Initialize systems  

### Main Loop
- Fixed timestep update  
- Variable timestep rendering  
- ScriptHost update  
- Module updates  

### Shutdown
- ScriptHost shutdown  
- Renderer shutdown  
- Platform shutdown  

---

## Roadmap

### Short‑term
- Array and vector reflection  
- Enum reflection  
- Nested struct serialization  
- Editor property inspector  

### Mid‑term
- Scene graph  
- Component system  
- Physics integration  
- GPU‑driven rendering  

### Long‑term
- Full editor  
- Node‑based shader graph  
- Hot‑reloadable C# gameplay layer  
- Multi‑threaded job system  

---

## License
This project is released under the MIT License.
