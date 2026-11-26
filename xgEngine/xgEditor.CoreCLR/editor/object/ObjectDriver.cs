// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text.Json;
using System.Text.Json.Nodes;
using xg;
using xg.editor;

namespace xgEditor.CoreCLR
{
    // The data of one .xco composition object being edited: the saved asset and the draft. Edits change the draft (and
    // bump DraftVersion); Apply writes the .xco with its name tree and queues a build; Revert drops the draft. No UI, so
    // batch tools can drive it too.
    public sealed class ObjectDriver : IToolDriver
    {

        public ObjectDriver(string path)
        {
            Path = path;
            Load();
        }

        public string Path { get; private set; }
        public string? Error { get; private set; }
        public ObjectAsset Saved { get; private set; } = new();
        public ObjectAsset Draft { get; private set; } = new();
        public int DraftVersion { get; private set; }

        public string Name => System.IO.Path.GetFileNameWithoutExtension(Path);
        public string? Guid => ProjectService.Assets?.GuidOf(Path);
        public bool HasPendingChanges => !Draft.SameAs(Saved);

        public void Load()
        {
            Error = null;
            if (ProjectService.Assets is not { } assets)
            {
                Error = "No project is open.";
                return;
            }
            try
            {
                Saved = ObjectAsset.Read(assets.FullPath(Path));
                Draft = Saved.Clone();
                RepairReferences(assets);
            }
            catch (Exception e) when (e is IOException or JsonException or InvalidDataException or UnauthorizedAccessException)
            {
                Error = $"This object can't be read: {e.Message}";
                Saved = new ObjectAsset();
                Draft = new ObjectAsset();
            }
            Changed();
        }

        public void Changed() => DraftVersion++;

        public ObjectNodeAsset? Find(string? id) => string.IsNullOrEmpty(id) ? null : Draft.Find(id);

        // Children of a node in draft order ("" = the roots).
        public IEnumerable<ObjectNodeAsset> Children(string parent) => parent.Length == 0
            ? Draft.Objects.Where(o => o.Parent.Length == 0 || Draft.Find(o.Parent) is null)
            : Draft.Objects.Where(o => string.Equals(o.Parent, parent, StringComparison.OrdinalIgnoreCase));

        // A new Object under a parent ("" = root), named uniquely among its kind ("Mesh", "Mesh 1", ...).
        public ObjectNodeAsset Add(ObjectNodeKind kind, string parent)
        {
            string stem = kind == ObjectNodeKind.Mesh ? "Mesh" : "Proxy";
            string name = stem;
            for (int i = 1; Draft.Objects.Any(o => o.Name == name); i++)
                name = $"{stem} {i}";
            ObjectNodeAsset node = new() { Id = AssetMeta.NewGuid(), Name = name, Kind = kind, Parent = parent };
            if (kind == ObjectNodeKind.Mesh && Find(parent) is { Kind: ObjectNodeKind.Mesh } parentMesh)
                node.Model = parentMesh.Model;
            Draft.Objects.Add(node);
            Changed();
            return node;
        }

        // Removes a node and its whole subtree.
        public void Remove(string id)
        {
            HashSet<string> doomed = new(StringComparer.OrdinalIgnoreCase) { id };
            bool grew = true;
            while (grew)
            {
                grew = false;
                foreach (ObjectNodeAsset node in Draft.Objects)
                    if (node.Parent.Length > 0 && doomed.Contains(node.Parent) && doomed.Add(node.Id))
                        grew = true;
            }
            Draft.Objects.RemoveAll(o => doomed.Contains(o.Id));
            Changed();
        }

        public bool IsDescendant(string id, string ancestor)
        {
            string parent = Find(id)?.Parent ?? string.Empty;
            for (int guard = 0; parent.Length > 0 && guard < 4096; guard++)
            {
                if (string.Equals(parent, ancestor, StringComparison.OrdinalIgnoreCase))
                    return true;
                parent = Find(parent)?.Parent ?? string.Empty;
            }
            return false;
        }

        // Moves a node under another parent ("" = root), keeping its world pose.
        public bool SetParent(string id, string parent)
        {
            if (Find(id) is not ObjectNodeAsset node || string.Equals(node.Parent, parent, StringComparison.OrdinalIgnoreCase)
                || string.Equals(id, parent, StringComparison.OrdinalIgnoreCase) || (parent.Length > 0 && IsDescendant(parent, id)))
                return false;
            float[] world = WorldOf(node);
            float[] parentWorld = parent.Length > 0 && Find(parent) is ObjectNodeAsset p ? WorldOf(p) : ObjectMath.Identity();
            float[] local = ObjectAssets.Multiply(world, ObjectAssets.Invert(parentWorld));
            TransformData pose = ObjectDriverComponent.Decompose(local);
            node.Parent = parent;
            node.Position = new[] { pose.PositionX, pose.PositionY, pose.PositionZ };
            node.Rotation = new[] { pose.RotationX, pose.RotationY, pose.RotationZ, pose.RotationW };
            node.Scale = new[] { pose.ScaleX, pose.ScaleY, pose.ScaleZ };
            Changed();
            return true;
        }

