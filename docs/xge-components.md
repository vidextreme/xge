# XGE Actors and Components (C#)

> Status: current implementation (2026-11). Future design is listed at the end. See the [README](../README.md) for the overall status table.

Gameplay code is written in C# against `xgSharedKit.Managed`, and runs the same way in CoreCLR modules (editor) and NativeAOT modules (runtime). Native actor identity and scene graphs are covered in [SceneGraph and Actors](xge-scenegraph.md).

| Piece | State |
|---|---|
| `Actor` API | Working |
| Behaviour components (`IComponentUpdate<T>`, CRTP) | Working, updated every frame |
| Data components (`ComponentData<TData>`, `ComponentDataSet<...>`) | Prototype: not bound to native memory, `OnDataUpdate` not scheduled |
| Native component storage (`CompositeSceneGraph`) | WIP |

---

## Actor

```csharp
Actor actor = Actor.Create("player");
actor.Name = "hero";

var player = actor.AddComponent<PlayerComponent>();
var same   = actor.GetComponent<PlayerComponent>();
actor.RemoveComponent<PlayerComponent>();

actor.Destroy();
```

- `Actor.Create` asks the engine for a new global `ActorID` (`XGCreateActor` / `XGCreateActorWithName`) and registers the managed `Actor` in the managed `ActorRegistry`.
- Components are plain C# objects (`where T : class, new()`). `ActorRegistry.GetOwner(component)` returns a component's actor.
- `Destroy` detaches every component, unregisters the actor and destroys it natively. Using a destroyed actor throws `ObjectDisposedException`.
- `ActorRegistry.Count`, `Actors`, `TryGet(id, out actor)` and `FrameCount` are available for queries.

---

## Behaviour components (CRTP)

A component that needs per-frame logic implements `IComponentUpdate<T>`, where `T` is the component's own type (the curiously recurring template pattern):

```csharp
public class EnemyComponent : IComponentUpdate<EnemyComponent>
{
    public int UpdateCount;
    public float ElapsedTime;

    public static void OnUpdate(EnemyComponent component, float deltaTime)
    {
        component.UpdateCount++;
        component.ElapsedTime += deltaTime;
    }
}
```

How it runs:

1. `AddComponent<T>` calls the interface's `Attach`, which adds the component to `ComponentUpdateGroup<T>.Instance`, one group per component type. The group registers itself with the managed `ActorRegistry` on first use.
2. Each group caches `&T.OnUpdate` in a `delegate* managed<T, float, void>`. Updating a group is a tight loop of direct calls: no virtual dispatch, no boxing and no reflection per frame. This also works under NativeAOT trimming.
3. Every frame, `Engine::Run` calls native `ActorRegistry::UpdateActors(dt)`, which calls each C# module's callback registered through `XGRegisterActorUpdate`. That callback runs `ActorRegistry.Update(dt)`, which updates every group.
4. `RemoveComponent` / `Destroy` only null out the slot during the pass. `FlushRemovals` compacts each group after all groups have updated, so removing components inside `OnUpdate` is safe.
5. If a group's `OnUpdate` throws, the error is logged once through `Debug.LogError` and the other groups keep running.

---

## Data components (prototype)

Data components separate blittable data from behaviour, aimed at native, cache-friendly storage.

```csharp
public struct CharacterData : IComponentData
{
    public int Health;
    public float Stamina;
}
```

| Type | Role |
|---|---|
| `IComponentData` | Marker for blittable data structs (checked by `BlittableValidator`) |
| `ComponentData<TData>` | Holds one `TData`. `ref TData Data` gives in-place access, and `DataCopy` returns a copy. `Refresh()` copies native memory into `Data`, `Commit()` copies `Data` back. |
| `BeginEdit()` | Returns a scope that optionally refreshes first and calls `Commit()` when disposed |
| `ComponentDataSet<T1>` / `<T1, T2>` | Base class for a component holding one or two data blocks (`D1`, `D2`) |
| `IComponentDataUpdate<TData>` | `static abstract void OnDataUpdate(ComponentData<TData> comp, float dt)` |

Current limits:

- `ComponentData<TData>` is never bound to native memory yet (`BindHandle` is a stub), so `Refresh` / `Commit` do nothing, and `ComponentDataSet`'s `D1` / `D2` are not created automatically.
- No scheduler calls `OnDataUpdate`. Only `IComponentUpdate<T>.OnUpdate` runs every frame.
- Always take `Data` by reference (`ref var data = ref cd.Data;`); `var data = cd.Data;` copies the struct and loses the writes.
- An older shape, `Component<TData> : ComponentOLD` with `ComponentHandle<T>` and `ScriptRuntime.AllocateComponent<T>`, is still in the code and will be removed when the data path settles.

See the [hybrid component sample](xge-component-sample.md) for a component that uses both update paths.

---

## Native storage (WIP)

- `CompositeSceneGraph` (`"compset"` in the `SceneGraphFactory`) keeps one `StridePool` per component type: a dense byte buffer with a parallel array of owner ActorIDs, and swap-and-pop removal.
- `XGAllocateComponent(engine, actorID, componentType, size)` allocates a block for an actor.
- Pointers into the pool aren't stable yet (the buffer can grow, and removal moves other actors' data), which is why the managed data components aren't bound to it.

See [Component graphs](xge-components2.md) for how per-type component graphs fit the scoped subsystem model.

---

## Future design

Not implemented yet:

- **Archetype component graph:** prefab-defined actors whose components share one stride (`Stride = sum(sizeof(TData_i))`), stored in chunks with free lists.
- **Stable dynamic storage:** per-type pools with free lists and stable handles, bound to `ComponentData<TData>`.
- **Data update scheduling:** iterate data blocks per type and call `OnDataUpdate`, including from worker threads.
- **Cross-actor lookup:** for example `World.GetComponent<T>(actorId)`, through an ActorID → storage index → managed instance map.
- **Reflection-driven editing:** `[Editable]` fields shown in the editor's property panel and serialized (the attribute exists in `TestCode.cs`; nothing reads it yet).
- **Native systems** (physics, rendering, animation, audio) operating on the same component data from C++.
- **NativeAOT direct access:** native component memory handed to NativeAOT modules as `void*` with fixed offsets and no marshalling.
