// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Reflection;
using System.Text.Json;
using System.Text.Json.Nodes;

namespace xg
{
    // Keeps a script module's actors and component values across a hot reload. Capture runs in the old scripts' copy of
    // this kit before they unload; Restore runs in the new copy after its Init. Values are the fields the Inspector
    // shows (public or [SerializeField], not [HideInInspector], not static or const): primitives, enums, strings and
    // structs of those (Vector2/3 included). Components whose type is gone, and fields that were renamed or removed or
    // changed type, are skipped and reported.
    public static class ReloadState
    {
        // JSON: { "actors": [ { "id": 12, "adopted": false, "components": [ { "type": "Full.Name, Assembly", "fields": {...} } ] } ] }
        public static string Capture()
        {
            JsonArray actors = new();
            foreach (Actor actor in ActorRegistry.Actors.ToArray())
            {
                JsonArray components = new();
                foreach (object component in actor.Components)
                {
                    Type type = component.GetType();
                    components.Add(new JsonObject
                    {
                        ["type"] = $"{type.FullName}, {type.Assembly.GetName().Name}",
                        ["fields"] = WriteFields(component, type),
                    });
                }
                actors.Add(new JsonObject
                {
                    ["id"] = (ulong)actor.ID,
                    ["adopted"] = actor.IsAdopted,
                    ["components"] = components,
                });
            }
            return new JsonObject { ["actors"] = actors }.ToJsonString();
        }

        // Re-creates the captured actors' components in this module and sets their values. Returns a summary.
        public static string Restore(string json)
        {
            JsonNode? root = JsonNode.Parse(json);
            int actorCount = 0;
            int componentCount = 0;
            List<string> skipped = new();
            foreach (JsonNode? node in root?["actors"]?.AsArray() ?? new JsonArray())
            {
                if (node is null)
                    continue;
                ActorID id = new(node["id"]!.GetValue<ulong>());
                if (ActorTree.GetName(id).Length == 0)
                    continue;
                Actor actor = ActorRegistry.TryGet(id, out Actor? existing) ? existing : Actor.Reattach(id, node["adopted"]?.GetValue<bool>() ?? false);
                actorCount++;
                foreach (JsonNode? entry in node["components"]?.AsArray() ?? new JsonArray())
                {
                    string key = entry?["type"]?.GetValue<string>() ?? string.Empty;
                    Type? type = ActorRegistry.FindOwnType(key, false);
                    if (type is null)
                    {
                        skipped.Add($"{key} (type not found)");
                        continue;
                    }
                    try
                    {
                        object component = actor.AddComponent(type);
                        ReadFields(component, type, entry?["fields"] as JsonObject, skipped);
                        componentCount++;
                    }
                    catch (Exception e)
                    {
                        skipped.Add($"{key} ({(e as TargetInvocationException)?.InnerException?.Message ?? e.Message})");
                    }
                }
            }
            foreach (string item in skipped)
                Debug.LogWarning($"[Scripts] Not restored: {item}");
            return $"{actorCount} actor(s), {componentCount} component(s)";
        }

        // Puts this module's live actors back to a capture taken earlier in the same load context (play mode in the
        // editor): matches each actor's components by type and order, writes the saved values, adds missing components
        // and removes the ones added since. Actors that no longer exist are skipped. Returns a summary.
        public static string Apply(string json)
        {
            int actorCount = 0;
            int componentCount = 0;
            int removed = 0;
            List<string> skipped = new();
            foreach (JsonNode? node in JsonNode.Parse(json)?["actors"]?.AsArray() ?? new JsonArray())
            {
                if (node?["id"] is not JsonNode idNode || !ActorRegistry.TryGet(new ActorID(idNode.GetValue<ulong>()), out Actor? actor))
                    continue;
                actorCount++;
                List<object> live = actor.Components.ToList();
                HashSet<object> matched = new(ReferenceEqualityComparer.Instance);
                foreach (JsonNode? entry in node["components"]?.AsArray() ?? new JsonArray())
                {
                    string key = entry?["type"]?.GetValue<string>() ?? string.Empty;
                    Type? type = ActorRegistry.FindOwnType(key, false);
                    if (type is null)
                    {
                        skipped.Add($"{key} (type not found)");
                        continue;
                    }
                    object? component = live.FirstOrDefault(c => !matched.Contains(c) && c.GetType() == type);
                    try
                    {
                        component ??= actor.AddComponent(type);
                        matched.Add(component);
                        ReadFields(component, type, entry?["fields"] as JsonObject, skipped);
                        componentCount++;
                    }
                    catch (Exception e)
                    {
                        skipped.Add($"{key} ({(e as TargetInvocationException)?.InnerException?.Message ?? e.Message})");
                    }
                }
                foreach (object component in live)
                    if (!matched.Contains(component) && actor.RemoveComponent(component))
                        removed++;
            }
            foreach (string item in skipped)
                Debug.LogWarning($"[Scripts] Not restored: {item}");
            return $"{actorCount} actor(s), {componentCount} component(s), {removed} removed";
        }

