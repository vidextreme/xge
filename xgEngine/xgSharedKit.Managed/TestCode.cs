// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg
{
    // =========================================================================
    // Example payload and component usage
    // =========================================================================
    [StructLayout(LayoutKind.Sequential)]
    public struct CharacterData : IComponentData
    {
        public CharacterStats Stats;
        public PassiveSkill Skill;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct PassiveSkill : IComponentData
    {
        public int SkillLevel;
        public int SkillType;
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct CharacterStats : IComponentData
    {
        public int Health;
        public float Stamina;
    }

    [ComponentIcon("Warning", Color = "#D6AA3C")]
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
    [ComponentIcon("Account", Color = "#5585D0")]
    public class PlayerComponent : ComponentDataSet<CharacterData>, IComponentUpdate<PlayerComponent>, IComponentDataUpdate<PlayerComponent, CharacterData>
    {
        public static void OnDataUpdate(ref CharacterData data, in DataUpdateContext context)
        {
            data.Skill.SkillLevel = 100;
        }

        public int UpdateCount;
        public float ElapsedTime;
        [Range(0, 1)]
        public float TargetTime = 5;

        public static void OnUpdate(PlayerComponent component, float deltaTime)
        {
            component.UpdateCount++;
            if (component.ElapsedTime < component.TargetTime)
            {
                component.ElapsedTime += deltaTime;
                if (component.ElapsedTime >= component.TargetTime)
                    Debug.LogSuccess("Great Success!");
            }
        }
    }

    public class EditableAttribute : Attribute
    {
    }

    public struct NPCData : IComponentData
    {
        public float Health;
        public float Awareness;
        public float AwarenessGainRate;
        public float HealthRegenRate;
    }

    // Hybrid component: a per-instance update (managed state) plus an ECS-style update over every NPCData element.
    [ComponentIcon("Object", Color = "#6A9A48")]
    public class NPCComponent : ComponentDataSet<NPCData>,
                                IComponentUpdate<NPCComponent>,
                                IComponentDataUpdate<NPCComponent, NPCData>
    {
        [Editable] private float _decisionTimer;
        [Editable] private bool _alerted;

        public static void OnUpdate(NPCComponent npc, float dt)
        {
            npc._decisionTimer += dt;
            ref NPCData data = ref npc.Data1;
            npc._alerted = data.Awareness > 0.5f;
            data.AwarenessGainRate = npc._alerted ? 0.3f : 0.1f;
            data.HealthRegenRate = npc._alerted ? 0.0f : 0.05f;
        }

        public static void OnDataUpdate(ref NPCData data, in DataUpdateContext context)
        {
            data.Awareness += data.AwarenessGainRate * context.DeltaTime;
            data.Health += data.HealthRegenRate * context.DeltaTime;
        }
    }
}
