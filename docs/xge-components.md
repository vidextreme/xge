# XGE Actors and Components (C#)

> Status: current implementation (2026-11). Future design is listed at the end. See the [README](../README.md) for the overall status table.

Gameplay code is written in C# against `xgSharedKit.Managed`, and runs the same way in CoreCLR modules (editor) and NativeAOT modules (runtime). Native identity, the scene graph and transforms are covered in [SceneGraph and Actors](xge-scenegraph.md); the native data pools behind data components are in [Native component data](xge-components2.md).

| Piece | State |
|---|---|
| `Actor` API | Working |
| Object components (any object, `IComponent`, `Component`) | Working, including runtime attachment |
| Behaviour updates (`IComponentUpdate<T>`, CRTP) | Working, every frame in play |
| Native `Transform` (optional per actor) | Working: proxy over the engine's `TransformSystem` |
| Data components (`ComponentDataSet<...>`, `IComponentData`) | Working: data lives in native pools |
| Data updates (`IComponentDataUpdate<...>`) | Working: inline or as parallel job ranges, with command buffers |
| Scheduling data updates by declared access across groups | Planned |

---

## Actor

```csharp
Actor actor = Actor.Create("player");
actor.Name = "hero";

var player = actor.AddComponent<PlayerComponent>();
var same   = actor.GetComponent<PlayerComponent>();
actor.RemoveComponent<PlayerComponent>();

actor.Parent = otherActor;          // keeps the world pose when the actor has a Transform
actor.Destroy();                    // destroys its children too
```

- `Actor.Create` asks the engine for a new global `ActorID` (`XG_CreateActor` / `XG_CreateActorWithName`) and registers the managed `Actor` in the module's `ActorRegistry`.
- `Destroy` detaches every component, unregisters the actor and destroys it natively. Native destruction takes the whole subtree, children first; every module hears about each destroyed actor through a destroy listener and releases its own `Actor` object. Using a destroyed actor throws `ObjectDisposedException`.
- `ActorRegistry.Count`, `Actors`, `TryGet(id, out actor)`, `GetOwner(component)` and `FrameCount` are available for queries. `ActorTree` reads the hierarchy across every module (`GetRoots`, `GetChildren`, `GetParent`, `SetParent(child, parent, keepWorld)`, `GetComponents`).

---

## Components

A component is **any object** attached to an actor. Nothing has to derive from a base class:

```csharp
actor.AddComponent<Health>();                 // a new instance (public parameterless constructor)
actor.AddComponent(typeof(Health));           // same, by type
actor.AddComponent(new PathAgent(navMesh));   // runtime attachment of an existing object
```

- An object has **one owner** at a time. Attaching an object that is already on another actor throws; remove it there first.
- `GetComponent<T>()` matches by `is T`, so interfaces work.
- `ActorRegistry.GetOwner(obj)` returns the owner of any attached object.

### `IComponent` and `Component`

Both are optional:

| Type | What it adds |
|---|---|
| `IComponent` | Default methods `OnAttached(Actor)` and `OnDetached()`, called on attach and removal. Good for managers or types with their own base class. |
| `Component` (abstract class) | Implements `IComponent` and keeps the owner: `Actor`, `ActorID`, `Transform`, `GetComponent<T>()`, `IsAttached`, plus protected virtual `OnAttached()` / `OnDetached()`. |
| `[RequireComponent(typeof(T))]` | `AddComponent` adds `T` first when the actor lacks it (`Transform` included). |

```csharp
public class SpawnManager : MyServiceBase, IComponent          // own base class, still attachable
{
    private Actor? _host;
    void IComponent.OnAttached(Actor actor) => _host = actor;
    void IComponent.OnDetached() => _host = null;
}

[RequireComponent(typeof(Transform))]
public class Follower : Component
{
    protected override void OnAttached() => Transform!.LocalPosition = new Vector3(0, 1, 0);
}
```

The editor's Add Component list shows public, non-abstract types with a parameterless constructor that have `[ComponentIcon]` or implement `IComponent` / `IComponentUpdate`, plus `Transform`. Other types can still be attached from code.

