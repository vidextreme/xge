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
- **Data pipeline:** assets keep stable GUIDs in their metadata, build in the background into a disposable, GUID-addressed `Library/`, and reach the GPU through a shared, ref-counted resource system with async loads and live reload on rebuild. Materials, archetypes and scenes are authored in dedicated editors.
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
| Actors & components (C#) | 🟢 Working | `Actor.Create`, `AddComponent` (any object, including existing instances), `GetComponent` / `RemoveComponent`, `Destroy` (takes the subtree). Optional `IComponent` callbacks and `Component` base class. Behaviour updates use **CRTP** (`IComponentUpdate<T>`, one function-pointer loop per type). **Data components** (`ComponentDataSet<T1..T4>`) keep blittable structs in native pools reached by `ref`; **data updates** (`IComponentDataUpdate<TSelf, TWrite, TRead...>`) run over a whole pool inline or as parallel job ranges, with command buffers for deferred changes. Data survives hot reload (with layout migration) and play mode restores it. Scheduling groups by declared access is planned. |
| SceneGraph & transforms (native) | 🟢 Working | ActorIDs are global 64-bit IDs; the root tree graph holds hierarchy and names, and destroying an actor destroys its subtree with destroy notifications. `ComponentDataRegistry` keeps one packed pool per data type with change versions; `TransformSystem` adds optional transforms (local pose in the `"xg.Transform"` pool, world matrices from the tree, keep-world reparenting). |
| Logging | 🟢 Working | Engine-owned logger shared by every module; Unity-style C# `Debug` (`Log`, `LogWarning`, `LogError`, …). |
| Math + C# interop | 🟢 Working | Vectors, quaternions, matrices, shapes, geometry queries, easing, random, color. Native and managed test suites in `tools/verify/`. |
| Immediate UI kit (`xgImUIKit`) | 🟢 Working | Panels, menus, widgets, virtualized lists, modals, layout groups (`Gui.Horizontal`/`Vertical`/`FlexibleSpace`), FreeType fonts (Noto), per-monitor DPI. Multi-viewport on DX12 and SDL GPU. |
| Editor UI | 🟡 Partial | Editor shell (`EditorUI: true`: main menu, toolbar, status bar, dock layout) with real panels: Scene and Game (render views drawing the scene's meshes and objects; the Game view renders through the main `Camera` component), Hierarchy (grouped by scene, with create, rename and delete), Inspector (drawn by Unity-style property drawers), Console, Navigator, Tools, Material Editor and Archetype Editor. Right-click context menus open at the cursor; tools add entries by implementing `IContextMenuTool` (the Navigator's asset and create menus use it). Asset names are shown without their file extensions; Rename is a shared dialog ("Rename Texture", with the item's thumbnail) that keeps the extension. |
| Game projects | 🟡 Step 1 | Create and open project folders (`--project`, `--new-project`), auto-build the C# scripts, and load them into the editor as `game.project`. Scripts under `Editor` folders build into a separate editor-only assembly (property drawers). Next: asset import and a NativeAOT player build. |
| Project Hub | 🟢 Working | Double-click `gameroot\bin\xgHub.lnk` (written by the build) or run `xge.exe --config hub.config`: add, create, open and remove projects, with a preview image per project. Projects open without a console window; each row has an "Open with console" button. |
| Renderers (DX12, Vulkan) | 🟡 Working, basic | Both backends have the same features: swap chains for every UI window, the UI pass behind `xg::UIRenderTarget`, offscreen render views, BC/RGBA textures with mips, meshes, project surface shaders, materials and readback (thumbnails). One directional light and an ambient term; no shadows, post-processing or frame graph yet. |
| Jobs / threading | 🟡 Partial | `JobSystem` runs worker and main-thread jobs with dependencies; asset builds, resource loads and large data updates use it. Module thread affinity (dispatch groups) is still planned. |
| Resources and assets | 🟢 Working | GUID metas with item tables and name trees, chunk files, a background `Library/` pipeline with manifests, import tags and cancellation, and a shared runtime resource system with async loads and live reload. See [Data and assets](docs/xge-data.md). |
| Textures, meshes | 🟢 Working | PNG/BMP to BC1/3/4/5 or RGBA8 with mips; FBX static meshes and hierarchy (ufbx). `MeshRenderer` draws a model with a material. |
| Shaders, materials | 🟢 Working | Project HLSL surface shaders with slot annotations compiled by DXC for both backends; `.xcm` materials with a Material Editor (live preview, colour pickers, texture pickers) and generated thumbnails. Built-in shaders, materials, primitive meshes and textures (`gameroot/engine/assets`) are ordinary assets that projects reference by GUID; broken materials draw a magenta error material. |
| Archetypes | 🟡 Working, early | `.xco` trees of meshes and proxies joined by fixed Jointers, drawn by `ArchetypeDriver`; Composition Actors with authored components; an Archetype Editor with a live preview. Animation and physics are planned. |
| Scenes | 🟢 Working | `.xcs` scenes save the actors set up in the editor (hierarchy, transforms, components and values, composition overrides). File menu, Hierarchy authoring, last scene reopened. Additive loading is prepared in the data model. See [Scenes](docs/xge-scenes.md). |
| Network replication | ⚪ Planned | |
| Hot reload | 🟢 Working | Saving a script (or Assets > Build Scripts / the toolbar button) rebuilds the project in the background and swaps the new scripts in without restarting the editor. Actors and their component field values are kept; a failed build leaves the old scripts running. |
| Play mode | 🟢 Working | Toolbar transport capsule: Play in Editor (becomes Pause), +1 (step one frame while paused), Stop, and Play Standalone (runs the current build in a separate xge process without the editor). Component updates run only while playing (in every module), and the project's scripts are paused outside play. Everything changed during play is temporary: Stop restores the actors and component values from before Play and removes actors spawned during play. Works with or without a project. A green (playing) or amber (paused) outline frames the dock area. |

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
.\xge.exe --config hub.config                     # Project Hub (or double-click xgHub.lnk: no console window)
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
| `EditorUI` | `true` shows the editor shell (title bar, main menu, toolbar, status bar and the dock layout); `false` shows an empty dockspace, where the panels still dock. |
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

public struct Spin : IComponentData { public float Angle, Speed; }

public class Spinner : ComponentDataSet<Spin>, IComponentDataUpdate<Spinner, Spin>
{
    // Every Spin element in a dense loop, split across job workers when the pool is large.
    public static void OnDataUpdate(ref Spin s, in DataUpdateContext ctx) => s.Angle += s.Speed * ctx.DeltaTime;
}
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
├── samples/                  # Sample projects (SampleGame)
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
- [SceneGraph and actors](docs/xge-scenegraph.md) — ActorRegistry, the actor tree, destroy listeners, node mixins
- [Actors and components (C#)](docs/xge-components.md) — Actor API, components, Transform, data components and parallel data updates
- [Native component data](docs/xge-components2.md) — ComponentDataRegistry pools, TransformSystem, cross-module use
- [Hybrid component sample](docs/xge-component-sample.md)
- [Data and assets](docs/xge-data.md) — asset GUIDs and metas, the Library pipeline, chunk files, runtime resources, textures, meshes, shaders, materials, archetypes
- [Scenes](docs/xge-scenes.md) — the `.xcs` format, membership, component ids, composition overrides, editor workflow
- [Design notes](docs/xge-design-notes.md) — codecs, dispatch groups, rendering and networking plans

Each document describes the current code and lists unbuilt work under "Future design".

---

## Roadmap

**Near term**
- Player packaging and a NativeAOT player build that loads scenes
- Additive scenes (load several, pick the active one, move actors between them)
- Transform gizmos in the Scene view and the Archetype Editor
- Inspector: undo and multi-selection

**Mid term**
- Animation (skinned meshes, Timeline) and physics data in archetypes
- Wire up dispatch groups (thread affinity)
- Data updates scheduled by declared access, double-buffered pools, a parallel transform pass
- FrameGraph and GPU resource abstractions
- Arrays and nested types in reflection

**Long term**
- Network replication and RPC
- Visual scripting

---

## License

[MIT](LICENSE)