        // Destroys this module's own actors that a capture doesn't list (spawned after it was taken). Returns how many.
        public static int DestroyActorsExcept(string json)
        {
            HashSet<ulong> kept = new();
            foreach (JsonNode? node in JsonNode.Parse(json)?["actors"]?.AsArray() ?? new JsonArray())
                if (node?["id"] is JsonNode id)
                    kept.Add(id.GetValue<ulong>());
            int destroyed = 0;
            foreach (Actor actor in ActorRegistry.Actors.ToArray())
            {
                if (actor.IsAdopted || kept.Contains((ulong)actor.ID))
                    continue;
                actor.Destroy();
                destroyed++;
            }
            return destroyed;
        }

        public static IEnumerable<FieldInfo> VisibleFields(Type type)
        {
            const BindingFlags flags = BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic;
            foreach (FieldInfo field in type.GetFields(flags))
            {
                if (field.IsLiteral || field.IsDefined(typeof(HideInInspectorAttribute), true))
                    continue;
                if (field.IsPublic || field.IsDefined(typeof(SerializeFieldAttribute), true))
                    yield return field;
            }
        }

        private static JsonObject WriteFields(object target, Type type)
        {
            JsonObject fields = new();
            foreach (FieldInfo field in VisibleFields(type))
            {
                JsonNode? value = WriteValue(field.GetValue(target), field.FieldType, 0);
                if (value is not null)
                    fields[field.Name] = new JsonObject { ["type"] = field.FieldType.FullName, ["value"] = value };
            }
            return fields;
        }

        private static JsonNode? WriteValue(object? value, Type type, int depth)
        {
            if (value is null)
                return null;
            if (type.IsEnum)
                return JsonValue.Create(value.ToString());
            switch (Type.GetTypeCode(type))
            {
                case TypeCode.Boolean: return JsonValue.Create((bool)value);
                case TypeCode.String: return JsonValue.Create((string)value);
                case TypeCode.Char: return JsonValue.Create(value.ToString());
                case TypeCode.SByte: case TypeCode.Int16: case TypeCode.Int32: case TypeCode.Int64:
                    return JsonValue.Create(Convert.ToInt64(value));
                case TypeCode.Byte: case TypeCode.UInt16: case TypeCode.UInt32: case TypeCode.UInt64:
                    return JsonValue.Create(Convert.ToUInt64(value));
                case TypeCode.Single: return JsonValue.Create((float)value);
                case TypeCode.Double: return JsonValue.Create((double)value);
                case TypeCode.Decimal: return JsonValue.Create((decimal)value);
            }
            if (type.IsValueType && !type.IsPrimitive && depth < 8)
            {
                JsonObject fields = new();
                foreach (FieldInfo field in type.GetFields(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic))
                {
                    JsonNode? inner = WriteValue(field.GetValue(value), field.FieldType, depth + 1);
                    if (inner is not null)
                        fields[field.Name] = inner;
                }
                return fields;
            }
            return null;
        }

        private static void ReadFields(object target, Type type, JsonObject? saved, List<string> skipped)
        {
            if (saved is null)
                return;
            Dictionary<string, FieldInfo> fields = VisibleFields(type).ToDictionary(f => f.Name);
            foreach ((string name, JsonNode? entry) in saved)
            {
                if (!fields.TryGetValue(name, out FieldInfo? field))
                {
                    skipped.Add($"{type.Name}.{name} (field removed or renamed)");
                    continue;
                }
                if (entry?["type"]?.GetValue<string>() != field.FieldType.FullName)
                {
                    skipped.Add($"{type.Name}.{name} (type changed)");
                    continue;
                }
                try
                {
                    field.SetValue(target, ReadValue(entry?["value"], field.FieldType, field.GetValue(target), 0));
                }
                catch (Exception e)
                {
                    skipped.Add($"{type.Name}.{name} ({e.Message})");
                }
            }
        }

        private static object? ReadValue(JsonNode? node, Type type, object? current, int depth)
        {
            if (node is null)
                return current;
            if (type.IsEnum)
                return Enum.TryParse(type, node.GetValue<string>(), out object? parsed) ? parsed : current;
            switch (Type.GetTypeCode(type))
            {
                case TypeCode.Boolean: return node.GetValue<bool>();
                case TypeCode.String: return node.GetValue<string>();
                case TypeCode.Char: string text = node.GetValue<string>(); return text.Length > 0 ? text[0] : current;
                case TypeCode.SByte: case TypeCode.Int16: case TypeCode.Int32: case TypeCode.Int64:
                    return Convert.ChangeType(node.GetValue<long>(), type);
                case TypeCode.Byte: case TypeCode.UInt16: case TypeCode.UInt32: case TypeCode.UInt64:
                    return Convert.ChangeType(node.GetValue<ulong>(), type);
                case TypeCode.Single: return node.GetValue<float>();
                case TypeCode.Double: return node.GetValue<double>();
                case TypeCode.Decimal: return node.GetValue<decimal>();
            }
            if (type.IsValueType && node is JsonObject saved && depth < 8)
            {
                object boxed = current ?? Activator.CreateInstance(type)!;
                foreach (FieldInfo field in type.GetFields(BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic))
                    if (saved.TryGetPropertyValue(field.Name, out JsonNode? inner))
                        field.SetValue(boxed, ReadValue(inner, field.FieldType, field.GetValue(boxed), depth + 1));
                return boxed;
            }
            return current;
        }
    }
}
