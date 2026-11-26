// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.CompilerServices;
using xg.editor;

namespace xgEditor.CoreCLR
{
    // The file-local instance ids of live components (16 hex digits, from .xcs and .xco entries), kept beside the
    // component objects so nothing changes at runtime. Components recreated by hot reload or play mode lose their
    // entry; Bind gives them back the id of the same type at the same position from the last known order.
    public static class ComponentIds
    {
        private static readonly ConditionalWeakTable<object, string> _ids = new();

        public static string? Get(object component) => _ids.TryGetValue(component, out string? id) ? id : null;

        public static void Set(object component, string id) => _ids.AddOrUpdate(component, id);

        // Ids for an actor's components (in order, Transform excluded): known ids first, then the unused ids of `known`
        // (type, id) pairs matched by type in order, then new ones.
        public static List<string> Bind(IReadOnlyList<object> components, IReadOnlyList<(string Type, string Id)>? known)
        {
            List<string> result = new();
            HashSet<string> used = new(StringComparer.OrdinalIgnoreCase);
            foreach (object component in components)
                if (Get(component) is string id)
                    used.Add(id);
            foreach (object component in components)
            {
                string? id = Get(component);
                if (id is null || result.Contains(id, StringComparer.OrdinalIgnoreCase))
                {
                    string type = TypeBridge.KeyOf(component.GetType());
                    id = known?.FirstOrDefault(k => k.Type == type && !used.Contains(k.Id)).Id;
                    if (string.IsNullOrEmpty(id))
                        id = SceneAsset.NewComponentId();
                    used.Add(id);
                    Set(component, id);
                }
                result.Add(id);
            }
            return result;
        }
    }
}
