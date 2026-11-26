// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;

namespace xgEditor.CoreCLR
{
    // The Mesh Renderer card: Visible, then Mesh and Material with the same table as the Archetype Editor (a
    // draw check and a material per submesh, stored in MeshRendererData's submesh entries) and Edit Here. With Auto
    // Apply on, the actor draws the edited material's unsaved draft in every view (an editor-only override) until Save
    // or Revert. One table per actor, dropped a few seconds after the actor stops being shown.
    public sealed class MeshRendererDataEditor : IComponentDataEditor
    {
        private const long ForgetAfterMs = 3000;

        private readonly Dictionary<ulong, Entry> _entries = new();

        public string DataType => typeof(MeshRendererData).FullName!;

        // The table shown for an actor (hooks), or null when its card hasn't been drawn.
        public SubmeshMaterialTable? TableFor(ActorID actor) => _entries.TryGetValue(actor.Value, out Entry? entry) ? entry.Table : null;

        public object OnGUI(ActorID actor, object data)
        {
            MeshRendererData renderer = data is MeshRendererData own ? own : (MeshRendererData)(TypeBridge.Convert(data, typeof(MeshRendererData)) ?? default(MeshRendererData));
            Forget();
            if (!_entries.TryGetValue(actor.Value, out Entry? entry))
            {
                entry = new Entry(new SubmeshMaterialTable("mesh_renderer_" + actor.Value));
                _entries[actor.Value] = entry;
            }
            entry.LastUsed = Environment.TickCount64;

            Rows rows = new(renderer);
            bool visible = (renderer.Flags & MeshRendererFlags.Hidden) == 0;
            if (Gui.Checkbox("Visible", ref visible))
            {
                rows.Data.Flags = visible ? rows.Data.Flags & ~MeshRendererFlags.Hidden : rows.Data.Flags | MeshRendererFlags.Hidden;
                rows.Changed = true;
            }
            Gui.Tooltip("Draw the model (the actor keeps its settings while hidden)");
            Gui.Space(4.0f);
            using (Gui.Font(FontStyle.Bold))
                Gui.Label("Mesh and Material");
            int submeshes = ModelSubmeshes.Of(rows.Model).Count;
            entry.Table.Note = submeshes > MeshRendererData.MaxSubmeshes
                ? $"Up to {MeshRendererData.MaxSubmeshes} submeshes keep their own setting."
                : null;
            entry.Table.OnGUI(rows);
            UpdateOverride(actor, entry);

            if (!rows.Changed)
                return data;
            return data is MeshRendererData ? rows.Data : TypeBridge.Convert(rows.Data, data.GetType()) ?? data;
        }

        private static void UpdateOverride(ActorID actor, Entry entry)
        {
            MaterialEditorView view = entry.Table.MaterialView;
            ulong draft = view.AutoApply && view.Driver is { HasPendingChanges: true } ? view.DraftMaterial : 0;
            string guid = draft != 0 ? entry.Table.EditMaterial : string.Empty;
            if (draft == entry.Draft && guid == entry.DraftGuid)
                return;
            if (draft != 0 && ResourceID.TryParse(guid, out ResourceID id))
                MeshRenderer.SetEditorMaterialOverride(actor, id.File, draft);
            else
                MeshRenderer.SetEditorMaterialOverride(actor, default, 0);
            entry.Draft = draft;
            entry.DraftGuid = guid;
        }

        private void Forget()
        {
            long now = Environment.TickCount64;
            foreach ((ulong actor, Entry entry) in _entries.Where(p => now - p.Value.LastUsed > ForgetAfterMs).ToList())
            {
                if (entry.Draft != 0)
                    MeshRenderer.SetEditorMaterialOverride(new ActorID(actor), default, 0);
                entry.Table.Dispose();
                _entries.Remove(actor);
            }
        }

        private sealed class Entry
        {
            public Entry(SubmeshMaterialTable table) => Table = table;
            public SubmeshMaterialTable Table { get; }
            public long LastUsed;
            public ulong Draft;
            public string DraftGuid = string.Empty;
        }

        // The renderer's submesh entries as table rows. A submesh without an entry is drawn with the renderer's Material;
        // editing it adds an entry (up to MeshRendererData.MaxSubmeshes).
        private sealed class Rows : ISubmeshMaterials
        {
            public MeshRendererData Data;
            public bool Changed;

            public Rows(MeshRendererData data) => Data = data;

            public string Model => Data.Model.ToString();

            public void SetModel(string modelGuid)
            {
                Data.Model = AssetReference.FromText(modelGuid);
                Data.ClearSubmeshes();
                Changed = true;
            }

            public bool IsDrawn(string submesh) => Find(submesh) is not int index || !Data.GetSubmesh(index).Hidden;

            public void SetDrawn(string submesh, bool drawn)
            {
                if (Ensure(submesh) is not int index)
                    return;
                MeshRendererSubmesh entry = Data.GetSubmesh(index);
                entry.Flags = drawn ? entry.Flags & ~MeshRendererSubmeshFlags.Hidden : entry.Flags | MeshRendererSubmeshFlags.Hidden;
                Data.SetSubmesh(index, entry);
                Changed = true;
            }

            public string MaterialOf(string submesh) => (Find(submesh) is int index ? Data.GetSubmesh(index).Material : Data.Material).ToString();

            public void SetMaterial(string submesh, string material)
            {
                if (Ensure(submesh) is not int index)
                    return;
                MeshRendererSubmesh entry = Data.GetSubmesh(index);
                entry.Material = AssetReference.FromText(material);
                Data.SetSubmesh(index, entry);
                Changed = true;
            }

            public bool CanEdit(string submesh) => Find(submesh) is not null || Data.SubmeshCount < MeshRendererData.MaxSubmeshes;

            private int? Find(string submesh) =>
                ResourceID.TryParse(submesh, out ResourceID id) && Data.FindSubmesh(id.File) is int index and >= 0 ? index : null;

            private int? Ensure(string submesh) =>
                ResourceID.TryParse(submesh, out ResourceID id) && Data.EnsureSubmesh(id.File) is int index and >= 0 ? index : null;
        }
    }
}
