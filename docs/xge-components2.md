# XGE Component Graphs

> Status: current implementation (2026-11). Future design is listed at the end. See the [README](../README.md) for the overall status table.

In XGE, **SceneGraphs are systems**: they are scoped, overridable and resolved through the module tree (see [Scoped systems](xge-architecture.md#scoped-systems)). Component storage builds on that: each component type gets its own graph, registered under an alias, and every graph keys its data by the same global `ActorID`.

---

## One graph per component type

The dynamic component graph is `CompositeSceneGraph`, created through the `SceneGraphFactory` as `"compset"`. It implements `ComponentSceneGraph`:

```cpp
class ComponentSceneGraph : public SceneGraph
{
    XG_DECLARE_BASE_SYSTEM(ComponentSceneGraph)
public:
    virtual void InitializeComponentSet(const char* contextName, size_t strideSize) = 0;

    template<typename T>
    T* CreateComponent(ActorID id);   // creates a node, allocates its data block, registers it
};
```

- Each instance stores one component type in a `StridePool`: a dense byte buffer with `stride = sizeof(data)`, plus a parallel array of owner ActorIDs.
- Components are nodes (`ComponentSetNode`) carrying the `SceneNodeComponentSet` mixin (`Context`, `Size`, `Data`).
- Because every graph is a system of the same base type, a module tells them apart by **alias**:

```cpp
auto* players = static_cast<ComponentSceneGraph*>(factory->Create("compset", *actorRegistry));
engine->RegisterSystemWithAlias(module, players, "PlayerComponent");
players->InitializeComponentSet("PlayerComponent", 12);

auto* lookup = engine->GetSystemWithAlias<ComponentSceneGraph>(module, "PlayerComponent");
```

`game.kernel` registers a `"PlayerComponent"` graph like this today, on the root.

From C#, `XGAllocateComponent(engine, actorID, componentType, size)` allocates a block in the graph for that component type.

---

## One ActorID, many representations

- The engine's `ActorRegistry` records each actor's memberships: which graphs hold a node for it (`RegisterGraphNode`, `GetMembership`).
- An actor can be in the tree graph (hierarchy and name) and in any number of component graphs at the same time, and each graph keeps its own layout and rules.
- Systems can cross-reference actors across graphs through the shared ID.

---

## Scoping

Because component graphs are systems:

- a module can register several of them (one per alias),
- children inherit them unless they register their own under the same alias,
- different parts of the module tree can use different storage for the same component type.

Today every graph is registered on the root, so the whole tree shares them.

---

## Current limits

- `StridePool` swap-and-pop removal moves another actor's data without updating its node, and growing the buffer invalidates earlier pointers. Pointers into a pool aren't stable yet.
- The managed `ComponentData<TData>` isn't bound to this storage yet (see [Components](xge-components.md#data-components-prototype)).
- Components can be added and destroyed, but not moved between actors.

---

## Summary

| | Dynamic component graph | Archetype graph |
|---|---|---|
| Status | WIP (`CompositeSceneGraph`) | Planned |
| Composition | One component type per graph | Prefab-defined set of components |
| Layout | Stride = size of one component | Stride = sum of the prefab's components |
| Memory | Dense per component type | Dense per actor, chunked |
| Runtime flexibility | High: add and remove at runtime | Low: fixed by the prefab |
| ActorID | Shared | Shared |

---

## Future design

**Archetype SceneGraph** (not implemented): built from prefab compositions. Each prefab defines a fixed component set and stride, and the graph allocates one contiguous block per actor. Iteration is cache-friendly because every node has the same layout. Intended for stable domains such as rendering, transforms and animation, while the dynamic graphs cover components that change at runtime (physics bodies, AI state, gameplay).

Also planned:

- stable handles and free lists in the dynamic pools,
- transferring components between actors,
- graphs scoped to the module that owns the world instead of the root.
