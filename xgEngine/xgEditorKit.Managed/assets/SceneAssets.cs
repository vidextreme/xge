// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text.Json;
using System.Text.Json.Nodes;

namespace xg.editor
{
    // One component of a saved actor: a file-local instance id (16 hex digits, unique within its actor), the type key
    // ("Namespace.Class, Assembly"; the .cs file it lives in doesn't matter), the Inspector-visible field values in the
    // ReloadState capture form, and one entry per data element for data components.
    public sealed class SceneComponentAsset
    {
        public string Id { get; set; } = string.Empty;
        public string Type { get; set; } = string.Empty;
        public JsonObject Fields { get; set; } = new();
        public List<JsonObject> Data { get; } = new();

        public SceneComponentAsset Clone()
        {
            SceneComponentAsset copy = new() { Id = Id, Type = Type, Fields = Fields.DeepClone().AsObject() };
            foreach (JsonObject data in Data)
                copy.Data.Add(data.DeepClone().AsObject());
            return copy;
        }
    }

    // Per-instance overrides of one Composition Actor of a driver: the .xco node GUID (the name helps repair) and the
    // components that differ from the authored defaults (matched by component id; new ids are additions).
    public sealed class SceneCompositionAsset
    {
        public string Node { get; set; } = string.Empty;
        public string Name { get; set; } = string.Empty;
        // Editor-only: hidden in the Scene view.
        public bool Hidden { get; set; }
        public List<SceneComponentAsset> Components { get; } = new();

        public SceneCompositionAsset Clone()
        {
            SceneCompositionAsset copy = new() { Node = Node, Name = Name, Hidden = Hidden };
            foreach (SceneComponentAsset component in Components)
                copy.Components.Add(component.Clone());
            return copy;
        }
    }

    // A Hierarchy folder (editor-only, no actor): a scene-local id (32 hex digits), a name, the parent folder id (empty =
    // the scene root) and its editor flags. Folders hold root actors (SceneActorAsset.Folder) and other folders.
    public sealed class SceneFolderAsset
    {
        public string Id { get; set; } = string.Empty;
        public string Name { get; set; } = string.Empty;
        public string Parent { get; set; } = string.Empty;
        public bool Hidden { get; set; }
        public bool Locked { get; set; }

        public SceneFolderAsset Clone() => new() { Id = Id, Name = Name, Parent = Parent, Hidden = Hidden, Locked = Locked };
    }

    // One saved actor: a scene-local id (32 hex digits, stable across saves; runtime ActorIDs are fresh on every load),
    // a name, a parent id, the folder of a root actor, the editor flags (hidden in the Scene view, locked), an optional
    // transform, components and composition overrides. Unknown keys are kept on rewrite.
    public sealed class SceneActorAsset
    {
        public string Id { get; set; } = string.Empty;
        public string Name { get; set; } = string.Empty;
        public string Parent { get; set; } = string.Empty;
        public string Folder { get; set; } = string.Empty;
        public bool Hidden { get; set; }
        public bool Locked { get; set; }
        public JsonObject Extra { get; set; } = new();
        public bool HasTransform { get; set; }
        public float[] Position { get; set; } = { 0.0f, 0.0f, 0.0f };
        public float[] Rotation { get; set; } = { 0.0f, 0.0f, 0.0f, 1.0f };
        public float[] Scale { get; set; } = { 1.0f, 1.0f, 1.0f };
        public List<SceneComponentAsset> Components { get; } = new();
        public List<SceneCompositionAsset> Compositions { get; } = new();

        public SceneActorAsset Clone()
        {
            SceneActorAsset copy = new()
            {
                Id = Id, Name = Name, Parent = Parent, Folder = Folder, Hidden = Hidden, Locked = Locked, Extra = Extra.DeepClone().AsObject(),
                HasTransform = HasTransform,
                Position = (float[])Position.Clone(), Rotation = (float[])Rotation.Clone(), Scale = (float[])Scale.Clone(),
            };
            foreach (SceneComponentAsset component in Components)
                copy.Components.Add(component.Clone());
            foreach (SceneCompositionAsset composition in Compositions)
                copy.Compositions.Add(composition.Clone());
            return copy;
        }
    }

    // An authored .xcs scene. Meta is the embedded asset meta (AssetMetaStore: guid, tags, names), kept as is; content
    // comparisons ignore it. Hidden/Locked are the scene's own editor flags (the top-level "editor" block) and Folders the
    // Hierarchy folders. Unknown top-level keys are kept on rewrite (room for scene settings later). Each actor
    // belongs to exactly one scene and ids never point across scenes, so several scenes can be loaded side by side.
    public sealed class SceneAsset
    {
        public const int CurrentVersion = 1;
        public const string Extension = ".xcs";

