# XGE Runtime Architecture

> Status: current implementation (2026-11). Future design is listed at the end. See the [README](../README.md) for build steps and the overall status table.

This document covers the runtime backbone: the module tree (HSM), scoped subsystems, script hosts, messaging, reflection and logging.

| Piece | State |
|---|---|
| ScriptTree (HSM) | Working |
| Scoped subsystems | Working |
| Messenger + routes | Working (native); the managed route API is incomplete |
| Codec registry | Working for numbers and bools; string fields need work |
| Reflection (`xgReflectGen`) | Working for scalar fields and enums |
| Engine-owned logger | Working |
| Dispatch groups | Not wired; every module runs on the main thread |

---

## Hierarchical Script Modules (HSM)

Every script module lives in one tree, the **ScriptTree**. A node's position defines its update order and the scope of the subsystems it sees.

The tree as it loads today:

```
root                    (null host; owns the engine subsystems)
└── editor.kernel       xgEditorKernel.dll     native
    └── game.kernel     xgGameKernel.dll       native
        ├── editor      xgEditor.CoreCLR.dll   CoreCLR
        └── game.demo   GameDemoModule.dll     NativeAOT
```

- `xge` loads `editor.kernel` with `Engine::AddScriptModule(id, path, parent, group)`. The editor kernel then loads its children the same way.
- Each module receives a module id that increases in load order. Shutdown runs in reverse load order, so children shut down before their parents.
- `Engine::Run` updates the tree every frame, parents before children, then calls `ActorRegistry::UpdateActors(dt)`.
- Known issue: `ScriptTree::Update` starts from every main-thread node, and each node also recurses into its children, so nested modules currently update more than once per frame.
- Every module implements `ScriptModule` (`xgCore/public/xgScriptModule.h`):

```cpp
class ScriptModule
{
public:
    explicit ScriptModule(const char* id, uint32_t moduleID, ScriptHost* host, const char* group);

    virtual bool Init(ScriptEngine* engine) = 0;
    virtual void Update(float dt) = 0;
    virtual void Shutdown() = 0;
    virtual bool IsValid() const = 0;
    virtual void OnMessage(const ScriptMessage& msg) = 0;
    virtual SceneGraph* GetSceneGraph() = 0;
};
```

Modules talk to the engine only through `ScriptEngine*` (`xgCore/public/xgScriptEngine.h`), a pure-virtual, append-only interface.

---

## Scoped subsystems

A **subsystem** is any overridable runtime service. Subsystems are registered on a module, and lookup walks up the tree until it finds one: the closest ancestor wins, and children inherit unless they override.

```cpp
struct SceneGraphFactory : public Subsystem {
    XG_DECLARE_BASE_SUBSYSTEM(SceneGraphFactory)
};

class SceneGraphFactoryImpl : public SceneGraphFactory {
    XG_DECLARE_SUBSYSTEM(SceneGraphFactoryImpl, SceneGraphFactory)
};

engine->RegisterSubsystem(module, new SceneGraphFactoryImpl());
auto* factory = engine->GetSubsystem<SceneGraphFactory>(module);
```

- The subsystem's type id is the FNV-1a hash of the base type name (`SuperTypeID`).
- `RegisterSubsystemWithAlias(module, subsystem, "name")` / `GetSubsystemWithAlias(module, "name")` register several subsystems of the same type on one module, for example one component graph per component type.
- Passing `nullptr` as the module registers on the root.
- The engine registers these on the root at startup: `Messenger`, `ActorRegistry`, `EventDispatcher`, `SceneGraphFactory` and `EventQueue`. `game.kernel` currently registers its scene graphs on the root too.

---

## Script hosts

A **ScriptHost** loads a kind of module and creates its `ScriptModule` objects. Hosts are ref-counted and shared by every module they load.