---

## Behaviour updates (CRTP)

A component that needs per-instance logic every frame implements `IComponentUpdate<T>`, where `T` is its own type:

```csharp
public class EnemyComponent : IComponentUpdate<EnemyComponent>
{
    public int UpdateCount;

    public static void OnUpdate(EnemyComponent component, float deltaTime) => component.UpdateCount++;
}
```

1. Attaching adds the component to `ComponentUpdateGroup<T>.Instance`, one group per type.
2. Each group caches `&T.OnUpdate` in a `delegate* managed<T, float, void>`: a tight loop of direct calls, no virtual dispatch, boxing or reflection per frame. NativeAOT-safe.
3. Every frame `Engine::Run` calls `ActorRegistry::UpdateActorsPerModule`, which calls each module's callback (`XG_RegisterActorUpdate`) and so `ActorRegistry.Update(dt)`. In the editor this only happens in play mode.
4. Removals during the pass are deferred until every group has run.
5. A throwing group is reported once; the others keep running.

`OnUpdate` runs on the main thread and may do anything: touch other actors, add components, destroy actors.

---

## Transform

Transforms are optional and native. `Transform` is a thin proxy that holds only the actor id; every read or write goes to the engine's `TransformSystem`, so all modules see the same values and nothing needs saving across a hot reload.

```csharp
Transform t = actor.AddTransform();               // or AddComponent<Transform>()
t.LocalPosition = new Vector3(1, 2, 3);
t.LocalEulerAngles = new Vector3(0, 90, 0);       // degrees: x pitch, y yaw, z roll
Vector3 world = t.Position;
t.SetParent(parent.Transform, keepWorld: true);
actor.RemoveTransform();
```

| Member | Notes |
|---|---|
| `Local`, `World` | Whole poses as `TransformData` (position, quaternion, scale; 40 bytes, the native layout) |
| `LocalPosition`, `LocalRotation`, `LocalScale`, `LocalEulerAngles` | Local pose |
| `Position`, `Rotation`, `LocalToWorld` | World pose and matrix (row vectors, `v * M`) |
| `GetLocal` / `SetLocal(position, rotation, scale)` | One call for the whole local pose |
| `Parent`, `SetParent(parent, keepWorld = true)` | Nearest ancestor with a transform; reparenting goes through the hierarchy |
| `Transform.Of(id)`, `Exists(id)`, `IsValid` | Proxies for any actor, including other modules' |
| `GetLocals` / `SetLocals` / `GetWorlds` / `SetWorlds` | Span batches |
| `CurrentVersion`, `CollectChanged(since, list)` | Change tracking, e.g. for a physics sync |
| `FromEuler`, `ToEuler`, `GetActors`, `Snapshot` | Helpers; `Snapshot` is what play mode restores at Stop |

