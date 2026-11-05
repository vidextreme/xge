![xfg xge](docs/xge-logo.png#gh-dark-mode-only)
![xfg xge](docs/xge-logo-white.png#gh-light-mode-only)

# 🎮 XGE — Cross-Game Engine

A work-in-progress, modular C++20 engine focused on **foundational architecture** and **clean systems engineering** rather than production features. XGE explores a hierarchical module runtime, reflection-driven data, and dual-runtime C# scripting (CoreCLR + NativeAOT) behind a stable C ABI.

> **Work in progress.** Windows only for now (MSVC, .NET 10). Expect APIs to change.

## Contents

- [Highlights](#highlights)
- [Status](#status)
- [Getting started](#getting-started)
- [Architecture](#architecture)
- [A taste of the API](#a-taste-of-the-api)
- [Repository structure](#repository-structure)
- [Documentation](#documentation)
- [Roadmap](#roadmap)
- [License](#license)

---

## Highlights

- **HSM (Hierarchical Script Modules):** every module (native DLL, CoreCLR or NativeAOT) is a node in one ScriptTree with deterministic parent → child updates.
- **Scoped systems:** services are registered per module and resolved by walking up the tree, so children inherit or override them.
- **Dual C# scripting:** CoreCLR for the editor, NativeAOT for the runtime, both behind the same `ScriptModule` interface.
- **Reflection codegen:** a Tree-sitter tool (`xgReflectGen`) generates type schemas from `XG_SERIALIZABLE` / `XG_FIELD` / `XG_ENUM` markers, used for JSON, message codecs and C# enums.
- **Editor shell:** Dear ImGui (docking + multi-viewport) on SDL3.
- **Stable ABI:** cross-DLL interfaces are pure virtual and append-only, and only C types and function pointers cross module boundaries.

---

## Status

| Area | Status | Notes |
|---|---|---|
| ScriptTree / HSM | 🟢 Working | Native, CoreCLR and NativeAOT modules load into one tree. |
| Scoped systems | 🟢 Working | Lookup walks up the tree; the closest ancestor wins. |
| Messenger / routes | 🟢 Working | `Direct`, `ChildrenOf`, `ParentOf`, `SiblingsOf`, `Filtered`, `SendToAll`. The managed route API is still incomplete. |
| Codec registry | 🟡 Partial | JSON and binary codecs work for numbers and bools; string fields need work. |
| Reflection (`xgReflectGen`) | 🟡 Partial | bool / int / uint / float / `const char*` fields and enums. No arrays or nested types yet. |
| Config (`gameconfig.json`) | 🟢 Working | Loaded by `xge` through reflection into `EngineConfig`. |
| Actors & components (C#) | 🟡 Partial | `Actor.Create`, `AddComponent<T>` / `GetComponent<T>` / `RemoveComponent<T>`, `Destroy`. Update logic uses **CRTP**: a component implements `IComponentUpdate<T>` with a `static abstract OnUpdate(T, float)`, and one `ComponentUpdateGroup<T>` per type calls it through a cached function pointer (no virtual call, boxing or reflection per frame). Removals are deferred until after the pass. Driven every frame by `ActorRegistry::UpdateActors(dt)` in both CoreCLR and NativeAOT modules. Data components (`ComponentData<TData>`, `IComponentDataUpdate<TData>`) and native component storage are prototypes. |
| SceneGraph (native) | 🟡 Partial | ActorIDs are global 64-bit IDs; an actor can belong to several graphs. The tree graph works; transforms are not propagated yet. |
| Logging | 🟢 Working | Engine-owned logger shared by every module; Unity-style C# `Debug` (`Log`, `LogWarning`, `LogError`, …). |
| Math + C# interop | 🟢 Working | Vectors, quaternions, matrices, shapes, geometry queries, easing, random, color. Native and managed test suites in `verify/`. |
| Editor UI | 🟡 Prototype | Docking layout with static panels (non-functional). Toggle with `EditorUI` in `gameconfig.json`. |
| Renderer (DX12) | 🟠 Very rough | Device, swap chain, resize, and a UI pass that draws the editor through a generic `xg::Renderer` UI API (no ImGui in the renderer, no DX12 in the editor). No 3D rendering yet; multi-viewport is SDL GPU only. |
| Dispatch groups / threading | 🟡 Partial | Thread domains and a dispatch system exist but are not wired; everything runs on the main thread. |
| Vulkan | ⚪ Planned | |
| Network replication | ⚪ Planned | |
| Hot reload | ⚪ Planned | |

---

## Getting started

### Requirements

- Windows 10/11 x64
- Visual Studio 2026 with the C++ desktop workload (toolset v145)
- .NET 10 SDK

### Build

Build the full solution `xgEngine/XGE.slnx` (Debug x64) from Visual Studio, or from PowerShell:

```powershell
cd xgEngine
& (vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe") XGE.slnx -restore -m -p:Configuration=Debug -p:Platform=x64 -v:minimal -nologo
```

The NativeAOT publish of `GameDemoModule` needs `C:\Program Files (x86)\Microsoft Visual Studio\Installer` (for `vswhere.exe`) on `PATH` when MSBuild is started from a plain shell.

Binaries go to `gameroot/bin`.

### Run

Run `xge.exe` from `gameroot/bin`. It reads `gameroot/bin/gameconfig.json`:

```json
{
    "RendererModule": "",
    "EditorUI": true
}
```

| Key | Meaning |
|---|---|
| `RendererModule` | Renderer DLL to load, for example `xgRendererDX12`. When set, the editor UI draws through it; when empty, the editor draws through SDL GPU (with multi-viewport). |
| `EditorUI` | `true` shows the editor UI prototype; `false` shows an empty ImGui dockspace with the OS window frame. |

---

## Architecture

```
xgBase         → Base types, macros, logging, JSON, reflection, math (static lib)
xgPlatform     → Platform backends (SDL3 window, Win32 files)
xgCore         → Engine runtime: ScriptTree, systems, Messenger, codecs, ActorRegistry
Renderers      → DX12 (very rough), Vulkan (planned)
Script hosts   → CoreCLR (hostfxr), Native + NativeAOT
Kernels        → xgEditorKernel (editor UI), xgGameKernel (scene graphs, actor exports)
Kits           → xgSharedKit / xgEditorKit (native), xgSharedKit.Managed (C#)
xge            → Launcher
```

### Module tree at runtime

```
root
└── editor.kernel   xgEditorKernel.dll     (native)
    └── game.kernel xgGameKernel.dll       (native)
        ├── editor      xgEditor.CoreCLR.dll  (CoreCLR)
        └── game.demo   GameDemoModule.dll    (NativeAOT)
```

### Frame loop

`Engine::Run` measures the frame delta (steady clock, capped at 0.25 s), polls window events, updates the ScriptTree (every module, parent before child), then updates actors and their components through `ActorRegistry::UpdateActors(dt)`.

---

## A taste of the API

### Systems

```cpp
struct SceneGraphFactory : public System {
    XG_DECLARE_BASE_SYSTEM(SceneGraphFactory)
};

class SceneGraphFactoryImpl : public SceneGraphFactory {
    XG_DECLARE_SYSTEM(SceneGraphFactoryImpl, SceneGraphFactory)
};

engine->RegisterSystem(module, new SceneGraphFactoryImpl());       // scoped to module and its children
auto* factory = engine->GetSystem<SceneGraphFactory>(module);      // closest ancestor wins
```

### Messages

```cpp
Messenger* messenger = engine->GetSystem<Messenger>(this);
messenger->Send(msg, Route::Direct("game.demo"));
messenger->Send(msg, Route::ChildrenOf("editor.kernel"));
messenger->SendToAll(msg);
```

### Reflection

```cpp
XG_SERIALIZABLE()
struct EngineConfig
{
    XG_DECLARE_TYPE_NAME(EngineConfig);

    XG_FIELD()
    const char* RendererModule = nullptr;

    XG_FIELD()
    bool EditorUI = true;
};

const TypeSchema* schema = engine.GetTypeRegistry()->Get(EngineConfig::TypeName);
```

### C# components and logging

```csharp
public class EnemyComponent : IComponentUpdate<EnemyComponent>
{
    public float ElapsedTime;
    public static void OnUpdate(EnemyComponent c, float dt) => c.ElapsedTime += dt;
}

var enemy = Actor.Create("enemy").AddComponent<EnemyComponent>();
Debug.Log("enemy spawned");
```

`OnUpdate` is static and resolved through CRTP, so each component type is updated by a direct call through one function pointer per type. That keeps it devirtualized and NativeAOT-friendly.

---

## Repository structure

```
XGE/
├── docs/                     # Design notes, diagrams
├── gameroot/                 # Runtime deployment root
│   ├── bin/                  # Engine, modules, gameconfig.json
│   ├── editor/               # Editor assets (fonts, icon)
│   └── coreclr/              # Bundled .NET runtime for the CoreCLR host
└── xgEngine/                 # Engine source (XGE.slnx)
    ├── 3rdparty/             # SDL3, Dear ImGui (docking), nlohmann/json, DirectXMath, Tree-sitter
    ├── tools/xgReflectGen/   # Reflection code generator
    ├── verify/               # Math test suites (native + managed)
    ├── xgBase/               # Base types, macros, logging, JSON, reflection, math
    ├── xgPlatform/           # Platform backends
    ├── xgCore/               # Engine runtime
    ├── xgUtility/            # SDL ⇄ xgEvent helpers
    ├── xgRendererDX12/       # DirectX 12 renderer module (very rough)
    ├── xgScriptCoreCLR/      # CoreCLR script host
    ├── xgScriptNative/       # Native / NativeAOT script host + C interop exports
    ├── xgEditorKernel/       # Editor kernel module (ImGui)
    ├── xgGameKernel/         # Game kernel module (scene graphs, actor API)
    ├── xgSharedKit/          # Shared native editor/runtime library
    ├── xgSharedKit.Managed/  # Shared C# library (runtime, actors, components, math, Debug)
    ├── xgEditorKit/          # Editor utilities (placeholder)
    ├── xgEditor.CoreCLR/     # Managed editor module
    ├── GameDemoModule/       # NativeAOT sample module
    └── xge/                  # Launcher executable
```

---

## Documentation

- [Runtime architecture](docs/xge-architecture.md) — HSM, scoped systems, script hosts, Messenger, reflection, logging
- [SceneGraph and actors](docs/xge-scenegraph.md) — ActorRegistry, scene graphs, node mixins
- [Actors and components (C#)](docs/xge-components.md) — Actor API, CRTP behaviour components, data components
- [Component graphs](docs/xge-components2.md) — per-type component storage as scoped systems
- [Hybrid component sample](docs/xge-component-sample.md)
- [Design notes](docs/xge-design-notes.md) — codecs, dispatch groups, rendering and networking plans

Each document describes the current code and lists unbuilt work under "Future design".

---

## Roadmap

**Near term**
- Wire up dispatch groups (thread affinity)
- Multi-viewport (floating windows) on the engine renderer
- Back the editor panels with real data (outliner → scene graph, properties → reflection)

**Mid term**
- Transform propagation and one settled component model
- FrameGraph and GPU resource abstractions
- Arrays and nested types in reflection

**Long term**
- Vulkan backend
- Network replication and RPC
- Hot reload of C# gameplay modules
- Visual scripting

---

## License

[MIT](LICENSE)
