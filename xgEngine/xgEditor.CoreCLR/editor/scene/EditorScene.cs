// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text.Json.Nodes;
using xg;
using xg.editor;

namespace xgEditor.CoreCLR
{
    // One loaded scene: its file (null while Untitled), GUID and meta, its members (runtime ActorID -> scene-local id),
    // the last known component order per member (rebinds component ids after hot reload or play mode) and the content
    // JSON of the last save or load (what IsDirty compares the current capture with). Several can be loaded at once;
    // SceneManager owns them.
    public sealed class EditorScene
    {
        public EditorScene(string? path, string guid)
        {
            Path = path;
            Guid = guid;
        }

        public string? Path { get; internal set; }
        public string Guid { get; internal set; }
        public JsonObject? Meta { get; internal set; }
        public JsonObject Extra { get; internal set; } = new();
        public Dictionary<ulong, string> Members { get; } = new();
        public Dictionary<ulong, List<(string Type, string Id)>> ComponentOrder { get; } = new();
        // Unknown keys of each member's saved entry, written back on save.
        public Dictionary<ulong, JsonObject> ActorExtra { get; } = new();
        public string Snapshot { get; internal set; } = string.Empty;
        // Set while instance overrides wait for their Composition Actors after a load; the snapshot is retaken after.
        public int SettleFrames { get; internal set; }
        public bool IsDirty { get; internal set; }
        internal long CheckedAt { get; set; }

        public string Name => Path is null ? "Untitled" : AssetNames.DisplayName(Path);

        public bool Contains(ActorID actor) => Members.ContainsKey(actor.Value);

        public string? IdOf(ActorID actor) => Members.TryGetValue(actor.Value, out string? id) ? id : null;

        // Editor flags of the scene itself (saved in the .xcs "editor" block; they override everything in it).
        public bool Hidden { get; internal set; }
        public bool Locked { get; internal set; }

        // Hierarchy folders in tree order, and the folder of each root member (absent = the scene root).
        public List<SceneFolder> Folders { get; } = new();
        public Dictionary<ulong, string> FolderOf { get; } = new();

        public SceneFolder? FindFolder(string? id) =>
            string.IsNullOrEmpty(id) ? null : Folders.FirstOrDefault(f => string.Equals(f.Id, id, StringComparison.OrdinalIgnoreCase));

        public string? FolderIdOf(ActorID actor) => FolderOf.TryGetValue(actor.Value, out string? id) ? id : null;

        // The folder's state with its parent folders and the scene (hidden or locked by any of them).
        public (bool Hidden, bool Locked) FolderState(string? folderId)
        {
            bool hidden = Hidden;
            bool locked = Locked;
            SceneFolder? folder = FindFolder(folderId);
            for (int guard = 0; folder is not null && guard < 256; guard++)
            {
                hidden |= folder.Hidden;
                locked |= folder.Locked;
                folder = FindFolder(folder.Parent);
            }
            return (hidden, locked);
        }

        // True when ancestor is folderId or one of its parent folders.
        public bool IsInFolder(string? folderId, string ancestor)
        {
            SceneFolder? folder = FindFolder(folderId);
            for (int guard = 0; folder is not null && guard < 256; guard++)
            {
                if (string.Equals(folder.Id, ancestor, StringComparison.OrdinalIgnoreCase))
                    return true;
                folder = FindFolder(folder.Parent);
            }
            return false;
        }
    }

    // A Hierarchy folder of a scene: editor-only (no actor). It holds root actors and other folders.
    public sealed class SceneFolder
    {
        public string Id { get; init; } = string.Empty;
        public string Name { get; set; } = string.Empty;
        // The parent folder id, empty for the scene root.
        public string Parent { get; set; } = string.Empty;
        public bool Hidden { get; set; }
        public bool Locked { get; set; }
        // The chosen icon, or null for the default group icon.
        public EditorIcon? Icon { get; set; }
    }
}
