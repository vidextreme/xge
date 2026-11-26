// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;

[assembly: ToolGroup("Scene", "Actors", Width = 170.0f, Priority = 5, Tooltip = "Add actors to the scene")]

namespace xgEditor.CoreCLR
{
    // File menu scene entries, the Tools ribbon's Scene > Actors group, and the Hierarchy's context menu.
    public static class SceneCommands
    {
        private static bool _registered;

        public static void RegisterMenus()
        {
            if (_registered)
                return;
            _registered = true;
            Menu.Add("File/New Scene", () => SceneDialogs.Guard(SceneManager.New));
            Menu.Add("File/Open Scene...", SceneDialogs.OpenPicker);
            Menu.Add("File/Save Scene", () => SceneManager.Save());
            Menu.Add("File/Save Scene As...", () =>
            {
                if (SceneManager.Active is { } scene)
                    SceneDialogs.OpenSaveAs(scene);
            });
        }

        [ToolButton("Create Empty Actor", Category = "Scene", Group = "Actors", Icon = nameof(Icon.Object), Priority = 10,
            Tooltip = "Add an empty actor to the active scene (under the selected scene actor, or in the selected group)")]
        public static void CreateEmpty()
        {
            ActorID selected = Selection.ActiveActor;
            ActorID? parent = selected.Value != 0 && SceneManager.IsMember(selected) ? selected : null;
            if (SceneManager.CreateActor("Actor", parent) is { } actor)
                Selection.Set(actor.ID);
        }

        [ToolButton("Create Camera", Category = "Scene", Group = "Actors", Icon = nameof(Icon.Camera), Priority = 15,
            Tooltip = "Add a camera actor to the active scene (under the selected scene actor, or in the selected group)")]
        public static void CreateCamera()
        {
            ActorID selected = Selection.ActiveActor;
            ActorID? parent = selected.Value != 0 && SceneManager.IsMember(selected) ? selected : null;
            if (SceneManager.CreateCamera(parent) is { } actor)
                Selection.Set(actor.ID);
        }

        [ToolButton("Add Selected Object", Category = "Scene", Group = "Actors", Icon = nameof(Icon.Object), Priority = 20,
            Validate = nameof(ObjectSelected), Tooltip = "Add an actor that draws the selected composition object (.xco)")]
        public static void AddSelectedObject()
        {
            if (SelectedObject() is string path && SceneManager.CreateFromObject(path) is { } actor)
                Selection.Set(actor.ID);
        }

        [ToolButton("Remove Actor", Category = "Scene", Group = "Actors", Icon = nameof(Icon.Trash), Priority = 30,
            Validate = nameof(ActorsRemovable), Tooltip = "Remove the selected scene actors and their children (asks first)")]
        public static void RemoveSelectedActors() => ConfirmRemoveActors(RemovableActors(Selection.Actors));

        [ToolButton("Remove Group", Category = "Scene", Group = "Actors", Icon = nameof(Icon.Trash), Priority = 40,
            Validate = nameof(GroupRemovable), Tooltip = "Remove the group selected in the Hierarchy; its actors and groups move up (asks first)")]
        public static void RemoveSelectedGroup()
        {
            if (SceneManager.FolderTarget is { } target)
                ConfirmRemoveGroup(target.Scene, target.Folder);
        }

        [ToolValidator]
        public static ToolUsability ActorsRemovable() => RemovableActors(Selection.Actors).Count > 0 ? ToolUsability.Usable : ToolUsability.Unusable;

        [ToolValidator]
        public static ToolUsability GroupRemovable() =>
            SceneManager.FolderTarget is { } target && !SceneManager.IsFolderLocked(target.Scene, target.Folder.Id) ? ToolUsability.Usable : ToolUsability.Unusable;

        // Scene members that can be removed (no Composition Actors, nothing locked), without those under another one.
        public static List<ActorID> RemovableActors(IEnumerable<ActorID> actors)
        {
            HashSet<ulong> candidates = actors.Where(a => a.Value != 0 && SceneManager.IsMember(a) && !ObjectDriverComponent.IsCompositionActor(a)
                && !ActorEditorState.IsLockedInSubtree(a)).Select(a => (ulong)a.Value).ToHashSet();
            List<ActorID> result = new();
            foreach (ulong id in candidates)
            {
                bool nested = false;
                for (ActorID? parent = ActorTree.GetParent(new ActorID(id)); parent is { } p && p.Value != 0 && !nested; parent = ActorTree.GetParent(p))
                    nested = candidates.Contains(p.Value);
                if (!nested)
                    result.Add(new ActorID(id));
            }
            return result;
        }

        public static void ConfirmRemoveActors(IReadOnlyList<ActorID> actors)
        {
            if (actors.Count == 0)
                return;
            string first = ActorTree.GetName(actors[0]);
            bool children = actors.Any(a => ActorTree.ChildCount(a) > 0);
            ConfirmDialog.Open(new ConfirmRequest
            {
                Title = actors.Count == 1 ? "Remove Actor" : "Remove Actors",
                Message = actors.Count == 1 ? $"Remove {first}?" : $"Remove {actors.Count} actors?",
                Detail = children ? "Their children are removed too." : "Its components and values are lost.",
                DrawPreview = (draw, rect) => draw.Icon(Icon.Object, rect.Center, 28.0f, Style.Color(StyleColor.IconObject)),
                Confirm = () =>
                {
                    int removed = 0;
                    foreach (ActorID actor in actors.ToArray())
                        if (SceneManager.Destroy(actor))
                            ++removed;
                    if (removed > 0)
                        Selection.Clear();
                    Debug.Log($"[Debug] Removed {removed} actor(s)");
                },
            });
        }