| Host | Loads | How |
|---|---|---|
| CoreCLR (`xgScriptCoreCLR`) | `*.CoreCLR.dll` assemblies | Starts .NET 10 through hostfxr, then binds the static `ScriptEntry.Init/Update/Shutdown/OnMessage` methods of the assembly. |
| Native (`xgScriptNative`) | Native DLLs | Calls the `CreateScriptModule(id, moduleID, host, group)` export, which returns a C++ `ScriptModule`. |
| Native (`xgScriptNative`) | NativeAOT DLLs | Binds the `xg:Init`, `xg:Update`, `xg:Shutdown` and `xg:OnMessage` C exports (`[UnmanagedCallersOnly(EntryPoint = "xg:Init")]` in C#). |

The managed side of both C# hosts is `xgSharedKit.Managed` (`ScriptRuntime`, actors, components, math, `Debug`). Its interop calls go through `LibraryImport` into `xgScriptNative` and `xgGameKernel`.

---

## Messenger and routes

`Messenger` (`xgCore/public/xgMessenger.h`) delivers `ScriptMessage` objects to modules through `OnMessage`. A `Route` (`xgBase/public/xgRoute.h`) selects the receivers. Delivery is synchronous.

```cpp
struct ScriptMessage
{
    const char* TypeName;   // reflected type name
    const void* Payload;    // encoded bytes (JSON or binary)
    int PayloadSize;
    uint32_t SourceID;
};

static Route Direct(const char* id);
static Route ChildrenOf(const char* id, int depthLimit = -1);
static Route ParentOf(const char* id, int depthLimit = -1);
static Route SiblingsOf(const char* id);
static Route Filtered(bool (*predicate)(ScriptModule*));

virtual void Send(const ScriptMessage& msg, const Route& route) = 0;
virtual void SendToAll(const ScriptMessage& msg) = 0;
```

### Example: routing a game event

```cpp
XG_SERIALIZABLE()
struct EnemySpotted
{
    XG_DECLARE_TYPE_NAME(EnemySpotted);

    XG_FIELD() float X = 0;
    XG_FIELD() float Y = 0;
    XG_FIELD() float Radius = 0;
};

class AIModule : public ScriptModule
{
public:
    using ScriptModule::ScriptModule;

    bool Init(ScriptEngine* engine) override { _engine = engine; return true; }

    void ReportEnemy(float x, float y, float radius)
    {
        EnemySpotted evt{ x, y, radius };
        const TypeSchema* schema = _engine->GetTypeRegistry()->Get(EnemySpotted::TypeName);

        ScriptMessage msg{};
        _engine->GetCodecRegistry(ScriptBackendType::Native)->Encode(EnemySpotted::TypeName, &evt, schema, msg);

        _engine->GetSubsystem<Messenger>(this)->Send(msg, Route::SiblingsOf(GetId()));
    }

private:
    ScriptEngine* _engine = nullptr;
};

void PlayerModule::OnMessage(const ScriptMessage& msg)
{
    if (std::strcmp(msg.TypeName, EnemySpotted::TypeName) != 0)
        return;

    EnemySpotted evt{};
    const TypeSchema* schema = _engine->GetTypeRegistry()->Get(EnemySpotted::TypeName);
    if (_engine->GetCodecRegistry(ScriptBackendType::Native)->Decode(msg.TypeName, msg, schema, &evt))
        ReactToThreat(evt.X, evt.Y, evt.Radius);
}
```

Codec details (native vs dynamic codecs, the `"*"` fallback) are in the [design notes](xge-design-notes.md#codec-registry--dynamic-message-encoding).

---

## Reflection

`tools/xgReflectGen` parses headers with Tree-sitter at build time and writes `Generated/*.generated.h/.cpp`.

- `XG_SERIALIZABLE()` marks a struct and `XG_FIELD()` marks each reflected field. The generator emits a `TypeSchema` with one `FieldSchema` per field (name, offset, size, value type), registered in the engine's `TypeRegistry`.
- Supported field types: `bool`, integers, `float` and `const char*`. `XG_ENUM(...)` enums are also emitted as C# enums.
- `XG_DECLARE_TYPE_NAME(Type)` gives a type its own `TypeName` and `StaticTypeID` (FNV-1a), so lookups don't repeat string literals.
- Consumers: the message codecs, and `ReadReflected` / `DescribeReflected` (`xgBase/public/xgReflectJson.h`), which `xge` uses to load `gameconfig.json` into `EngineConfig`.

---

## Logging

- The engine owns one `Logger` (`xgBase/public/xgLog.h`, implemented in `xgCore/LoggerImpl`). `ScriptEngine::GetLogger()` exposes it.
- xgBase is a static library, so every DLL has its own copy of its globals. Each module entry point calls `xg::SetLogger(engine->GetLogger())`, so all `xg::Log` calls reach the same callbacks.
- C# code logs through `Debug` (`Log`, `LogWarning`, `LogError`, `LogException`, …), which forwards to the engine logger.

---

## Future design

Not implemented yet:

- **Dispatch groups and thread affinity:** modules assigned to worker, pinned or dedicated domains, with cross-thread queued message delivery. `ThreadDomain` and a `DispatchSystem` exist but aren't wired.
- **Headless multi-instance servers:** a server module hosting N isolated game-instance subtrees with no renderer.
- **Shared world data:** parent modules owning shared state (world state, caches, config, services) that children request through the Messenger.
- **Hot-reload boundaries:** unloading and reloading a subtree. CoreCLR assemblies currently load into a non-collectible context, and native DLLs aren't unloaded.
- **More script hosts:** Lua, Squirrel, Python, JavaScript, WASM. The host interface is backend-agnostic, but none exist.
