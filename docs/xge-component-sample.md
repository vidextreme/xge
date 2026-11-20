# Hybrid Component Sample

> Status: sample of the current API (2026-11).

A hybrid component combines both update paths from [Components](xge-components.md):

- **Behaviour** through `IComponentUpdate<T>`: a CRTP `static OnUpdate(T, float)` that runs every frame for every instance, on the main thread, with full access to managed state.
- **Data** through `ComponentDataSet<TData>` and `IComponentDataUpdate<TSelf, TData>`: a blittable `IComponentData` struct stored in the engine's native pool for its type, updated by `static OnDataUpdate` in a dense loop that may run on several job workers at once.

The same code is in `xgEngine/xgSharedKit.Managed/TestCode.cs`.

```csharp
public struct NPCData : IComponentData
{
    public float Health;
    public float Awareness;
    public float AwarenessGainRate;
    public float HealthRegenRate;
}

[ComponentIcon("Object", Color = "#6A9A48")]
public class NPCComponent : ComponentDataSet<NPCData>,
                            IComponentUpdate<NPCComponent>,
                            IComponentDataUpdate<NPCComponent, NPCData>
{
    [Editable] private float _decisionTimer;
    [Editable] private bool _alerted;

    // Per instance, main thread: decisions that need managed state.
    public static void OnUpdate(NPCComponent npc, float dt)
    {
        npc._decisionTimer += dt;
        ref NPCData data = ref npc.Data1;
        npc._alerted = data.Awareness > 0.5f;
        data.AwarenessGainRate = npc._alerted ? 0.3f : 0.1f;
        data.HealthRegenRate = npc._alerted ? 0.0f : 0.05f;
    }

    // Every NPCData element, possibly in parallel: plain arithmetic on the data.
    public static void OnDataUpdate(ref NPCData data, in DataUpdateContext context)
    {
        data.Awareness += data.AwarenessGainRate * context.DeltaTime;
        data.Health += data.HealthRegenRate * context.DeltaTime;
    }
}
```

## Notes

- **No copies.** `Data1` returns `ref NPCData` into the native pool. Keep it in a `ref` local (`ref NPCData data = ref npc.Data1;`) when you change several fields; `var data = npc.Data1;` copies the struct and the writes are lost. (The older `standard-ecs.png` image still shows the previous, copy-based API.)
- **Order in a frame:** every `OnUpdate`, then every data update, then the command buffers recorded by the data updates are played back.
- **Inside `OnDataUpdate`** change only the element you were given. To touch anything else (another actor, another data type, destroy an actor, spawn something) record it in `context.Commands`; it runs on the main thread after the pass.
- **Reading other data:** add read types to the interface, for example `IComponentDataUpdate<NPCComponent, NPCData, TransformData>` with `OnDataUpdate(ref NPCData data, in TransformData transform, in DataUpdateContext context)`. NPCs without a transform are then skipped by that update.
- **Hot reload and play mode** keep the data: pools survive a reload (with layout migration when the struct changes), and play mode restores them at Stop.
- **`[Editable]`** is declared in `TestCode.cs`; nothing reads it yet. The Inspector shows public and `[SerializeField]` fields, then the data structs.