        public static void ConfirmRemoveGroup(EditorScene scene, SceneFolder folder)
        {
            SceneFolder? parent = scene.FindFolder(folder.Parent);
            ConfirmDialog.Open(new ConfirmRequest
            {
                Title = "Remove Group",
                Message = $"Remove the group {folder.Name}?",
                Detail = $"Its actors and groups move up to {parent?.Name ?? scene.Name}.",
                DrawPreview = (draw, rect) => draw.Icon(Icon.Group, rect.Center, 28.0f, HierarchyStyle.FolderIcon),
                Confirm = () =>
                {
                    if (SceneManager.DeleteFolder(scene, folder))
                    {
                        if (SceneManager.FolderTarget is { } target && target.Folder == folder)
                            SceneManager.FolderTarget = null;
                        Debug.Log($"[Debug] Removed group {folder.Name}");
                    }
                },
            });
        }

        [ToolValidator]
        public static ToolUsability ObjectSelected() => SelectedObject() is not null ? ToolUsability.Usable : ToolUsability.Unusable;

        internal static string? SelectedObject() =>
            Selection.Objects.OfType<AssetRef>().Where(a => !string.IsNullOrEmpty(a.Path) && ObjectAsset.IsObject(a.Path)).Select(a => a.Path).LastOrDefault();
    }

    // The Hierarchy's right-click menu: create actors in the scene (as children of a clicked scene actor), add the
    // selected .xco, rename and delete scene actors.
    public sealed class SceneContextMenu : IContextMenuTool
    {
        // The shared Rename dialog for a scene actor: its parent path as context, the object icon as preview.
        public static void RenameActor(ActorID actor)
        {
            List<string> path = new();
            for (ActorID? parent = ActorTree.GetParent(actor); parent is { } p && path.Count < 64; parent = ActorTree.GetParent(p))
                path.Insert(0, ActorTree.GetName(p));
            string scene = SceneManager.SceneOf(actor)?.Name ?? "Scene";
            RenameDialog.Open(new RenameRequest
            {
                Title = "Rename Actor",
                Context = string.Join(" / ", path.Prepend(scene)),
                Name = ActorTree.GetName(actor),
                DrawPreview = (draw, rect) => draw.Icon(Icon.Object, rect.Center, 28.0f, Style.Color(StyleColor.IconObject)),
                Commit = name => SceneManager.Rename(actor, name) ? null : "Only scene actors can be renamed",
            });
        }

        public const string ItemSource = "hierarchy.item";
        public const string BackgroundSource = "hierarchy.background";

        public string ContextMenuGroup => "Scene";

        public ToolUsability OnSelectItems(object[] selection) => ToolUsability.Usable;

        public ToolUsability OnContextMenu(ContextMenuContext context) =>
            context.Source is ItemSource or BackgroundSource or VisibilityContextMenu.FolderSource ? ToolUsability.Usable : ToolUsability.NotApplicable;

        public void OnContextMenuGUI(ContextMenu menu, ContextMenuContext context)
        {
            ActorID? target = context.Source == ItemSource && context.Targets.FirstOrDefault() is ActorID actor ? actor : null;
            bool member = target is { } t && SceneManager.IsMember(t);
            ActorID? parent = member ? target : null;
            (EditorScene Scene, SceneFolder Folder)? folder = context.Source == VisibilityContextMenu.FolderSource
                && context.TryGet(out SceneFolder? clicked) && context.TryGet(out EditorScene? owner) ? (owner!, clicked!)
                : parent is null && context.Source == BackgroundSource ? SceneManager.FolderTarget : null;
            Actor? Create(Func<Actor?> create) => folder is { } f ? SceneManager.CreateInFolder(f.Scene, f.Folder, create) : create();
            string label = parent is not null ? "Create Child Actor" : folder is { } named ? $"Create Actor in {named.Folder.Name}" : "Create Empty Actor";
            menu.Item(label, Icon.Object, () =>
            {
                if (Create(() => SceneManager.CreateActor("Actor", parent)) is { } created)
                    Selection.Set(created.ID);
            });
            menu.Item("Create Camera", Icon.Camera, () =>
            {
                if (Create(() => SceneManager.CreateCamera(parent)) is { } created)
                    Selection.Set(created.ID);
            });
            string? objectPath = SceneCommands.SelectedObject();
            menu.Item(objectPath is null ? "Add Selected Object" : $"Add {AssetNames.DisplayName(objectPath)}", Icon.Object, () =>
            {
                if (objectPath is not null && Create(() => SceneManager.CreateFromObject(objectPath, parent)) is { } created)
                    Selection.Set(created.ID);
            }, enabled: objectPath is not null, tooltip: objectPath is null ? "Select a composition object (.xco) in the Navigator first" : null);
            if (target is not { } chosen)
                return;
            bool composed = ObjectDriverComponent.IsCompositionActor(chosen);
            string why = composed ? "Composition Actors belong to their object" : "Only scene actors can be changed here";
            menu.Separator();
            menu.Item("Rename...", Icon.File, () => RenameActor(chosen), enabled: member && !composed, tooltip: member && !composed ? null : why);
            List<ActorID> removable = SceneCommands.RemovableActors(Selection.Contains(chosen) ? Selection.Actors.Append(chosen) : new[] { chosen });
            menu.Item(removable.Count > 1 ? $"Remove {removable.Count} Actors" : "Remove Actor", Icon.Trash, () => SceneCommands.ConfirmRemoveActors(removable),
                enabled: member && !composed && removable.Count > 0, tooltip: member && !composed ? removable.Count > 0 ? null : "The actor or one under it is locked" : why);
        }
    }
}