        private static readonly HashSet<string> _known = new(StringComparer.Ordinal) { "version", AssetMetaStore.EmbeddedKey, "editor", "folders", "actors" };
        private static readonly HashSet<string> _knownActor = new(StringComparer.Ordinal) { "id", "name", "parent", "folder", "editor", "transform", "components", "compositions" };

        public int Version { get; set; } = CurrentVersion;
        public JsonObject? Meta { get; set; }
        public bool Hidden { get; set; }
        public bool Locked { get; set; }
        public List<SceneFolderAsset> Folders { get; } = new();
        public List<SceneActorAsset> Actors { get; } = new();
        public JsonObject Extra { get; set; } = new();

        public static bool IsScene(string path) => Path.GetExtension(path).Equals(Extension, StringComparison.OrdinalIgnoreCase);

        public static string NewActorId() => Guid.NewGuid().ToString("N");

        public static string NewComponentId() => Guid.NewGuid().ToString("N")[..16];

        public SceneActorAsset? Find(string id) => Actors.FirstOrDefault(a => string.Equals(a.Id, id, StringComparison.OrdinalIgnoreCase));

        public SceneAsset Clone()
        {
            SceneAsset copy = new() { Version = Version, Meta = Meta?.DeepClone().AsObject(), Extra = Extra.DeepClone().AsObject(), Hidden = Hidden, Locked = Locked };
            foreach (SceneFolderAsset folder in Folders)
                copy.Folders.Add(folder.Clone());
            foreach (SceneActorAsset actor in Actors)
                copy.Actors.Add(actor.Clone());
            return copy;
        }

        public bool SameAs(SceneAsset other) => ContentJSON() == other.ContentJSON();

        // The scene's JSON without the meta block.
        public string ContentJSON() => Document(false).ToJsonString(JSONFile.Options) + "\r\n";

        public string ToJSON() => Document(true).ToJsonString(JSONFile.Options) + "\r\n";

        public bool Write(string path) => JSONFile.WriteText(path, ToJSON());

        public static SceneAsset Read(string path) => Parse(File.ReadAllText(path));

        public static SceneAsset Parse(string json)
        {
            JsonNode? root = JsonNode.Parse(json.TrimStart('﻿'), documentOptions: new JsonDocumentOptions { CommentHandling = JsonCommentHandling.Skip, AllowTrailingCommas = true });
            if (root is not JsonObject document)
                throw new InvalidDataException("A scene is a JSON object");
            SceneAsset scene = new()
            {
                Version = document["version"]?.GetValue<int>() ?? CurrentVersion,
                Meta = document[AssetMetaStore.EmbeddedKey]?.DeepClone() as JsonObject,
            };
            if (scene.Version > CurrentVersion)
                throw new InvalidDataException($"Scene version {scene.Version} is newer than {CurrentVersion}");
            foreach ((string key, JsonNode? value) in document)
                if (!_known.Contains(key))
                    scene.Extra[key] = value?.DeepClone();
            (scene.Hidden, scene.Locked) = EditorFlags(document["editor"]);
            if (document["folders"] is JsonArray folders)
                foreach (JsonNode? item in folders)
                    if (item is JsonObject folder)
                    {
                        (bool hidden, bool locked) = EditorFlags(folder["editor"]);
                        scene.Folders.Add(new SceneFolderAsset { Id = Text(folder["id"]), Name = Text(folder["name"]), Parent = Text(folder["parent"]), Hidden = hidden, Locked = locked });
                    }
            if (document["actors"] is JsonArray actors)
                foreach (JsonNode? item in actors)
                    if (item is JsonObject actor)
                        scene.Actors.Add(ParseActor(actor));
            return scene;
        }

        private static SceneActorAsset ParseActor(JsonObject json)
        {
            SceneActorAsset actor = new() { Id = Text(json["id"]), Name = Text(json["name"]), Parent = Text(json["parent"]), Folder = Text(json["folder"]) };
            (actor.Hidden, actor.Locked) = EditorFlags(json["editor"]);
            foreach ((string key, JsonNode? value) in json)
                if (!_knownActor.Contains(key))
                    actor.Extra[key] = value?.DeepClone();
            if (json["transform"] is JsonObject transform)
            {
                actor.HasTransform = true;
                actor.Position = Floats(transform["position"], actor.Position);
                actor.Rotation = Floats(transform["rotation"], actor.Rotation);
                actor.Scale = Floats(transform["scale"], actor.Scale);
            }
            if (json["components"] is JsonArray components)
                foreach (JsonNode? item in components)
                    if (item is JsonObject component)
                        actor.Components.Add(ParseComponent(component));
            if (json["compositions"] is JsonArray compositions)
            {
                foreach (JsonNode? item in compositions)
                {
                    if (item is not JsonObject composition)
                        continue;
                    SceneCompositionAsset entry = new() { Node = Text(composition["node"]), Name = Text(composition["name"]), Hidden = EditorFlags(composition["editor"]).Hidden };
                    if (composition["components"] is JsonArray list)
                        foreach (JsonNode? component in list)
                            if (component is JsonObject value)
                                entry.Components.Add(ParseComponent(value));
                    actor.Compositions.Add(entry);
                }
            }
            return actor;
        }

