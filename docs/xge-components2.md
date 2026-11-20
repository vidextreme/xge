# XGE Native Component Data

> Status: current implementation (2026-11). Future design is listed at the end. See the [README](../README.md) for the overall status table.

Component data that systems iterate (transforms, script data structs, later physics bodies and render proxies) lives in **native pools owned by the engine**: one tightly packed pool per data type, keyed by the global `ActorID`. Actors stay plain identities in the scene graph; every module attaches its own data to the same id. The C# side is described in [Components](xge-components.md).

| Piece | State |
|---|---|
| `ComponentDataRegistry` (pools, versions, listeners) | Working |
| Layout migration on re-registration | Working |
| `TransformSystem` on the `"xg.Transform"` pool | Working |
| Script data types from C# (`IComponentData`) | Working |
| Native modules registering their own data types and updates | API ready, no module uses it yet |

---

## ComponentDataRegistry

`xg::ComponentDataRegistry` (`xgBase/public/xgComponentData.h`, implemented by `xgCore/ComponentDataRegistryImpl.*` over `xgCore/ComponentPool.*`) is a root subsystem shared by every module.

- **Types:** `RegisterType(moduleId, DataTypeDesc)` with `Key`, `Size`, `Align` and `LayoutHash` returns a `DataTypeID`. Registering an existing key with the same layout returns the same id. `RegisterTypeWithLayout` also takes a field layout (`"name:type:offset:size;..."`).
- **Flags:** `EngineOwned` types (such as `"xg.Transform"`) are written only through their system. `Shared` types stay registered when their module unloads (C# script types use it, so their data survives hot reload). Other types are unregistered with their module.
- **Elements:** at most one per actor and type. `Add` adds a reference and creates the element zero-filled; `Release` drops one and removes the element at zero; `Remove` removes it whatever the count. Destroying an actor removes it from every pool.
- **Storage:** dense, swap-and-pop. `GetDense(type, &owners, &count)` returns the packed elements (stride = size rounded up to alignment) and their owners in slot order. Elements move on removal, so never keep a pointer or slot across an add or remove.
- **Change versions:** every add or write stamps the element with the next value of a global counter. `CollectChanged(type, since, ...)` lists actors changed after a version, so any number of consumers can track changes independently. Writers that bypass the registry call `MarkChanged` / `MarkAllChanged`.
- **Listeners:** `DataPoolListener` with `Added`, `Removed` and `Moved` callbacks lets a system keep columns parallel to a pool (the transform system's world matrices).
- **Updaters:** `ClaimUpdater(type, moduleId)` lets one module own a type's update; claims are released with the module.
- **Joins:** `Join(type, other, slots)` writes, for each element of `type`, the slot of the same actor's element of `other` (or -1). Data updates use it to match their read types.
- **Migration:** when a type is re-registered with another layout and both layouts are known, the pool is rebuilt in place. The type id and slot order stay, fields matched by name, type and size are copied, the rest are zero, and a warning lists the dropped fields.
- Main thread only. Off-thread work (data update jobs, a physics thread) runs on spans taken at a sync point, while no structural change can happen.

C exports for scripts are in `xgScriptNative/interop/xgComponentDataInterop.cpp` (`XG_ComponentData*`).

---

## TransformSystem

`xg::TransformSystem` (`xgBase/public/xgTransforms.h`, `xgCore/TransformSystemImpl.*`) gives actors an optional transform:

- The local pose is the element of the engine-owned pool `"xg.Transform"`: `xg::TransformData` (position, quaternion x/y/z/w, scale; 40 bytes, the same layout in C# as `TransformData`).
- World matrices and dirty flags are columns kept parallel to that pool through its listener.
- **There is no second hierarchy.** Parenting is the root `SceneGraph`'s; `HierarchyNode::TransformIndex` links an actor's node to its slot so the world pass needs no lookups. An actor without a transform passes its parent's world matrix on to its children.
- Matrices are row-major for row vectors: `world = Scale * Rotation * Translation * parentWorld`.
- Setters compute the actor's own world matrix immediately and mark the transformed descendants dirty. `Update()` runs in `Engine::Run` after the actor updates and recomputes what is still dirty, parents first; `GetWorld` recomputes on demand when stale.
- `SetParent(child, parent, keepWorld)` reparents in the scene graph; with `keepWorld` the local pose is recomputed so the world pose stays. `XG_ActorSetParent` keeps the world pose; `XG_ActorSetParentEx` takes the flag.
- Batches (`GetLocals`, `SetLocals`, `GetWorlds`, `SetWorlds`), change tracking (`CollectChanged`, `GetCurrentVersion`) and `CopyActors`. C exports `XG_Transform*` live with the actor exports in `xgSharedKit/xgActorSystem.cpp`.

---

## Cross-module use

The registry and the transform system are root subsystems, so every module, native or managed, sees the same pools. A threaded module such as a future physics engine fits like this:

1. It owns its own pools (bodies, colliders) keyed by `ActorID`.
2. On the main thread, at its position in the module tree, it pulls `CollectChanged` transforms into its bodies.
3. It steps on job workers or its own thread, touching only its own data.
4. Back on the main thread, it writes the poses with `SetWorlds`.
5. Destroy listeners (`ActorRegistry::AddDestroyListener`, `XG_ActorRegisterDestroyListener`) keep its pools in step with actors.

---

## Future design

Not implemented yet:

- **Declared-access scheduling** of data updates, native and managed together, from each update's read and write `DataTypeID` sets.
- **Double-buffered pools** for updates that read neighbouring elements.
- **Parallel transform pass:** one job per root subtree, each writing only its own world slots.
- **Read-only world-matrix snapshot** for render and physics threads.
- **Native component sources:** native pools listed in the Inspector through a reflection schema.
- **Archetype storage:** prefab-defined actors with a fixed set of data types in one block.
