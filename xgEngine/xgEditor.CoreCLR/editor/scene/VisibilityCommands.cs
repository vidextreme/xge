// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;

[assembly: ToolGroup("Scene", "Visibility", Width = 170.0f, Priority = 6, Tooltip = "Editor visibility and locks (the Scene view only; the game is unaffected)")]

namespace xgEditor.CoreCLR
{
    // Tools ribbon Scene > Visibility: the Show Hidden Actors override and the selection's hide and lock toggles.
    public static class VisibilityCommands
    {
        [ToolToggle("Show Hidden Actors", Category = "Scene", Group = "Visibility", Icon = nameof(Icon.Eye), Priority = 10, AllowLocked = true,
            State = nameof(ShowingHidden), Tooltip = "The Scene view draws hidden actors too; every actor keeps its own visibility")]
        public static bool ShowHidden(bool requested)
        {
            ActorEditorState.ShowHidden = requested;
            return requested;
        }

        [ToolToggle("Hide Selected", Category = "Scene", Group = "Visibility", Icon = nameof(Icon.EyeOff), Priority = 20, AllowLocked = true,
            Validate = nameof(HasActors), State = nameof(SelectionHidden), Tooltip = "Hide the selected actors in the Scene view (the game still draws them)")]
        public static bool HideSelected(bool requested)
        {
            ActorEditorState.SetHidden(Selection.Actors, requested);
            return requested;
        }

        [ToolToggle("Lock Selected", Category = "Scene", Group = "Visibility", Icon = nameof(Icon.Lock), Priority = 30, AllowLocked = true,
            Validate = nameof(HasActors), State = nameof(SelectionLocked), Tooltip = "Lock the selected actors: they stay selectable, but nothing edits them")]
        public static bool LockSelected(bool requested)
        {
            ActorEditorState.SetLocked(Selection.Actors, requested);
            return requested;
        }

        public static bool ShowingHidden() => ActorEditorState.ShowHidden;

        public static bool SelectionHidden() => Selection.Actors.Count > 0 && Selection.Actors.All(ActorEditorState.IsHidden);

        public static bool SelectionLocked() => Selection.Actors.Count > 0 && Selection.Actors.All(ActorEditorState.IsLocked);

        [ToolValidator]
        public static ToolUsability HasActors() => Selection.Actors.Count > 0 ? ToolUsability.Usable : ToolUsability.Unusable;
    }

    // The Hierarchy's lock, visibility and folder entries: on actors (hierarchy.item), folders (hierarchy.folder: targets
    // the SceneFolder and its EditorScene) and scene headers (hierarchy.background with the EditorScene as target).
    public sealed class VisibilityContextMenu : IContextMenuTool
    {
        public const string FolderSource = "hierarchy.folder";
        private const int MaxFolderEntries = 8;

        public string ContextMenuGroup => "Visibility";
        public int ContextMenuPriority => 10;
        public bool AllowLocked => true;

        public ToolUsability OnSelectItems(object[] selection) => ToolUsability.NotApplicable;

        public ToolUsability OnContextMenu(ContextMenuContext context) =>
            context.Source is SceneContextMenu.ItemSource or SceneContextMenu.BackgroundSource or FolderSource ? ToolUsability.Usable : ToolUsability.NotApplicable;

        public void OnContextMenuGUI(ContextMenu menu, ContextMenuContext context)
        {
            EditorScene? scene = context.Targets.OfType<EditorScene>().FirstOrDefault();
            if (context.Source == FolderSource && context.Targets.OfType<SceneFolder>().FirstOrDefault() is { } folder && scene is not null)
                FolderEntries(menu, scene, folder);
            else if (context.Source == SceneContextMenu.ItemSource && context.Targets.FirstOrDefault() is ActorID actor)
                ActorEntries(menu, actor);
            else if (context.Source == SceneContextMenu.BackgroundSource)
                SceneEntries(menu, scene ?? SceneManager.Active);
        }

        // Lock and visibility apply to the selection when the clicked actor is part of it.
        private static IReadOnlyList<ActorID> Targets(ActorID actor) => Selection.Contains(actor) ? Selection.Actors : new[] { actor };