        public static SceneComponentAsset ParseComponent(JsonObject json)
        {
            SceneComponentAsset component = new()
            {
                Id = Text(json["id"]),
                Type = Text(json["type"]),
                Fields = json["fields"]?.DeepClone() as JsonObject ?? new JsonObject(),
            };
            if (json["data"] is JsonArray data)
                foreach (JsonNode? item in data)
                    component.Data.Add(item?.DeepClone() as JsonObject ?? new JsonObject());
            return component;
        }

        public static JsonObject ComponentJSON(SceneComponentAsset component)
        {
            JsonObject json = new() { ["id"] = component.Id, ["type"] = component.Type, ["fields"] = component.Fields.DeepClone() };
            if (component.Data.Count > 0)
                json["data"] = new JsonArray(component.Data.Select(d => (JsonNode)d.DeepClone()).ToArray());
            return json;
        }

        private static string Text(JsonNode? node) =>
            node is JsonValue value && value.TryGetValue(out string? text) ? text : string.Empty;

        private static bool Flag(JsonNode? node) => node is JsonValue value && value.TryGetValue(out bool on) && on;

        private static (bool Hidden, bool Locked) EditorFlags(JsonNode? node) =>
            node is JsonObject json ? (Flag(json["hidden"]), Flag(json["locked"])) : (false, false);

        // The "editor" block of a node, or null when no flag is set.
        private static JsonObject? EditorJSON(bool hidden, bool locked)
        {
            if (!hidden && !locked)
                return null;
            JsonObject json = new();
            if (hidden)
                json["hidden"] = true;
            if (locked)
                json["locked"] = true;
            return json;
        }

        private static float[] Floats(JsonNode? node, float[] fallback)
        {
            float[] result = (float[])fallback.Clone();
            if (node is JsonArray array)
                for (int i = 0; i < Math.Min(array.Count, result.Length); i++)
                    result[i] = array[i]?.GetValue<float>() ?? result[i];
            return result;
        }

        private static JsonArray Floats(float[] values) =>
            new(values.Select(v => (JsonNode)JsonValue.Create(MathF.Round(v, 6))!).ToArray());

        private JsonObject Document(bool withMeta)
        {
            JsonArray actors = new();
            foreach (SceneActorAsset actor in Actors)
            {
                JsonObject json = new()
                {
                    ["id"] = actor.Id,
                    ["name"] = actor.Name,
                    ["parent"] = actor.Parent.Length > 0 ? actor.Parent : null,
                };
                if (actor.Folder.Length > 0)
                    json["folder"] = actor.Folder;
                if (EditorJSON(actor.Hidden, actor.Locked) is JsonObject flags)
                    json["editor"] = flags;
                if (actor.HasTransform)
                    json["transform"] = new JsonObject { ["position"] = Floats(actor.Position), ["rotation"] = Floats(actor.Rotation), ["scale"] = Floats(actor.Scale) };
                json["components"] = new JsonArray(actor.Components.Select(c => (JsonNode)ComponentJSON(c)).ToArray());
                if (actor.Compositions.Count > 0)
                {
                    JsonArray compositions = new();
                    foreach (SceneCompositionAsset composition in actor.Compositions)
                    {
                        JsonObject entry = new() { ["node"] = composition.Node, ["name"] = composition.Name };
                        if (EditorJSON(composition.Hidden, false) is JsonObject compositionFlags)
                            entry["editor"] = compositionFlags;
                        entry["components"] = new JsonArray(composition.Components.Select(c => (JsonNode)ComponentJSON(c)).ToArray());
                        compositions.Add(entry);
                    }
                    json["compositions"] = compositions;
                }
                foreach ((string key, JsonNode? value) in actor.Extra)
                    json[key] = value?.DeepClone();
                actors.Add(json);
            }
            JsonObject document = new() { ["version"] = CurrentVersion };
            if (withMeta && Meta is not null)
                document[AssetMetaStore.EmbeddedKey] = Meta.DeepClone();
            if (EditorJSON(Hidden, Locked) is JsonObject sceneFlags)
                document["editor"] = sceneFlags;
            if (Folders.Count > 0)
            {
                JsonArray folders = new();
                foreach (SceneFolderAsset folder in Folders)
                {
                    JsonObject entry = new() { ["id"] = folder.Id, ["name"] = folder.Name, ["parent"] = folder.Parent.Length > 0 ? folder.Parent : null };
                    if (EditorJSON(folder.Hidden, folder.Locked) is JsonObject folderFlags)
                        entry["editor"] = folderFlags;
                    folders.Add(entry);
                }
                document["folders"] = folders;
            }
            document["actors"] = actors;
            foreach ((string key, JsonNode? value) in Extra)
                document[key] = value?.DeepClone();
            return document;
        }

