![xfg xge](docs/xge-logo.png#gh-dark-mode-only)
![xfg xge](docs/xge-logo-white.png#gh-dark-mode-only#gh-light-mode-only)

# 🎮 XGE — Cross‑Game Engine

A work‑in‑progress, modular C++20 engine focused on **foundational architecture**, **technical design**, and **clean systems engineering** rather than commercial features or production‑ready tooling.

XGE is an exploration of modern engine architecture: reflection‑driven data, modular runtime systems, dual‑runtime scripting (CoreCLR + NativeAOT), and strict layering. The goal is to build a **solid, extensible foundation** that can grow into a full engine over time.

The project emphasizes:
- strict layering  
- reflection‑driven data  
- modular runtime systems  
- dual scripting runtimes (CoreCLR + NativeAOT, extendable to support other language like Squirrel, Lua, etc)
- editor integration  
- clean, STL‑free public API surfaces  


---

# 🔗 Quick Jump

- [Overview](#-overview)  
- [Design Philosophy](#-design-philosophy)  
- [Architecture](#-architecture)  
- [Current Engine Capabilities](#-current-engine-capabilities)  
- [Current Work‑In‑Progress](#-current-workinprogress-core-architecture-initiative)  
- [Rendering Architecture](#-rendering-architecture)  
- [Network Serialization (Planned)](#-network-serialization-planned)  
- [Scripting Architecture](#-scripting-architecture)  
- [Repository Structure](#-repository-structure)  
- [Reflection System](#-reflection-system)  
- [JSON Serialization](#-json-serialization)  
- [Runtime Flow](#-runtime-flow)  
- [Roadmap](#-roadmap)  
- [License](#-license)  

---

# 📘 Overview

XGE is a modular, reflection‑driven, cross‑platform engine foundation designed for long‑term scalability.  
It prioritizes **clean architecture**, **strict layering**, and **runtime modularity** over monolithic engine design.

XGE is built to support:

- multiple scripting runtimes  
- multiple rendering backends  
- editor/runtime separation  
- deterministic data pipelines  
- future network replication  
- future visual scripting  
- future hot‑reload workflows  

---

# 🧭 Design Philosophy

### **Strict Layering**
No platform code in core.  
No renderer code in scripting.  
No STL in public headers.

### **Reflection Everywhere**
Tree‑sitter generates metadata used for:
- JSON serialization  
- editor property panels  
- network replication (future)  
- deterministic data pipelines  

### **Modularity First**
Everything is a module:
- renderer  
- scripting runtime  
- editor kernel  
- future physics/audio/gameplay  

### **Runtime Flexibility**
Load modules at runtime.  
Swap renderer or scripting backend without recompiling the engine.

---

# 🏛️ Architecture

```
xgBase        → Base types, macros, platform detection  
xgPlatform    → Platform backends (Win32, SDL)  
xgCore        → Engine runtime  
Renderers     → DX12, Vulkan (modules)  
Script Hosts  → CoreCLR, NativeAOT (modules)  
Editor        → Native editor kernel + managed editor runtime  
```

---

# 🚀 Current Engine Capabilities

## Core Systems
- Modular engine with dynamic module loading  
- Reflection‑driven configuration  
- JSON serialization via reflection metadata  
- Strict layering  
- STL‑free public API surface  

## Platform Layer
- SDL3 window backend  
- Win32 file backend  
- Cross‑platform module loader  
- Event system (SDL → `xgEvent`)  

## Rendering
- DirectX 12 renderer module  
- Swap chain, command queue, render targets  
- Clear + present rendering loop  

## Scripting
- Dual‑runtime scripting: CoreCLR + NativeAOT  
- Unified `ScriptModule` interface  
- Managed editor runtime  

## Editor
- Native editor kernel module  
- Full ImGui integration  
- Docking + multi‑viewport  
- Viewport, Outliner, Properties, Timeline, Assets panels  

---

# 🧩 Current Work‑In‑Progress (Core Architecture Initiative)

## 🌿 HSM — Hierarchical Script Modules

XGE’s runtime is built around **HSM (Hierarchical Script Modules)** — a structured tree of ScriptModules that defines ownership, routing scope, and execution domains.

The system consists of:

- ~~ScriptTree — hierarchical tree of ScriptModules (HSM)~~ **(DONE)**
- ~~Messenger — message routing backbone~~ **(DONE)**
- ~~Route Traversal — parent/child/sibling routing rules~~ **(DONE)**
- ~~Codec Registry — dynamic message encoding/decoding~~ **(DONE)**
- Dispatch Groups — thread‑affinity execution domains **(IN PROGRESS)**


Together, these form XGE’s core runtime architecture.

---

## 🌳 ScriptTree — Hierarchical Script Modules (HSM) **(DONE)**

ScriptTree stores every ScriptModule in a strict hierarchy:

- parent → child relationships  
- sibling traversal  
- domain grouping (Main, Worker, Pinned, Dedicated)  
- deterministic update order  
- scoped message routing  

Each ScriptModule is a node in the tree, enabling:

- hierarchical updates  
- scoped messaging  
- clean separation of systems  
- future hot‑reload boundaries  

---

## 📡 Messenger — Central Message Router **(DONE)**

Messenger delivers `ScriptMessage` objects across the ScriptTree using explicit `Route` objects.  
Routing is deterministic and handled by ScriptTree; encoding/decoding is handled by the Codec Registry.

### C++ Example — Sending Messages

Messages are delivered by constructing a `Route` and passing it to `Messenger::Send`.

```cpp
ScriptMessage msg{ PlayerDamaged{ .Amount = 10 } };

// Direct message to a specific ScriptModule
Messenger->Send(msg, Route::Direct("Player"));

// Broadcast to all children of a ScriptModule
Messenger->Send(msg, Route::ChildrenOf("UIRoot"));

// Bubble upward to parents
Messenger->Send(msg, Route::ParentOf("Inventory"));

// Send to siblings
Messenger->Send(msg, Route::SiblingsOf("EnemyAI"));

// Filtered routing (predicate receives ScriptModule*)
Messenger->Send(msg, Route::Filtered([](ScriptModule* m) {
    return m->GetId() == "HUD";
}));

// Global broadcast
Messenger->SendToAll(msg);
```


---

## 🛣️ Route Traversal — How Messages Move Through the Tree **(DONE)**

Messages can be routed using simple rules:

- ToParent — bubble upward  
- ToChildren — broadcast downward  
- ToSiblings — lateral routing  
- ToDomain — cross‑thread dispatch  
- ToSpecificNode — direct targeting  

Example:

```cpp
Messenger->Send(msg, Route::ToParent);
Messenger->Broadcast(msg, Route::ToChildren);
Messenger->Send(msg, Route::ToDomain(Thread::Worker));
```

---

## 🔐 Codec Registry — Dynamic Message Encoding **(DONE)**

The CodecRegistry is responsible for encoding and decoding all `ScriptMessage` payloads.  
It supports both **native C++ types** and **dynamic types** (`DynamicObject`), using reflection metadata (`TypeSchema`) generated by the TypeRegistry.

CodecRegistry provides:

- native encoders/decoders (registered per typeName)
- dynamic JSON encode/decode for `DynamicObject`
- lookup of encoder/decoder functions
- unified public `Encode` / `Decode` entry points
- integration with Messenger and ScriptTree

### Native Type Encoding

Native types register their encoder/decoder functions:

```cpp
registry.RegisterEncoder("PlayerDamaged", Encode_PlayerDamaged);
registry.RegisterDecoder("PlayerDamaged", Decode_PlayerDamaged);
```

When Messenger delivers a message, CodecRegistry selects the correct encoder based on the message’s `typeName`.

### Dynamic Type Encoding

Dynamic types (`DynamicObject`) do not register encoders.  
CodecRegistry automatically falls back to JSON‑based dynamic encoding:

- `EncodeDynamic(obj, outMessage)`
- `DecodeDynamic(message, schema, outObj)`

This allows:

- editor inspection
- flexible message formats
- runtime‑generated message types
- future network serialization

### Public Encode/Decode API

All message encoding flows through two entry points:

```cpp
bool Encode(const char* typeName,
            const void* src,
            const TypeSchema* schema,
            ScriptMessage& outMessage) const;

bool Decode(const char* typeName,
            const ScriptMessage& message,
            const TypeSchema* schema,
            void* dst) const;
```

These functions:

- select native or dynamic codec
- serialize the payload into `ScriptMessage`
- deserialize payload into native struct or `DynamicObject`

### Integration with Messenger

Messenger does not serialize messages itself.  
Instead, before delivery:

1. Messenger calls `CodecRegistry::Encode`
2. ScriptTree routes the encoded message
3. Receiving ScriptModule calls `CodecRegistry::Decode`
4. The decoded object is passed to the handler method

This ensures:

- consistent message formats
- reflection‑driven serialization
- safe cross‑module communication
- future network replication compatibility



## 🧵 Dispatch Groups — Thread Affinity **(IN PROGRESS)**

Dispatch groups define execution domains:

- Input  
- Gameplay  
- Animation  
- RenderPrep  
- Async  
- Custom  

Modules declare dispatch groups via attributes (C#) or macros (C++).  
The engine handles scheduling and thread affinity.

---

## Summary

The HSM + ScriptTree + Messenger + Route Traversal + Codec Registry + Dispatch Groups form XGE’s core runtime architecture.

They provide:

- deterministic module updates  
- safe multi‑threaded messaging  
- structured message encoding  
- hierarchical routing  
- clean separation of systems  
- future‑proof foundations for AI, gameplay, editor tooling, and networking  

---

# 🎨 Rendering Architecture

## DirectX 12 (Current)
Validates:
- command queue + allocator  
- descriptor heaps  
- swap chain  
- GPU/CPU sync  
- render targets  

## Vulkan (Planned)
Will validate:
- cross‑platform rendering  
- explicit sync  
- descriptor sets  
- pipeline layout abstraction  
- multi‑queue support  

Both backends will share:
- unified renderer interface  
- FrameGraph  
- resource abstractions  

---

# 🌐 Network Serialization (Planned)

## Reflection‑Driven Replication
- enumerate fields  
- identify replicated fields  
- serialize only required fields  
- apply deltas deterministically  

## Delta Compression
- compare previous vs current state  
- emit only changed fields  

## Bit‑Packed Serialization
- primitive packing  
- boolean compression  
- enum packing  
- optional string hashing  

## Replication Policies

```cpp
XG_FIELD(meta=Replicated)
XG_FIELD(meta=Replicated | Interpolated)
```

## RPCs (Future)
- method lookup  
- parameter serialization  
- stub generation  

---

# 🧩 Scripting Architecture

## CoreCLR (Editor Mode)
- hot reload  
- reflection  
- dynamic assembly loading  
- debugging  
- full .NET ecosystem  

## NativeAOT (Runtime)
- zero dependencies  
- instant startup  
- no JIT  
- native debugging  
- predictable performance  

Both runtimes implement:

```cpp
class ScriptModule {
public:
    virtual bool Init(ScriptEngine* engine) = 0;
    virtual void Update(float dt) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsValid() const = 0;
};
```

---

# 📂 Repository Structure

"
XGE/
├── docs/                     # Documentation, diagrams, design notes
├── gameroot/                 # Runtime deployment root
│   ├── bin/                  # Native binaries (engine + modules)
│   ├── editor/               # Editor assets (fonts, UI resources)
│   └── coreclr/              # CoreCLR runtime + managed assemblies
│       └── Microsoft.NETCore.App/
│           └── 10.0.8/
├── xgEngine/                 # Engine source tree
│   ├── 3rdparty/             # External libraries (SDL, ImGui, etc.)
│   ├── tools/                # Engine tools (reflection generator, etc.)
│   │   └── xgReflectGen/
│   ├── xgBase/               # Base types, macros, platform detection
│   ├── xgCommon/             # Shared utilities (native + C#)
│   ├── xgCore/               # Core engine runtime
│   ├── xgPlatform/           # Platform backends (Win32, SDL)
│   ├── xgRendererDX12/       # DirectX 12 renderer module
│   ├── xgScriptCoreCLR/      # CoreCLR scripting host module
│   ├── xgScriptNative/       # NativeAOT scripting host module
│   ├── xgEditorKernel/       # Native editor kernel module
│   ├── xgUtility/            # Utility layer (platform helpers, etc.)
│   │   └── platform/
│   └── xge/                  # Engine executable (launcher)
"

---

# 🧬 Reflection System

- Tree‑sitter C++  
- generates `.generated.h`  
- produces `RawFieldInfo` arrays  
- supports bool/int/float/const char*  
- uses `offsetof()`  

Example:

```cpp
XG_SERIALIZABLE()
struct EngineConfig {
    XG_FIELD()
    const char* RendererModule = nullptr;
};
```

---

# 📄 JSON Serialization

- custom wrapper over nlohmann::json  
- STL only in `.cpp`  
- reflection‑driven serialization  

---

# 🔁 Runtime Flow

## Startup
- load config  
- load renderer module  
- load scripting module  
- initialize systems  

## Main Loop
- fixed update  
- variable rendering  
- ScriptHost update  
- module updates  

## Shutdown
- ScriptHost shutdown  
- renderer shutdown  
- platform shutdown  

---

# 🛣️ Roadmap

## High Priority
- Dispatch Groups  
- HSM tooling  
- messaging inspector  
- editor integration  
- hot reload boundaries  

## Mid‑Term
- scene graph  
- component system  
- property inspector  
- enum/array reflection  
- FrameGraph  
- GPU‑driven rendering  

## Long‑Term
- full editor  
- shader graph  
- hot‑reloadable C# gameplay  
- network replication  
- RPC system  
- job system  
- Vulkan parity  

---

# 📝 License
MIT License.
