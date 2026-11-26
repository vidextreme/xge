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
            Tooltip = "Add an actor with a transform to the active scene (under the selected scene actor)")]
        public static void CreateEmpty()
        {
            ActorID selected = Selection.ActiveActor;
            ActorID? parent = selected.Value != 0 && SceneManager.IsMember(selected) ? selected : null;
            if (SceneManager.CreateActor("Actor", parent) is { } actor)
                Selection.Set(actor.ID);
        }

        [ToolButton("Add Selected Object", Category = "Scene", Group = "Actors", Icon = nameof(Icon.Object), Priority = 20,
            Validate = nameof(ObjectSelected), Tooltip = "Add an actor that draws the selected composition object (.xco)")]
        public static void AddSelectedObject()
        {
            if (SelectedObject() is string path && SceneManager.CreateFromObject(path) is { } actor)
                Selection.Set(actor.ID);
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
            menu.Item("Delete", Icon.Trash, () =>
            {
                if (SceneManager.Destroy(chosen))
                    Selection.Clear();
            }, enabled: member && !composed, tooltip: member && !composed ? null : why);
        }
    }
}