        // Local-to-root matrix of a node in the draft (the chain of its Jointers).
        public float[] WorldOf(ObjectNodeAsset node) => ObjectDrawList.WorldOf(Draft, node);

        // The submesh items of a model asset (from its meta, cached by the meta's write time).
        public IReadOnlyList<AssetItem> SubmeshesOf(string modelGuid) => ModelSubmeshes.Of(modelGuid);

        // Points a Mesh Object at a model, keeping the parts whose submesh the model still has; a new model starts with
        // all of its submeshes.
        public void SetModel(ObjectNodeAsset node, string modelGuid)
        {
            if (node.Model == modelGuid)
                return;
            node.Model = modelGuid;
            node.Parts.Clear();
            foreach (AssetItem item in SubmeshesOf(modelGuid))
                node.Parts.Add(new ObjectPartAsset { Submesh = item.Guid });
            Changed();
        }

        // Ticks or unticks a submesh; an unticked part stays in the list (hidden) so it keeps its material.
        public void SetPartIncluded(ObjectNodeAsset node, string submesh, bool included)
        {
            ObjectPartAsset? part = node.Parts.FirstOrDefault(p => p.Submesh == submesh);
            if (part is null)
            {
                if (!included)
                    return;
                node.Parts.Add(new ObjectPartAsset { Submesh = submesh });
            }
            else if (part.Hidden == !included)
            {
                return;
            }
            else
            {
                part.Hidden = !included;
            }
            Changed();
        }

        public void SetPartMaterial(ObjectNodeAsset node, string submesh, string material)
        {
            ObjectPartAsset? part = node.Parts.FirstOrDefault(p => p.Submesh == submesh);
            if (part is null)
            {
                part = new ObjectPartAsset { Submesh = submesh };
                node.Parts.Add(part);
            }
            part.Material = material;
            Changed();
        }

        // The build's problems with the draft (empty when it would build).
        public List<string> Problems()
        {
            List<string> errors = new();
            Draft.Ordered(errors);
            return errors;
        }

        public void Apply() => SaveTo(Path);

        public void Revert()
        {
            Draft = Saved.Clone();
            Changed();
        }

        // Writes the draft to another asset path ("Assets/..."), which this driver then edits. The copy gets a new GUID.
        public bool SaveAs(string path)
        {
            if (!SaveTo(path, true))
                return false;
            Path = path;
            return true;
        }

        public void Dispose()
        {
        }

        private bool SaveTo(string path, bool copy = false)
        {
            if (ProjectService.Assets is not { } assets)
                return false;
            string full = assets.FullPath(path);
            Draft.EnsureComponentIds();
            ObjectAsset written = Draft.Clone();
            written.Meta = CurrentMeta(full) ?? written.Meta?.DeepClone().AsObject() ?? new JsonObject();
            if (copy || written.Meta["guid"] is null)
                written.Meta = new JsonObject { ["guid"] = AssetMeta.NewGuid(), ["tags"] = written.Meta["tags"]?.DeepClone() ?? new JsonArray() };
            JsonObject names = new();
            foreach (string guid in written.ReferencedGuids().Distinct().OrderBy(g => g, StringComparer.Ordinal))
                if (assets.PathOf(guid) is string referenced)
                    names[guid] = referenced;
            written.Meta["names"] = names;
            try
            {
                written.Write(full);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                Debug.LogError($"[Objects] Could not save {path}: {e.Message}");
                return false;
            }
            assets.RequestBuild(path);
            Draft.Meta = written.Meta.DeepClone().AsObject();
            Saved = Draft.Clone();
            Changed();
            Debug.Log($"[Objects] Saved {path}");
            return true;
        }

        private static JsonObject? CurrentMeta(string full)
        {
            if (!File.Exists(full))
                return null;
            try
            {
                return ObjectAsset.Read(full).Meta;
            }
            catch (Exception e) when (e is IOException or JsonException or InvalidDataException)
            {
                return null;
            }
        }

        // A referenced GUID that no longer resolves is looked up by the path the name tree recorded.
        private void RepairReferences(AssetDatabase assets)
        {
            if (Draft.Meta?["names"] is not JsonObject table)
                return;
            foreach ((string guid, JsonNode? recordedNode) in table)
            {
                string? recorded = recordedNode?.GetValue<string>();
                if (recorded is null || assets.PathOf(guid) is not null)
                    continue;
                if (assets.GuidOf(recorded) is string found && found != guid && Draft.ReplaceGuid(guid, found))
                    Debug.LogWarning($"[Objects] {Path}: {recorded} has a new GUID; the object now points to it (save to keep the fix)");
            }
        }
    }

    // 4x4 helpers for row-major matrices (row vectors).
    public static class ObjectMath
    {
        public static float[] Identity() => new float[] { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };

        public static (float X, float Y, float Z) Transform(float[] m, float x, float y, float z) =>
            (x * m[0] + y * m[4] + z * m[8] + m[12], x * m[1] + y * m[5] + z * m[9] + m[13], x * m[2] + y * m[6] + z * m[10] + m[14]);
    }
}