        private static void ActorEntries(ContextMenu menu, ActorID actor)
        {
            IReadOnlyList<ActorID> targets = Targets(actor);
            bool locked = ActorEditorState.IsLocked(actor);
            bool hidden = ActorEditorState.IsHidden(actor);
            bool composed = ObjectDriverComponent.IsCompositionActor(actor);
            menu.Item(locked ? "Unlock" : "Lock", locked ? Icon.Unlock : Icon.Lock, () => ActorEditorState.SetLocked(targets, !locked),
                enabled: !composed, tooltip: composed ? "Composition Actors are driven by their object" : null);
            menu.Item(hidden ? "Show" : "Hide", hidden ? Icon.Eye : Icon.EyeOff, () => ActorEditorState.SetHidden(targets, !hidden));

            if (SceneManager.SceneOf(actor) is not { } scene || !IsRoot(actor))
                return;
            bool movable = !ActorEditorState.IsLockedInHierarchy(actor) && !scene.Locked;
            string? why = movable ? null : "Unlock the actor, its group and the scene first";
            menu.Separator();
            ActorID[] roots = targets.Where(a => SceneManager.SceneOf(a) == scene && IsRoot(a)).ToArray();
            menu.Item("Group Selected", Icon.Group, () =>
            {
                if (SceneManager.CreateFolder(scene, scene.FolderIdOf(actor)) is { } created)
                    foreach (ActorID root in roots)
                        SceneManager.MoveToFolder(root, created.Id);
            }, enabled: movable, tooltip: why);
            string? current = scene.FolderIdOf(actor);
            int shown = 0;
            foreach (SceneFolder folder in scene.Folders)
            {
                if (string.Equals(folder.Id, current, StringComparison.OrdinalIgnoreCase) || shown++ >= MaxFolderEntries)
                    continue;
                bool open = movable && !SceneManager.IsFolderLocked(scene, folder.Id);
                menu.Item($"Move to {FolderPath(scene, folder)}", Icon.Group, () =>
                {
                    foreach (ActorID root in roots)
                        SceneManager.MoveToFolder(root, folder.Id);
                }, enabled: open, tooltip: open ? null : why ?? "The group is locked");
            }
            if (current is not null)
                menu.Item("Remove from Group", Icon.Group, () =>
                {
                    foreach (ActorID root in roots)
                        SceneManager.MoveToFolder(root, null);
                }, enabled: movable, tooltip: why);
        }

        private static bool IsRoot(ActorID actor) => ActorTree.GetParent(actor) is not { } parent || parent.Value == 0;

        private static void FolderEntries(ContextMenu menu, EditorScene scene, SceneFolder folder)
        {
            bool locked = SceneManager.IsFolderLocked(scene, folder.Id);
            string? why = locked ? "The group, a parent group or the scene is locked" : null;
            menu.Item("New Group Inside", Icon.Group, () =>
            {
                if (SceneManager.CreateFolder(scene, folder.Id) is { } created)
                    RenameFolder(scene, created);
            }, enabled: !locked, tooltip: why);
            menu.Item("Rename...", Icon.File, () => RenameFolder(scene, folder), enabled: !locked, tooltip: why);
            menu.Item("Remove Group", Icon.Trash, () => SceneCommands.ConfirmRemoveGroup(scene, folder), enabled: !locked, tooltip: why ?? "Its actors and groups move up");
            menu.Separator();
            menu.Item(folder.Locked ? "Unlock" : "Lock", folder.Locked ? Icon.Unlock : Icon.Lock, () => SceneManager.SetFolderLocked(scene, folder, !folder.Locked));
            menu.Item(folder.Hidden ? "Show" : "Hide", folder.Hidden ? Icon.Eye : Icon.EyeOff, () => SceneManager.SetFolderHidden(scene, folder, !folder.Hidden));
        }

        private static void SceneEntries(ContextMenu menu, EditorScene? scene)
        {
            if (scene is null)
                return;
            menu.Item("New Group", Icon.Group, () =>
            {
                if (SceneManager.CreateFolder(scene, null) is { } created)
                    RenameFolder(scene, created);
            }, enabled: !scene.Locked, tooltip: scene.Locked ? "The scene is locked" : null);
            menu.Item(scene.Locked ? "Unlock Scene" : "Lock Scene", scene.Locked ? Icon.Unlock : Icon.Lock, () => SceneManager.SetSceneLocked(scene, !scene.Locked));
            menu.Item(scene.Hidden ? "Show Scene" : "Hide Scene", scene.Hidden ? Icon.Eye : Icon.EyeOff, () => SceneManager.SetSceneHidden(scene, !scene.Hidden));
        }

        public static string FolderPath(EditorScene scene, SceneFolder folder)
        {
            List<string> names = new() { folder.Name };
            for (SceneFolder? parent = scene.FindFolder(folder.Parent); parent is not null && names.Count < 16; parent = scene.FindFolder(parent.Parent))
                names.Insert(0, parent.Name);
            return string.Join(" / ", names);
        }

        // The shared Rename dialog for a folder (opened a couple of frames later, after the menu closes).
        public static void RenameFolder(EditorScene scene, SceneFolder folder)
        {
            SceneFolder? parent = scene.FindFolder(folder.Parent);
            RenameDialog.Open(new RenameRequest
            {
                Title = "Rename Group",
                Context = parent is null ? scene.Name : scene.Name + " / " + FolderPath(scene, parent),
                Name = folder.Name,
                DrawPreview = (draw, rect) => draw.Icon(Icon.Group, rect.Center, 28.0f, HierarchyStyle.FolderIcon),
                Commit = name => SceneManager.RenameFolder(scene, folder, name) ? null : "The group is locked",
            });
        }
    }
}
