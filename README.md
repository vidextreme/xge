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
xgBase        → Public interfaces, POD types, reflection metadata  
xgPlatform    → Platform backends (Win32, SDL, etc.)  
xgCore        → Engine runtime (world, systems, update loop)  
Renderers     → DX12, Vulkan (modules)  
Script Hosts  → CoreCLR, NativeAOT (modules)  
Editor        → Managed editor runtime (CoreCLR)  
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

## 🌿 Dispatch, Flow & Affinity System

A new system enabling ScriptModules to define **their own internal execution flow** using a simple, intuitive API.

Each ScriptModule becomes a **self‑contained mini‑runtime** with:
- steps (phases)  
- ordering rules  
- parallel execution  
- thread affinity hints  
- internal transitions  
- message routing  

The engine handles:
- thread creation  
- dispatch queues  
- cross‑thread safety  
- deterministic ordering  
- flow execution  
- message delivery  

### ✨ High‑Level Flow Definition (User‑Facing API)

```cpp
void MyModule::DefineFlow(FlowBuilder& flow)
{
    flow.Step("Sense").Do(&MyModule::Sense);
    flow.Step("Think").Do(&MyModule::Think);
    flow.Step("Plan").Do(&MyModule::Plan);
    flow.Step("Act").Do(&MyModule::Act);

    flow.Order("Sense", "Think");
    flow.Order("Think", "Plan");
    flow.Order("Plan", "Act");

    flow.RunOn("Sense", Thread::Worker);
    flow.RunOn("Think", Thread::Worker);
    flow.RunOn("Plan", Thread::Pinned(0));
    flow.RunOn("Act", Thread::Main);
}
```

### 🧵 Thread Affinity (High‑Level)

- Thread::Main  
- Thread::Worker  
- Thread::Pinned(N)  
- Thread::Dedicated  

### 🔄 Internal Messaging & Transitions (TBD)

```cpp
flow.OnMessage<EnemySpotted>()
    .From("Sense")
    .To("Think");
```

```cpp
flow.Transition("Plan", "Act").AsImmediate();
```

### 🌳 Hierarchical Script Modules (HSM)

Integrates with:
- parent/child relationships  
- scoped communication  
- optional affinity inheritance  
- future hot‑reload boundaries  

### 🏷️ Attribute‑Based Grouping

#### C# Example
```csharp
using xg.Dispatch;

public class AIModule : ScriptModule
{
    [Dispatch("Sense")]
    public void ScanProximity()
    {
        // Gameplay logic
    }
}
```

#### C++ Example

```cpp
#include "xgDispatchAttributes.h"
#include "xgScriptModule.h"

class PlayerModule : public xg::ScriptModule
{
public:
    XG_Dispatch("Gameplay")
    void UpdateGameplay(float dt)
    {
        // Gameplay logic
    }
};
```

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
```
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
```


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
- Dispatch, Flow & Affinity system  
- HSM  
- thread affinity  
- attribute scheduling  
- messaging layer  
- editor integration  

## Mid‑Term
- hot reload  
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
