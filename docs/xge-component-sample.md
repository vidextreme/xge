# Hybrid Component Sample

> Status: sample of the current API (2026-11). The data-update half is a prototype; see the notes below.

A hybrid component combines both component paths from [Components](xge-components.md):

- **Behaviour** through `IComponentUpdate<T>`: a CRTP `static OnUpdate(T, float)` that the engine calls every frame for every instance.
- **Data** through `ComponentDataSet<TData>` and `IComponentDataUpdate<TData>`: blittable `IComponentData` structs meant to live in native storage, updated by `static OnDataUpdate`.

The same code is in `xgEngine/xgSharedKit.Managed/TestCode.cs`.

```csharp
public struct NPCData : IComponentData
{
    public float Health;
    public float Awareness;
    public float AwarenessGainRate;
    public float HealthRegenRate;
}

public class NPCComponent : ComponentDataSet<NPCData>,
                            IComponentUpdate<NPCComponent>,
                            IComponentDataUpdate<NPCData>
{
    [Editable] private float _decisionTimer;
    [Editable] private bool _alerted;

    public static void OnUpdate(NPCComponent npc, float dt)
    {
        npc._decisionTimer += dt;

        ref var npcData = ref npc.D1.Data;
        npc._alerted = npcData.Awareness > 0.5f;
        npcData.AwarenessGainRate = npc._alerted ? 0.3f : 0.1f;
        npcData.HealthRegenRate = npc._alerted ? 0.0f : 0.05f;
    }

    public static void OnDataUpdate(ComponentData<NPCData> cd, float dt)
    {
        using (cd.BeginEdit())
        {
            ref var data = ref cd.Data;
            data.Awareness += data.AwarenessGainRate * dt;
            data.Health += data.HealthRegenRate * dt;
        }
    }
}
```

## Notes

- **Take data by reference.** `ComponentData<TData>.Data` returns `ref TData`. Writing `var data = cd.Data;` copies the struct and every write is lost, so the sample uses `ref var`. (The older `standard-ecs.png` image still shows the `var` version.)
- **`BeginEdit()`** refreshes `Data` from native memory first, and the returned scope calls `Commit()` on dispose, copying `Data` back to native memory. There is no command queue.
- **What runs today:** `OnUpdate` runs every frame once the component is added with `actor.AddComponent<NPCComponent>()`. `OnDataUpdate` is not scheduled yet, `D1` isn't created automatically, and data blocks aren't bound to native storage, so the data half is a design preview.
- **`[Editable]`** is declared in `TestCode.cs`; no editor panel reads it yet.