        // The actors parents first, or the reasons the scene can't load as is (bad or duplicate ids, missing parents,
        // cycles, duplicate component ids on one actor).
        public List<SceneActorAsset> Ordered(List<string> errors)
        {
            Dictionary<string, SceneActorAsset> byId = new(StringComparer.OrdinalIgnoreCase);
            foreach (SceneActorAsset actor in Actors)
            {
                if (!AssetMeta.IsValidGuid(actor.Id))
                    errors.Add($"Actor \"{actor.Name}\" has no valid id");
                else if (!byId.TryAdd(actor.Id, actor))
                    errors.Add($"Actor id {actor.Id} is used twice");
                foreach (IGrouping<string, SceneComponentAsset> group in actor.Components.GroupBy(c => c.Id, StringComparer.OrdinalIgnoreCase))
                    if (group.Key.Length == 0 || group.Count() > 1)
                        errors.Add(group.Key.Length == 0 ? $"A component of \"{actor.Name}\" has no id" : $"Component id {group.Key} is used twice on \"{actor.Name}\"");
            }
            foreach (SceneActorAsset actor in Actors)
            {
                if (actor.Parent.Length > 0 && !byId.ContainsKey(actor.Parent))
                    errors.Add($"Actor \"{actor.Name}\" names a missing parent");
                else if (actor.Parent.Length > 0 && Reaches(actor, byId))
                    errors.Add($"Actor \"{actor.Name}\" is its own ancestor");
            }
            List<SceneActorAsset> ordered = new();
            if (errors.Count > 0)
                return ordered;
            HashSet<string> placed = new(StringComparer.OrdinalIgnoreCase);
            bool progress = true;
            while (progress && ordered.Count < Actors.Count)
            {
                progress = false;
                foreach (SceneActorAsset actor in Actors)
                {
                    if (placed.Contains(actor.Id) || (actor.Parent.Length > 0 && !placed.Contains(actor.Parent)))
                        continue;
                    ordered.Add(actor);
                    placed.Add(actor.Id);
                    progress = true;
                }
            }
            return ordered;
        }

        private static bool Reaches(SceneActorAsset start, Dictionary<string, SceneActorAsset> byId)
        {
            HashSet<string> seen = new(StringComparer.OrdinalIgnoreCase);
            string parent = start.Parent;
            while (parent.Length > 0 && byId.TryGetValue(parent, out SceneActorAsset? node))
            {
                if (string.Equals(parent, start.Id, StringComparison.OrdinalIgnoreCase) || !seen.Add(parent))
                    return true;
                parent = node.Parent;
            }
            return false;
        }

        // Asset GUIDs referenced by component values (AssetReference / ResourceGUID values, captured as { Hi, Lo }),
        // for the names tree in the meta.
        public IEnumerable<string> ReferencedGuids()
        {
            HashSet<string> found = new(StringComparer.Ordinal);
            void Walk(JsonNode? node)
            {
                if (node is JsonObject json)
                {
                    if (json.Count == 2 && json["Hi"] is JsonValue hi && json["Lo"] is JsonValue lo
                        && hi.TryGetValue(out ulong h) && lo.TryGetValue(out ulong l) && (h != 0 || l != 0))
                        found.Add($"{h:x16}{l:x16}");
                    foreach ((_, JsonNode? child) in json)
                        Walk(child);
                }
                else if (node is JsonArray array)
                {
                    foreach (JsonNode? child in array)
                        Walk(child);
                }
            }
            foreach (SceneActorAsset actor in Actors)
                foreach (SceneComponentAsset component in actor.Components.Concat(actor.Compositions.SelectMany(c => c.Components)))
                {
                    Walk(component.Fields);
                    foreach (JsonObject data in component.Data)
                        Walk(data);
                }
            return found;
        }
    }
}
