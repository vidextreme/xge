// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text.Json;
using System.Text.Json.Nodes;
using xg;

namespace xgEditor.CoreCLR
{
    // Adds the authored components of spawned Composition Actors: each frame it takes the actors the object drive system
    // spawned and adds every { "type", "fields" } entry through the component factories (the module that defines the
    // type adopts the actor), then writes the authored field values. A player build will need the same pass.
    public static class CompositionComponents
    {
        private static readonly List<object> _components = new();

        public static int Added { get; private set; }

        // Raised after a spawned Composition Actor got its authored components (scenes apply instance overrides here).
        public static event Action<ActorID>? Spawned;

        public static void Update()
        {
            for (int guard = 0; guard < 64 && ObjectDriverComponent.TryTakeSpawned(out ActorID actor, out string json); guard++)
                Apply(actor, json);
        }

        public static void Apply(ActorID actor, string json)
        {
            JsonArray? entries;
            try
            {
                entries = JsonNode.Parse(json) as JsonArray;
            }
            catch (JsonException e)
            {
                Debug.LogWarning($"[Objects] {ActorTree.GetName(actor)}: unreadable authored components ({e.Message})");
                return;
            }
            int added = 0;
            foreach (JsonNode? entry in entries ?? new JsonArray())
            {
                string? key = entry?["type"]?.GetValue<string>();
                if (string.IsNullOrEmpty(key))
                    continue;
                if (!ActorTree.AddComponent(actor, key))
                {
                    Debug.LogWarning($"[Objects] {ActorTree.GetName(actor)}: no module could add {key}");
                    continue;
                }
                added++;
                string fullName = key.Split(',')[0].Trim();
                _components.Clear();
                ActorTree.GetComponents(actor, _components);
                object? component = _components.LastOrDefault(c => c.GetType().FullName == fullName);
                if (component is null)
                    continue;
                if (entry?["id"]?.GetValue<string>() is { Length: > 0 } id)
                    ComponentIds.Set(component, id);
                foreach (string skipped in ReloadState.ApplyFields(component, entry?["fields"] as JsonObject))
                    Debug.LogWarning($"[Objects] {ActorTree.GetName(actor)}: not applied: {skipped}");
            }
            Added += added;
            if (added > 0)
                Debug.Log($"[Objects] Added {added} authored component(s) to {ActorTree.GetName(actor)}");
            try
            {
                Spawned?.Invoke(actor);
            }
            catch (Exception e)
            {
                Debug.LogException(e);
            }
        }
    }
}