`actor.Transform` is null when the actor has none, and `GetComponent<Transform>()` / `RemoveComponent<Transform>()` route to the same calls. The Inspector shows the transform as the first card (Euler angles stay as typed while the rotation doesn't change elsewhere).

---

## Data components

Data components keep blittable structs in **native memory**: one tightly packed pool per struct type, owned by the engine and keyed by actor. The component class holds behaviour and managed state; its data is reached by reference, with no copies and nothing to commit.

```csharp
public struct NPCData : IComponentData
{
    public float Health;
    public float Awareness;
    public float AwarenessGainRate;
    public float HealthRegenRate;
}

public class NPCComponent : ComponentDataSet<NPCData>
{
    public void Heal(float amount) => Data1.Health += amount;     // ref NPCData into the pool
}
```

- `ComponentDataSet<T1>` up to `<T1, T2, T3, T4>` derive from `Component`. Attaching adds one reference to the actor's element of each declared type (created zero-filled); detaching releases it. Two components declaring the same struct share the element.
- `Data1` … `Data4` return `ref Ti` straight into the pool. Elements move when others are removed, so don't keep pointers or `ref` locals across frames; reading `DataN` again is cheap.
- Structs are `unmanaged` and implement `IComponentData`. The type registers itself on first use in each module (key `"Full.Name, Assembly"`, with its field layout).
- Raw API without a component: `ComponentData.Get<T>(actor)` (ref), `Has`, `TryGet`, `Add` (one reference), `Release`, `Remove` (whatever the count) and `All<T>(out owners)` (the whole pool as a span).
- The Inspector draws each data struct under the component's own fields and writes edits back.
- **Hot reload:** pools are shared, so data survives. If a struct's layout changes, the registry migrates the pool in place: fields matched by name, type and size are kept, the others start at zero, and a warning lists dropped fields.
- **Play mode:** every script data pool is captured at Play and restored at Stop; elements created during play are removed.

---

## Data updates (ECS-style, parallel)

A data component can update **every element** of one of its struct types in a dense loop by implementing `IComponentDataUpdate<TSelf, TWrite, ...>`. `TSelf` is the component's own type (as in `IComponentUpdate<T>`; this keeps registration free of reflection for NativeAOT), `TWrite` is the struct it updates, and up to three more type arguments are read-only inputs:

```csharp
public class NPCComponent : ComponentDataSet<NPCData, LiftData>,
                            IComponentDataUpdate<NPCComponent, NPCData>,
                            IComponentDataUpdate<NPCComponent, LiftData, TransformData>
{
    public static void OnDataUpdate(ref NPCData data, in DataUpdateContext context)
    {
        data.Awareness += data.AwarenessGainRate * context.DeltaTime;
    }

    public static void OnDataUpdate(ref LiftData lift, in TransformData transform, in DataUpdateContext context)
    {
        lift.Height = transform.PositionY;
    }
}
```

How it runs:

1. Attaching a `ComponentDataSet` registers each data update whose `TWrite` the component declares. Each update is its own group with its own read set.
2. Groups run from `ActorRegistry.Update` after the behaviour updates (in the editor: play mode only). One module updates a data type: the first to claim it; later claims are ignored with a warning.
3. The group walks the **whole** `TWrite` pool, whichever component added each element. Reads are matched by actor; an element whose actor lacks a read type is skipped for that update.
4. Below 2048 elements the update runs inline; above, it is split into 512-element ranges scheduled as jobs on the worker threads, and the main thread waits for them.
5. `DataUpdateContext` carries `DeltaTime`, the element's `Actor` and the range's `CommandBuffer`.
6. Afterwards the command buffers are played back on the main thread in range order (deterministic), and the pool is stamped as changed.
7. A throwing update is reported once.

Rules inside `OnDataUpdate`:

- Write only your own element. Reads come in as `in` parameters.
- Structural changes (adding or removing components or data, destroying actors) and transform writes throw during the pass and off the main thread (`ScriptThread.CheckMain`). Record them instead:

| `CommandBuffer` call | Effect at playback |
|---|---|
| `SetData<T>(actor, value)` | Writes the actor's element, if it has one |
| `AddData<T>(actor, value)` | Adds a reference (creating the element) and writes it |
| `RemoveData<T>(actor)` | Removes the element |
| `SetLocalTransform` / `SetWorldTransform(actor, value)` | Sets the transform |
| `Destroy(actor)` | Destroys the actor (and its children) |
| `Invoke(action)` | Runs the action on the main thread |

- Engine-owned types such as `TransformData` can be reads but never `TWrite`.

See the [hybrid component sample](xge-component-sample.md) for a component that uses both update paths.

---

## Future design

Not implemented yet:

- **Declared-access scheduling:** build job dependencies from each group's read and write types so groups that don't conflict run at the same time (the read types are already part of the signature).
- **`ComponentDataLookup<T>`:** read-only random access to other actors' elements inside a data update.
- **Double-buffered pools:** read the previous frame and write the current one, for updates that read neighbouring elements.
- **Native component sources:** pools owned by native modules (physics bodies) shown in the Inspector through their reflection schema.
- **Archetypes:** prefab-defined actors with a fixed set of data types in one block, if profiling ever asks for it.
- **Native systems** (physics, rendering, animation, audio) registering their own updates on the same pools from C++.
