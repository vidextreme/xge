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
- **Scoped subsystems:** services are registered per module and resolved by walking up the tree, so children inherit or override them.
- **Dual C# scripting:** CoreCLR for the editor, NativeAOT for the runtime, both behind the same `ScriptModule` interface.
- **Reflection codegen:** a Tree-sitter tool (`xgReflectGen`) generates type schemas from `XG_SERIALIZABLE` / `XG_FIELD` / `XG_ENUM` markers, used for JSON, message codecs and C# enums.
- **Config-driven module tree:** `gameconfig.json` (or any file passed with `--config`) describes the engine settings and the whole module tree, so the editor, the Project Hub and a game runtime are just different configs.
- **Immediate UI kit:** `xgImUIKit` wraps Dear ImGui (docking + multi-viewport) behind a C ABI with C++ and C# wrappers, including Unity-style layout groups. It draws through the engine renderer or an SDL GPU fallback.
- **Game projects:** Unity-style project folders whose C# scripts are compiled into a separate solution and loaded into the editor. The Project Hub creates, opens and lists them.
- **Stable ABI:** cross-DLL interfaces are pure virtual and append-only, and only C types and function pointers cross module boundaries.

---

## Status

| Area | Status | Notes |
|---|---|---|
| ScriptTree / HSM | 🟢 Working | Native, CoreCLR and NativeAOT modules load into one tree. |
| Scoped subsystems | 🟢 Working | Lookup walks up the tree; the closest ancestor wins. |
| Messenger / routes | 🟢 Working | `Direct`, `ChildrenOf`, `ParentOf`, `SiblingsOf`, `Filtered`, `SendToAll`. The managed route API is still incomplete. |
| Codec registry | 🟡 Partial | JSON and binary codecs work for numbers and bools; string fields need work. |
| Reflection (`xgReflectGen`) | 🟡 Partial | bool / int / uint / float / `const char*` fields and enums. No arrays or nested types yet. |
| Config + command line | 🟢 Working | An `EngineConfig` table (read through reflection) and a `ScriptModules` tree. `--config`, `--project`, `--new-project`, `--no-script-build`. See [command line](docs/xge-command-line.md). |
| Actors & components (C#) | 🟡 Partial | `Actor.Create`, `AddComponent<T>` / `GetComponent<T>` / `RemoveComponent<T>`, `Destroy`. Update logic uses **CRTP**: a component implements `IComponentUpdate<T>` with a `static abstract OnUpdate(T, float)`, and one `ComponentUpdateGroup<T>` per type calls it through a cached function pointer (no virtual call, boxing or reflection per frame). Removals are deferred until after the pass. Driven every frame by `ActorRegistry::UpdateActors(dt)` in both CoreCLR and NativeAOT modules. Data components (`ComponentData<TData>`, `IComponentDataUpdate<TData>`) and native component storage are prototypes. |
| SceneGraph (native) | 🟡 Partial | ActorIDs are global 64-bit IDs; an actor can belong to several graphs. The tree graph works; transforms are not propagated yet. |
| Logging | 🟢 Working | Engine-owned logger shared by every module; Unity-style C# `Debug` (`Log`, `LogWarning`, `LogError`, …). |
| Math + C# interop | 🟢 Working | Vectors, quaternions, matrices, shapes, geometry queries, easing, random, color. Native and managed test suites in `tools/verify/`. |
| Immediate UI kit (`xgImUIKit`) | 🟢 Working | Panels, menus, widgets, virtualized lists, modals, layout groups (`Gui.Horizontal`/`Vertical`/`FlexibleSpace`), FreeType fonts (Noto), per-monitor DPI. Multi-viewport on DX12 and SDL GPU. |
| Editor UI | 🟡 Partial | Real panels: Console (fed by the engine log), Hierarchy (actor tree) and Inspector (components of the selected actor, drawn by Unity-style property drawers). The editor shell (`EditorUI: true`) and its other panels are still a non-functional prototype. |
| Game projects | 🟡 Step 1 | Create and open project folders (`--project`, `--new-project`), auto-build the C# scripts, and load them into the editor as `game.project`. Scripts under `Editor` folders build into a separate editor-only assembly (property drawers). Next: an Assets watcher with hot reload, then asset import and a NativeAOT player build. |
| Project Hub | 🟢 Working | `xge.exe --config hub.config`: add, create, open and remove projects, with a preview image per project. |
| Renderer (DX12) | 🟠 Very rough | Device, swap chains for every UI window, and a UI pass behind the generic `xg::UIRenderTarget` API (no ImGui in the renderer, no DX12 in the UI kit). Offscreen render views draw a test cube and grid for the scene views; no real 3D pipeline yet. |
| Dispatch groups / threading | 🟡 Partial | Thread domains and a dispatch system exist but are not wired; everything runs on the main thread. |
| Vulkan | ⚪ Planned | |
| Network replication | ⚪ Planned | |
| Hot reload | ⚪ Planned | |

---

## Getting started

### Requirements

- Windows 10/11 x64 (Linux - planned)
- Visual Studio 2026 with the C++ desktop workload (toolset v145, other compile - planned)
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

```powershell
cd gameroot\bin
.\xge.exe                                         # editor
.\xge.exe --config hub.config                     # Project Hub
.\xge.exe --project ..\..\samples\SampleGame      # open a game project in the editor
.\xge.exe --new-project D:\Games\SpaceShooter     # create a project from the template, then open it
```

By default xge reads `gameconfig.json` next to `xge.exe`, wherever it's started from. The config has two tables: engine settings, and the module tree to load:

```json
{
    "EngineConfig": {
        "RendererModule": "xgRendererDX12",
        "EditorUI": false,
        "EditorMultiViewport": true
    },
    "ScriptModules": {
        "editor.kernel": { "path": "xgEditorKernel.dll", "group": "group.main", "dispatchgroup": "main", "children": {
            "game.kernel": { "path": "xgGameKernel.dll", "children": {
                "editor": "xgEditor.CoreCLR.dll",
                "game.demo": "GameDemoModule.dll"
            } }
        } }
    }
}
```

| `EngineConfig` key | Meaning |
|---|---|
| `RendererModule` | Renderer DLL, for example `xgRendererDX12`. Empty uses the SDL GPU fallback (no 3D views). |
| `EditorUI` | `true` shows the editor shell prototype; `false` shows an empty dockspace, where the real panels (Console, Hierarchy) still dock. |
| `EditorMultiViewport` | Panels dragged outside the main window become OS windows. |
| `ProjectPath`, `ProjectAutoBuild` | Project to open (relative to the config file) and whether to build its scripts first. |
| `WindowTitle`, `WindowWidth`, `WindowHeight` | Main window title and size. |

The [command-line reference](docs/xge-command-line.md) covers every option, the `ScriptModules` format, the startup order and more config examples.

In Visual Studio, set **xgHub.CoreCLR** as the startup project. Its Start button then offers the profiles **XGE (default config)** and **XGE Project Hub**.

---

## Architecture

```
xgBase         → Base types, macros, logging, JSON, reflection, math, UI draw/render-target interfaces (static lib)
xgPlatform     → SDL3 windows and events, secondary OS windows, file dialogs, image loading, SDL GPU UI fallback, Win32 files
xgCore         → Engine runtime: ScriptTree, subsystems, Messenger, codecs, ActorRegistry, logger
Renderers      → DX12 (very rough: UI surfaces, offscreen views), Vulkan (planned)
Script hosts   → CoreCLR (hostfxr), Native + NativeAOT (xgScriptNative, also the C exports for managed code)
Kernels        → xgEditorKernel (editor UI wiring), xgGameKernel (scene graphs, actor exports)
UI kit         → xgImUIKit (immediate-mode UI, the only Dear ImGui user), xgImUIKit.Managed (C#)
Kits           → xgSharedKit, xgEditorKit (UIHostSession), xgSharedKit.Managed (C# runtime, actors, Debug),
                 xgEditorKit.Managed (C# editor API: PropertyDrawer, SerializedProperty)
Managed apps   → xgEditor.CoreCLR (editor panels, projects), xgHub.CoreCLR (Project Hub)
Templates      → Game project template and SDK props/targets, installed to gameroot/sdk
xge            → Launcher: config file + command line → window, engine, module tree
```

### Module tree at runtime

xge doesn't hard-code any module: it loads the `ScriptModules` tree of the config file, depth-first in file order. The default `gameconfig.json` gives the editor tree. When a project is open, the editor adds the project's scripts at runtime:

```
root                      (engine subsystems)
└── editor.kernel         (xgEditorKernel.dll, native, UI host)
    └── game.kernel       (xgGameKernel.dll, native, scene graphs, actor exports)
        ├── editor        (xgEditor.CoreCLR.dll, CoreCLR, Console, Hierarchy, project service)
        └── game.demo     (GameDemoModule.dll, NativeAOT, test module)
```

`hub.config` loads a single module, which runs the UI host itself:

```
root
└── hub   xgHub.CoreCLR.dll   CoreCLR   Project Hub
```

A module can add modules with `ScriptRuntime.AddScriptModule(id, path, parentId, group)`, outside a frame update. See the [command-line reference](docs/xge-command-line.md#scriptmodules) for the config format.

### Frame loop

`Engine::Run` measures the frame delta (steady clock, capped at 0.25 s), polls window events, updates the ScriptTree (every module, parent before child), then updates actors and their components through `ActorRegistry::UpdateActors(dt)`.

---

## A taste of the API

### Subsystems

```cpp
struct SceneGraphFactory : public Subsystem {
    XG_DECLARE_BASE_SUBSYSTEM(SceneGraphFactory)
};

class SceneGraphFactoryImpl : public SceneGraphFactory {
    XG_DECLARE_SUBSYSTEM(SceneGraphFactoryImpl, SceneGraphFactory)
};

engine->RegisterSubsystem(module, new SceneGraphFactoryImpl());       // scoped to module and its children
auto* factory = engine->GetSubsystem<SceneGraphFactory>(module);      // closest ancestor wins
```

### Messages

```cpp
Messenger* messenger = engine->GetSubsystem<Messenger>(this);
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

    XG_FIELD()
    const char* WindowTitle = nullptr;

    XG_FIELD()
    int WindowWidth = 1280;
    // ...
};

const TypeSchema* schema = engine.GetTypeRegistry()->Get(EngineConfig::TypeName);
ReadReflected(json, *schema, &config, strings, "EngineConfig");   // reads the "EngineConfig" table
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
├── docs/                     # Architecture, command line, design notes, diagrams
├── gameroot/                 # Runtime deployment root
│   ├── bin/                  # xge.exe, engine and module DLLs, gameconfig.json, hub.config, imgui.ini
│   ├── editor/               # Editor assets (Noto/DejaVu fonts, icon)
│   ├── coreclr/              # Bundled .NET runtime for the CoreCLR host
│   └── sdk/                  # Game project SDK and template, installed by the build (generated)
├── samples/                  # Local test projects such as SampleGame (untracked)
└── xgEngine/                 # Engine source (XGE.slnx)
    ├── 3rdparty/             # SDL3, Dear ImGui (docking), FreeType, nlohmann/json, DirectXMath, Tree-sitter
    ├── templates/            # Game project template (GameProject/) and SDK props/targets (sdk/)
    ├── tools/xgReflectGen/   # Reflection code generator
    ├── tools/verify/         # Math test suites (native + managed), ImUIParity.ps1
    ├── xgBase/               # Base types, macros, logging, JSON, reflection, math, platform interfaces
    ├── xgPlatform/           # SDL3 windows, dialogs, images, SDL GPU UI fallback, Win32 files
    ├── xgCore/               # Engine runtime
    ├── xgUtility/            # SDL ⇄ xgEvent helpers
    ├── xgRendererDX12/       # DirectX 12 renderer module (very rough)
    ├── xgScriptCoreCLR/      # CoreCLR script host
    ├── xgScriptNative/       # Native / NativeAOT script host + C exports (projects, UI host, dialogs)
    ├── xgImUIKit/            # Immediate-mode UI kit (Dear ImGui + FreeType, C ABI, C++ wrapper)
    ├── xgImUIKit.Managed/    # C# bindings for xgImUIKit (Gui, Painter, Panel, layouts, modals)
    ├── xgEditorKernel/       # Editor kernel module (UI host, editor wiring, test panels)
    ├── xgGameKernel/         # Game kernel module (scene graphs, actor API)
    ├── xgSharedKit/          # Shared native editor/runtime library
    ├── xgSharedKit.Managed/  # Shared C# library (runtime, actors, components, math, Debug, UIHost)
    ├── xgEditorKit/          # Editor static library (UIHostSession)
    ├── xgEditorKit.Managed/  # C# editor API for editor scripts (PropertyDrawer, SerializedProperty, EditorGUI)
    ├── xgEditor.CoreCLR/     # Managed editor module (Console, Hierarchy, Inspector, projects)
    ├── xgHub.CoreCLR/        # Project Hub module
    ├── GameDemoModule/       # NativeAOT test module
    └── xge/                  # Launcher executable
```

---

## Documentation

- [Command line and config files](docs/xge-command-line.md) — xge options, startup order, `EngineConfig` and `ScriptModules`, shipped configs, examples
- [Runtime architecture](docs/xge-architecture.md) — HSM, scoped subsystems, script hosts, UI host, Messenger, reflection, logging
- [SceneGraph and actors](docs/xge-scenegraph.md) — ActorRegistry, scene graphs, node mixins
- [Actors and components (C#)](docs/xge-components.md) — Actor API, CRTP behaviour components, data components
- [Component graphs](docs/xge-components2.md) — per-type component storage as scoped subsystems
- [Hybrid component sample](docs/xge-component-sample.md)
- [Design notes](docs/xge-design-notes.md) — codecs, dispatch groups, rendering and networking plans

Each document describes the current code and lists unbuilt work under "Future design".

---

## Roadmap

**Near term**
- Game projects: an Assets watcher with background build and script hot reload, then asset import, then a NativeAOT player build
- Inspector: editing beyond fields (add/remove components, undo, multi-selection), and saving values with the scene
- Wire up dispatch groups (thread affinity)

**Mid term**
- Transform propagation and one settled component model
- FrameGraph and GPU resource abstractions
- Arrays and nested types in reflection

**Long term**
- Vulkan backend
- Network replication and RPC
- Visual scripting

---

## License

[MIT](LICENSE)
