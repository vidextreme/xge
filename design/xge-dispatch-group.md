# XGE Dispatch Graph

> Status: design (2026-11). Not implemented. `ThreadDomain` and `xgCore/DispatchSystem` exist but aren't wired, so every module runs on the main thread. See [Current state](#current-state) for what the code has today.

The Dispatch Graph is an execution framework, not a job system. Users don't create threads, worker pools, jobs or synchronization primitives. They describe the execution flow, and the engine handles:

- scheduling
- parallelization
- thread assignment
- synchronization
- deterministic ordering

It turns XGE's existing runtime pieces (HSM, ScriptTree, Messenger, Codec Registry and the scene graphs) into one execution model.

---

## Contents

1. [Core idea](#core-idea)
2. [Dispatch groups](#dispatch-groups)
3. [The graph](#the-graph)
4. [Assigning work to groups](#assigning-work-to-groups)
5. [Thread affinity](#thread-affinity)
6. [Messaging across groups](#messaging-across-groups)
7. [Scheduling model](#scheduling-model)
8. [Scene graph integration](#scene-graph-integration)
9. [Compared with fixed update loops](#compared-with-fixed-update-loops)
10. [Current state](#current-state)
11. [Open design questions](#open-design-questions)
12. [Implementation phases](#implementation-phases)

---

## Core idea

The runtime splits into five responsibilities:

| System | Responsibility |
|---|---|
| HSM | Ownership |
| ScriptTree | Hierarchy and system scope |
| Messenger | Communication |
| Dispatch Graph | Execution order |
| Scheduler | Threads |

```
        HSM            ownership
         │
         ▼
     ScriptTree        hierarchy
         │
         ▼
     Messenger         communication
         │
         ▼
   Dispatch Graph      execution
         │
         ▼
     Scheduler         threads
```

Users define:

- what runs
- which group it belongs to
- what depends on what

The engine decides:

- when it runs
- which thread runs it
- how the threads synchronize

The scheduler is an implementation detail. Gameplay code should rarely need `std::thread`, `CreateThread`, mutexes or condition variables.

### Relationship to the ScriptTree

The ScriptTree already provides parent-child ownership, scoped routing, scoped systems, domain grouping and deterministic update order.

```
GameRoot
├── Gameplay
│   ├── Player
│   ├── AI
│   └── Inventory
├── Animation
└── Rendering
```

The Dispatch Graph adds execution relationships *between* groups. The tree keeps deciding ownership, system lookup and message scope. The graph decides when each node runs. A child doesn't have to run in its parent's group.

---

## Dispatch groups

A dispatch group is a named execution domain. Typical groups:

| Group | Group | Group |
|---|---|---|
| Input | Animation | RenderPrep |
| Gameplay | Audio | Render |
| Physics | Networking | Async |
| AI | Custom | |

Groups are user-defined. The engine gives no group a special meaning, except that a group's [thread affinity](#thread-affinity) decides where it can run.

---

## The graph

Instead of fixed engine phases (`Update`, `LateUpdate`, `Render`), each game defines its own pipeline as a directed acyclic graph of groups.

### Linear pipeline

```
Input → Gameplay → Physics → Animation → RenderPrep → Render
```

### Branching pipeline

```
         Gameplay
        /        \
      AI        Physics
        \        /
        Animation
            │
        RenderPrep
            │
          Render
```

`AI` and `Physics` both depend only on `Gameplay`, so the scheduler runs them in parallel. `Animation` starts once both finish.

### Fan-out

```
Gameplay
├── AI
├── Physics
├── Audio
└── Networking
        │
        ▼
    Animation
```

The four children run at the same time, and `Animation` runs after all of them complete. Execution is parallel, but the dependencies stay deterministic.

### C++ API

```cpp
DispatchGraph graph;

graph.AddGroup("Input");
graph.AddGroup("Gameplay");
graph.AddGroup("Physics");
graph.AddGroup("Animation");
graph.AddGroup("RenderPrep");

graph.Connect("Input", "Gameplay");
graph.Connect("Gameplay", "Physics");
graph.Connect("Physics", "Animation");
graph.Connect("Animation", "RenderPrep");
```

### Fluent API

```cpp
DispatchBuilder builder;

builder
    .Group("Input")
    .Then("Gameplay")
    .Then("Physics")
    .Then("Animation")
    .Then("RenderPrep");
```

---

## Assigning work to groups

The unit the scheduler runs is a **(module, group) entry**: one module's work for one group. A module can join several groups. See [question 1](#1-unit-of-scheduling) for the details.

**C++ modules** join groups explicitly in `Init`:

```cpp
bool PlayerModule::Init(ScriptEngine* engine)
{
    _gameplay  = engine->JoinGroup(this, "Gameplay");
    _animation = engine->JoinGroup(this, "Animation");
    return true;
}

void PlayerModule::UpdateGroup(uint32_t group, float dt)
{
    if (group == _gameplay)
        TickGameplay(dt);
    else if (group == _animation)
        TickAnimation(dt);
}
```

**C# code** binds classes, component types and methods with attributes. The managed runtime joins the groups for its module.

```csharp
[XGDispatchGroup("Gameplay")]
class PlayerController : IComponentUpdate<PlayerController>
{
    public static void OnUpdate(PlayerController self, float dt) { }
}

[XGDispatchGroup("Animation")]
class CharacterAnimator : IComponentUpdate<CharacterAnimator>
{
    public static void OnUpdate(CharacterAnimator self, float dt) { }
}

static class PhysicsHooks
{
    [XGUpdate("Physics")]
    static void PhysicsStep(float dt) { }
}
```

A module that joins no group runs in the built-in `Default` group, which has `MainThread` affinity. Existing modules keep working unchanged.

### Update phases

Groups are the only ordering unit. The old phase list maps onto groups like this ([question 2](#2-phases-versus-groups)):

| Old phase | Now |
|---|---|
| `Init`, `Shutdown` | Module lifecycle calls, unchanged |
| `Update` | A group's normal run, once per frame |
| `LateUpdate` | A group that runs after the one it follows |
| `Fixed` | A group with a `TickRate`, run 0..N times per frame |
| `Render` | A group with `MainThread` affinity. The renderer's own submit runs after the graph. |
| `Async` | Not part of the frame. Work submitted to the worker pool. |
| `Custom` | Every group is custom |

---

## Thread affinity

A group can declare where it prefers to run.

| Affinity | Meaning |
|---|---|
| `MainThread` | The thread that owns the window and the event loop |
| `WorkerThread` | Any thread in the engine's worker pool |
| `DedicatedThread` | Its own long-lived thread |
| `PinnedThread(N)` | Lane `N`, a long-lived thread pinned to a core |

Example:

```
Input      → MainThread
Gameplay   → WorkerThread
Physics    → WorkerThread
Audio      → DedicatedThread
RenderPrep → MainThread
```

Affinity changes only thread placement. Execution order and the dependency structure stay the same.

The engine owns thread creation, worker pools, job queues, synchronization, work stealing and load balancing.

---

## Messaging across groups

The Messenger stays responsible for communication and keeps its routes: parent, children, siblings, a specific node, filtered, and broadcast.

The graph decides execution order. The Messenger decides message flow.

```
Gameplay
   │  Send(msg)
   ▼
Animation
```

Messages within a group are delivered right away. Messages between groups are queued and delivered when the receiving group starts, so the sender never touches the receiver while it runs. See [question 3](#3-cross-group-message-delivery) for the exact rules.

---

## Scheduling model

The scheduler treats every group as a node in a DAG:

```
Dispatch groups
      ↓
Dependency graph
      ↓
Topological sort
      ↓
Parallel scheduler
      ↓
Thread execution
```

This gives:

- deterministic execution order
- dependency validation
- cycle detection when the graph is built
- automatic parallelism

---

## Scene graph integration

Scene graph work joins the pipeline as ordinary groups:

```
Input → Gameplay → SceneGraph → TransformPropagation → Animation → RenderPrep → Render
```

Transform, animation and render preparation updates stay in step because they are ordered by the graph, not by convention.

---

## Compared with fixed update loops

| Traditional engines | XGE |
|---|---|
| `Update` | `Input` |
| `LateUpdate` | `Gameplay` |
| `Render` | `Physics` |
| | `Inventory` |
| | `QuestSystem` |
| | `Animation` |
| | `Audio` |
| | `RenderPrep` |
| | `Render` |

Benefits:

- fully customizable stages
- deterministic
- parallel-friendly
- organized by domain
- execution stages that aren't hard-coded in the engine

### Summary

The Dispatch System is a user-authored execution graph. Users define the execution domains, their dependencies and their scheduling intent. The engine provides thread management, synchronization, parallelization and deterministic execution.

- HSM defines ownership.
- ScriptTree defines hierarchy.
- Messenger defines communication.
- Dispatch Graph defines execution flow.
- Scheduler manages threads.

Dispatch groups stop being thread-affinity tags and become the execution model for the whole engine.

---

## Current state

What the code has today, and what this design has to change.

| Piece | Where | State |
|---|---|---|
| `ThreadDomain` enum | `xgCore/public/xgEngineDef.h` | `MainThread`, `WorkerThread`, `DedicatedThread`, `PinnedThread`. `ScriptTree::AddModule` files nodes into main, dedicated and pinned lists, ignores `WorkerThread`, and `Engine::AddScriptModule` always passes `MainThread`. |
| `DispatchSystem` | `xgCore/DispatchSystem.*` | A main queue plus dedicated and pinned thread loops over `std::function`. Never instantiated. There's no worker pool, `_running` is a plain `bool` read across threads, and nothing waits for a batch to finish. Treat it as a sketch, not a base. |
| `ScriptTree::Update` | `xgCore/ScriptTree.cpp` | Starts from every main-thread node, and `ScriptNode::Update` also recurses into children, so nested modules update more than once per frame (known issue). The graph needs nodes that don't recurse. |
| Messenger | `xgCore/MessengerImpl.cpp` | Synchronous: `Send` calls `OnMessage` on the receiver from the sender's thread. Safe only while everything runs on one thread. |
| `ScriptModule::GetGroup()` | `xgCore/public/xgScriptModule.h` | Already exists, but it's the **script host** group used by `RegisterHostInGroup`, not a dispatch group. The name collides. |
| Frame loop | `Engine::Run` in `xgCore/xgEngine.cpp` | Fixed order: poll events, `ScriptTree::Update`, `ActorRegistry::UpdateActors`, `Renderer::Render`. Actor updates and rendering sit outside the tree. |
| Managed components | `xgSharedKit.Managed/ComponentUpdateGroup.cs` | One update group per component type, run serially from `UpdateActors`. These are natural units for parallel work later. |
| Cross-DLL rule | `xgCore/public/xgScriptEngine.h` | xgBase, xgSharedKit and xgUtility are static libs, so the scheduler must live in xgCore and be reached through a stable ABI (pure virtual interface, C types, appended virtuals). |

---

## Open design questions

These need answers before or during implementation.

### 1. Unit of scheduling

> Proposed decision (2026-11): the scheduler runs (module, group) entries. Modules join groups at run time, and managed code binds classes and methods to groups inside its module.

A `ScriptModule` is a whole DLL or assembly (`editor.kernel`, `game.demo`). Its only per-frame entry is `Update(float dt)`, exported as `XG_Update` (native and NativeAOT) or `ScriptEntry.Update` (CoreCLR). Managed components update through a separate path: each managed module registers a callback with `ActorRegistry::AddUpdateCallback`, and `Engine::Run` calls them all after the tree in `UpdateActors`.

#### Rule 1: the scheduled unit is a (module, group) entry

- A module joins one or more groups. Each join creates one entry, and the scheduler calls the module once per entry per frame.
- Inside a group, entries run in tree order: parents before children, then load order. This keeps today's update order.

#### Rule 2: modules join groups through the engine

- `ScriptEngine::JoinGroup(ScriptModule* module, const char* group)` returns a `uint32_t` group id. Names cross the ABI only here; per-frame calls use ids.
- The group must exist in the graph. An unknown name logs an error and the module goes to `Default` instead. Modules don't create groups; the graph does ([question 7](#7-where-the-graph-is-defined)).
- Joining is a structural change ([question 4](#4-data-hazards-between-parallel-groups), rule 4). During `Init` it applies right away. Later, it applies at the next frame boundary.
- A module that joins nothing is put in `Default`: always present, `MainThread` affinity, first in the graph unless the graph places it. Existing modules run unchanged.

#### Rule 3: one new entry point

- `ScriptModule` gets an appended virtual `UpdateGroup(uint32_t group, float dt)`. Its default implementation calls `Update(dt)`.
- Native and NativeAOT modules may export `xg:UpdateGroup`. CoreCLR modules may expose `ScriptEntry.UpdateGroup`. Both are optional.
- A module that joins more than one group must provide `UpdateGroup`. Otherwise `JoinGroup` fails for the second group, because the engine can't tell the calls apart.

#### Rule 4: managed code binds to groups inside its module

- `[XGDispatchGroup("Name")]` on a class or component type, and `[XGUpdate("Name")]` on a static method, bind that code to a group. A method without its own group uses its class's group. Anything unbound uses the module's first group, or `Default`.
- `ComponentUpdateGroup<T>` reads the attribute once, when it's created, and joins the group for its module if needed. Component types appear lazily, so a join after `Init` applies at the next frame boundary.
- The managed `UpdateGroup(group, dt)` runs the `[XGUpdate]` methods bound to that group, then the component update groups bound to it, each in registration order.
- `ActorRegistry::AddUpdateCallback` and the separate `UpdateActors` step go away. Component updates run inside their module's entries instead of after the whole tree.

#### Rule 5: C++ uses explicit calls

The `XG_DISPATCH_GROUP(Name)` class macro is dropped for now. A macro on a class can't run code, and a C++ module is one class per DLL, so an explicit `JoinGroup` in `Init` is clearer. xgReflectGen could generate those calls later.

#### What this changes in existing code

- `ScriptModule`, `ScriptModuleNative` and `ScriptModuleCoreCLR` get `UpdateGroup` and look up the optional export.
- `ScriptEngine` gets `JoinGroup` (appended).
- `ActorRegistryImpl` drops update callbacks. The managed `ActorRegistry.cs` update hook becomes part of the managed `UpdateGroup`.
- `xgEditor.CoreCLR` and `GameDemoModule` keep working without changes through `Default`.

### 2. Phases versus groups

> Proposed decision (2026-11): groups are the only ordering unit. Phases become group properties, engine stages or lifecycle calls.

#### Rule 1: the frame has three stages

```
Frame start   (main thread)  poll events, apply structural commands, rotate outboxes
Graph                        every group in DAG order, in parallel where allowed
Frame end     (main thread)  Renderer::Render
```

- Frame start and frame end belong to the engine and aren't groups. They're the sync points that question 3 and question 4 refer to.
- `Render` in the examples is the user's main-thread render submission group. The renderer's own frame (`Renderer::Render`, present) runs after the graph, as it does today.

#### Rule 2: phases map onto groups

| Phase | Decision |
|---|---|
| `Init`, `Shutdown` | Module lifecycle calls. Not phases. |
| `Update` | A group's normal run, once per frame. |
| `LateUpdate` | Dropped. Make a group that runs after the one it follows. |
| `Fixed` | A group property, see rule 3. |
| `Render` | An ordinary group with `MainThread` affinity. |
| `Async` | Not part of the frame graph, see rule 4. |
| `Custom` | Dropped. Every group is custom. |

`[XGUpdate(Phase = ...)]` becomes `[XGUpdate("GroupName")]`.

#### Rule 3: fixed-rate groups

- A group can declare `TickRate` in Hz. The scheduler keeps an accumulator for it and runs the group 0..N times in a frame, passing the fixed step as `dt`.
- All of a frame's steps run back-to-back as one node in the DAG. Groups that depend on it wait for the last step.
- Steps per frame are capped (`MaxSteps`, default 4). Extra time is dropped, and debug builds log it. `Engine::Run` already caps `dt` at 0.25 s.
- Messages from other groups are drained once, before the first step. Messages inside the group stay synchronous.
- The group exposes an interpolation factor (accumulator / step) so render groups can blend between steps.
- If a fixed group runs zero times in a frame, its undelivered messages carry over ([question 3](#3-cross-group-message-delivery), rule 5).

#### Rule 4: async work

- `Async` is an API, not a phase: work submitted to the worker pool with no frame deadline (asset streaming, path finding, baking).
- Async work runs with no current group, so it may use only `ThreadSafe` systems (question 4, rule 3). Debug builds check this.
- A result comes back as a message to the module that submitted the work, delivered when that module's group next drains.
- Part of phase 7.

#### What this changes in existing code

- `Engine::Run` becomes frame start, graph, frame end. `ScriptTree::Update` and `UpdateActors` are no longer called directly.
- `EventQueue::Clear` and `PollEvents` move into frame start. `Renderer::Render` stays at the end.

### 3. Cross-group message delivery

> Decided (2026-11): synchronous within a group, queued between groups, with a fixed rule for which frame a message arrives in.

Today `Messenger::Send` calls `OnMessage` on every receiver right away, on the sender's thread. `ScriptMessage` only borrows its `TypeName` and `Payload` pointers, which usually point at the sender's stack or a pinned managed array. Receivers (`ScriptModuleCoreCLR::OnMessage`, `ScriptModuleNative::OnMessage`) copy the payload for the duration of the call. Once groups run in parallel, a direct call from one group into another breaks [question 4](#4-data-hazards-between-parallel-groups), and a borrowed pointer doesn't outlive `Send`.

#### Rule 1: messages within a group stay synchronous

- If the sender and the receiver are in the same group, `OnMessage` is called right away, as it is today.
- Modules inside one group run one after another, so this is safe and keeps the current behavior and latency.

#### Rule 2: messages between groups are queued

- `Send` resolves the route on the sender's thread and splits the recipients by group. The ScriptTree doesn't change during a frame (question 4, rule 4), so reading it from any group is safe.
- For recipients in other groups, the message is copied once into the sender group's **outbox**: type name, payload, source id and the list of recipient module ids. The copy lives in a frame arena that belongs to the sender group, so fan-out doesn't copy the message again for each recipient.
- Messages are fire-and-forget. A reply is another message, with the same timing rules.

#### Rule 3: which frame a message arrives in

Each group's outbox is double-buffered: one buffer for this frame and one for the previous frame. When group `R` starts, before any of its modules update, it drains messages addressed to it:

| Sender group `S` | `R` receives |
|---|---|
| An ancestor of `R` in the DAG (finished earlier this frame) | `S`'s outbox from **this** frame |
| Not an ancestor (runs in parallel with `R`, or after it) | `S`'s outbox from the **previous** frame |

- The rule depends only on the graph, never on which thread finished first. A parallel sender's messages always arrive one frame later, even when it happened to finish before `R` started.
- Downstream groups see upstream messages in the same frame, which covers the common case (`Gameplay` → `Animation`).
- Messages sent upstream or to a parallel group arrive next frame. This is documented behavior, not a bug.

#### Rule 4: delivery order

- `R` reads the outboxes in the graph's topological order. Ties between groups break by declaration order, so the topological order is the same on every run.
- Within one outbox, messages keep their send order. Modules inside a group run serially, so that order is deterministic too.
- Recipients of a single message are called in tree order, as today.

#### Rule 5: lifetime and edge cases

- **Payload lifetime:** the pointer handed to `OnMessage` is valid only during the call. This is the current contract, now written down. The arena buffer is reset when it's reused two frames later.
- **Unloaded modules:** recipients are stored by module id. Ids increase and are never reused, so a module unloaded before delivery is simply skipped.
- **Sends outside any group** (during `Init`, or between frames): they go into an engine outbox, delivered first at the start of the next frame.
- **Groups that skip a frame** (a `Fixed` group that ticks zero times, phase 7): their undelivered messages are carried into a pending list at the end of the frame, so nothing is lost when the outbox buffers rotate.
- **Limits:** each arena has a size budget. Debug builds log when a group exceeds it.

#### What this changes in existing code

- `MessengerImpl` gets per-group outboxes and a `Drain(group)` step that the scheduler calls before a group updates.
- `XG_SendMessageToEngine_Route` must call `Messenger::Send(msg, route)` instead of `SendToAll`. Today managed routed sends reach every module.
- `ScriptMessage` and `Messenger`'s interface don't change, so no ABI break. Receivers' per-call payload copies become redundant and can go later.
- Structural commands (question 4, rule 4) use their own buffers, not the message outboxes, because they're applied at sync points instead of being delivered to modules.

#### Rejected alternatives

- **Queue everything, even within a group:** one simple rule, but same-group messages would arrive a frame late and today's behavior would change.
- **Deliver as soon as the receiver starts, whatever the sender:** lower latency, but whether a parallel sender's message arrives this frame would depend on thread timing.
- **Lock-free direct calls into other groups:** breaks single ownership, and receivers would run on the sender's thread.

### 4. Data hazards between parallel groups

> Decided (2026-11): ownership by default, declared reads checked by the graph builder, deferred structural changes, and debug-only enforcement.

Ordering prevents two dependent groups from overlapping, but `AI` and `Physics` in the branching example can still touch the same scene graph, actor or system at once. Without a rule, "automatic parallelism" turns into data races.

#### Rule 1: every system has one owner group

- A system's owner is the dispatch group of the ScriptNode it's registered on. Systems are already registered per node (`RegisterSystemImpl(module, typeId, system)`), so ownership comes from where it's registered, with no extra declaration.
- During a frame, only the owner group may mutate an owned system.
- Other groups change it by sending messages or commands, which the owner applies when it next runs ([question 3](#3-cross-group-message-delivery)).

#### Rule 2: reads from other groups are declared, and the builder checks them

- A group declares the systems it reads but doesn't own, by type or alias: `Reads("TransformGraph")`, `[XGReads(typeof(TransformGraph))]`.
- When the graph is built, every declared read must have a path in the DAG to or from the owner group, so the reader never runs at the same time as the writer. If there's no path, the build fails with both group names and the system name.
- The builder never adds edges by itself. A silently added edge removes parallelism with no visible trace, so the user adds the edge explicitly.
- Several readers of the same system may run in parallel with each other.

#### Rule 3: a system declares a concurrency policy

`System` gets an appended virtual `GetConcurrency()` that returns one of:

| Policy | Meaning | Engine systems |
|---|---|---|
| `Owned` (default) | Rules 1 and 2 apply | scene graphs, component sets |
| `ThreadSafe` | Any group may call it at any time. The implementation is responsible. | `Messenger` (once it has mailboxes), `Logger` |
| `MainThreadOnly` | Only `MainThread`-affinity groups may use it | `EventQueue`, `EventDispatcher`, `Renderer` |
| `Deferred` | Calls are recorded and applied at a sync point | `ActorRegistry` structural calls, `SceneGraphFactory` |

#### Rule 4: structural changes are deferred

- Creating or destroying actors, `RegisterNode`, `DestroyNode`, `SetParent`, graph membership changes and module loads change shared structure that every group sees.
- They go into a command buffer for each group. The buffers are applied at a sync point in a fixed order (group order, then module id, then sequence), so the result doesn't depend on which thread finished first.
- Sync points: the end of each frame by default. A group can ask for its buffer to be applied when it finishes, so downstream groups in the same frame see the change.
- ID generation (`ActorRegistry::GenerateID`) stays immediate and atomic, so a group can refer to an actor it just queued for creation.

#### Rule 5: checks run only in debug builds

- The scheduler keeps the current group in a `thread_local` variable while a group runs.
- `GetSystemImpl` checks the caller's group against the owner, the declared reads and the policy, and logs a violation with both group names.
- Modules cache system pointers in `Init`, so a check at lookup time isn't enough. `Owned` systems also assert the owner in their mutating calls through a helper macro (`XG_ASSERT_OWNER(this)`).
- Release builds compile all of this away.

#### What this changes in existing code

- `game.kernel` registers its scene graphs on the root (`RegisterSystem<SceneGraph>(nullptr, ...)`), which would make the engine their owner. It should register them on its own node or on the node of the group that writes them.
- `ActorRegistryImpl` mixes immediate lookups (safe to read in parallel) with structural changes (must be deferred). Its interface splits along that line.
- Managed component types follow the same rules: a component type belongs to its group, and reading another group's component data needs `[XGReads]`. Component update groups inside one group stay serial until a later phase.

#### ABI additions (all appended, C types only)

- `System::GetConcurrency()` returning a `uint8_t` enum.
- `ScriptEngine::DeclareAccess(ScriptModule* module, const char* group, const char* systemAlias, uint8_t access)`.
- A C export for the managed side to declare reads collected from attributes.

#### Rejected alternatives

- **Access declarations only:** every group would have to list everything it touches, including writes, which is heavy for gameplay code and easy to get wrong.
- **Convention only:** no checks at all. The first data race shows up as a random crash far from its cause.
- **Locks inside every system:** hides ordering problems, makes results depend on timing and costs performance on the hot path.

### 5. Main-thread constraints

The window, SDL event polling, ImGui and the renderer's swapchain must stay on the main thread. `Input`, the editor UI and `Render` therefore need `MainThread` affinity, and the frame loop has to keep pumping the main thread while workers run. The editor kernel's UI must be its own main-thread group.

### 6. Managed runtime threading

CoreCLR and NativeAOT attach native threads automatically on the first call into managed code, so calling a managed entry from a worker works. Managed static state (`ScriptRuntime`, `ActorRegistry.cs`, `ComponentUpdateGroup<T>.Instance`, `Debug`) is not thread-safe, and the logger and log callbacks are called from every thread. Each needs a decision: lock, make it per-group, or keep it main-thread only.

### 7. Where the graph is defined

The API examples build the graph in code. Who calls that code, and when? Options: `gameconfig.json`, a graph file per game, or the root game module at `Init`. A data file keeps the graph editable in the editor and visible without running code. Group names should be interned to ids when the graph is built, so names never cross the ABI per frame.

### 8. Naming

The file is "dispatch group", the old title was "Dispatch Graph", and the code has `DispatchSystem`, `ThreadDomain` and a host "group". Proposal: **Dispatch Graph** (the structure), **dispatch group** (a node), **affinity** (replaces `ThreadDomain`), **Scheduler** (the runtime). Rename the existing host group to "host group" in code.

### 9. Hot reload and dynamic modules

Modules can load at runtime, and the editor plans hot reload. Adding or removing a module, or a group, while the scheduler runs needs a safe point, most likely between frames, plus a graph rebuild.

---

## Implementation phases

Each phase leaves the engine running and testable with the existing `[Debug]` and `[Actors]` output.

| # | Phase | Result |
|---|---|---|
| 1 | Fix `ScriptTree::Update` double updates. Separate the tree (ownership, systems) from execution, so nodes no longer recurse during updates. | Every module updates exactly once per frame. |
| 2 | `DispatchGraph` in xgCore: groups, edges, topological sort and cycle detection. Load it from config. Split `Engine::Run` into frame start, graph and frame end. Every module runs in `Default`, serially on the main thread. | The same behavior as today, now driven by the graph. |
| 3 | `ScriptEngine::JoinGroup`, `ScriptModule::UpdateGroup` and the optional `xg:UpdateGroup` export. Rename the host group. | Native modules run in user-defined groups and order. |
| 4 | Fix `XG_SendMessageToEngine_Route`. Per-group outboxes with the question 3 frame rule and drain order. System ownership, `GetConcurrency()`, declared reads with build-time checks, deferred structural commands, debug checks. | Messaging and data access are safe, and checked, before any thread exists. |
| 5 | Worker pool scheduler. Run independent groups in parallel and honor `MainThread` affinity. Replace `DispatchSystem`. | First real parallelism, built on the question 4 rules. |
| 6 | Managed `UpdateGroup`: `[XGDispatchGroup]`, `[XGUpdate("Group")]` and `[XGReads]`. Component updates move into module entries, and `ActorRegistry` update callbacks go away. Thread-safety pass on managed statics. | C# code takes part per group. |
| 7 | `TickRate` groups with accumulator and interpolation factor, the `Async` API, dedicated and pinned threads. | The full affinity table. |
| 8 | Tooling: graph dump in the log, per-group timings, a graph view in the editor. | The schedule is visible and debuggable. |

Phases 1 to 4 are single-threaded and low-risk. They also make phase 5 a scheduler change instead of an engine-wide rewrite.
