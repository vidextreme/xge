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

    public class EnemyComponent : IComponentUpdate<EnemyComponent>
    {
        public static void OnUpdate(EnemyComponent component, float deltaTime)
        {

        }
    }
    public class PlayerComponent : ComponentDataSet<CharacterData>, IComponentUpdate<PlayerComponent>, IComponentDataUpdate<CharacterData>
    {

        public static void OnDataUpdate(ComponentData<CharacterData> componentData, float dt)
        {
            using (componentData.BeginEdit())
            {
                componentData.Data.Skill.SkillLevel = 100;
            }
        }

        public static void OnUpdate(PlayerComponent component, float deltaTime)
        {

        }

        CharacterData _data;
        public ref CharacterData Data => ref _data;

        //public override string ToString()
        //{
        //    return $"PlayerComponent [HP: {Data.Stats.Health} DataSize: {DataSize}]";
        //}
    }

    public class EditableAttribute : Attribute
    {

    }


    // ECS Data (pure SoA)
    public struct NPCData : IComponentData
    {
        public float Health;
        public float Awareness;
        public float AwarenessGainRate;
        public float HealthRegenRate;
    }


    // Hybrid Component (Standard + ECS)
    public class NPCComponent : ComponentDataSet<NPCData>, // ComponentSet<D1, D2, ...>
                                  IComponentUpdate<NPCComponent>, // standard update (optional)
                                  IComponentDataUpdate<NPCData> // ECS update (optional)
    {
        [Editable] private float _decisionTimer;
        [Editable] private bool _alerted;

        // CRTP-style static dispatch (no virtual calls)

        // Standard Update (instance-based)
        public static void OnUpdate(NPCComponent npc, float dt)
        {
            npc._decisionTimer += dt;

            // ECS data is safely mutable in this edit scope
            var npcData = npc.D1.Data;
            npc._alerted = npcData.Awareness > 0.5f;
            npcData.AwarenessGainRate = npc._alerted ? 0.3f : 0.1f;
            npcData.HealthRegenRate = npc._alerted ? 0.0f : 0.05f;
        }

        // ECS Update (pure SoA)
        public static void OnDataUpdate(ComponentData<NPCData> cd, float dt)
        {
            using (cd.BeginEdit())
            {
                var data = cd.Data;
                data.Awareness += data.AwarenessGainRate * dt;
                data.Health += data.HealthRegenRate * dt;
            }// commit changes to ECS storage
        }
    }
}
