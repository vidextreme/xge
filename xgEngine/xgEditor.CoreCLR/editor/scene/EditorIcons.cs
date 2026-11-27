// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // The icons actors and Hierarchy groups show (editor-only, saved in the scene's editor blocks): resolution through
    // ComponentIcons (built-in Icon name or project image path, optional tint) and the setters the Hierarchy, the
    // Inspector and the icon picker share. A chosen icon without a tint uses the neutral icon colour (as the picker shows
    // it); only the default icons keep their kind colour.
    public static class EditorIcons
    {
        public static ComponentIcon Resolve(EditorIcon? icon, Icon fallback, Color32 defaultColor)
        {
            if (icon is not { } value || value.Name.Length == 0)
                return new ComponentIcon(fallback, null, defaultColor);
            return ComponentIcons.Resolve(value.Name, value.Color, fallback);
        }

        public static ComponentIcon OfActor(ActorID actor) =>
            Resolve(ActorEditorState.GetIcon(actor), Icon.Actor, Style.Color(StyleColor.IconObject));

        public static ComponentIcon OfFolder(SceneFolder folder, bool open) =>
            Resolve(folder.Icon, open ? Icon.GroupOpen : Icon.Group, HierarchyStyle.FolderIcon);

        public static ComponentIcon WithAlpha(ComponentIcon icon, byte alpha) =>
            new(icon.Icon, icon.Texture, new Color32(icon.Color.r, icon.Color.g, icon.Color.b, alpha));

        public static string Label(EditorIcon? icon) => icon?.Name ?? "default";

        // "#RRGGBB" (the # optional) as an opaque colour, or null.
        public static Color32? ParseColor(string? hex)
        {
            string text = hex?.TrimStart('#') ?? string.Empty;
            if (text.Length != 6 || !uint.TryParse(text, System.Globalization.NumberStyles.HexNumber, System.Globalization.CultureInfo.InvariantCulture, out uint value))
                return null;
            return new Color32((byte)(value >> 16), (byte)(value >> 8), (byte)value, 255);
        }

        // Whether the actor's icon can change: not a Composition Actor, not effectively locked.
        public static bool CanEdit(ActorID actor) =>
            actor.Value != 0 && !ArchetypeDriver.IsCompositionActor(actor) && !ActorEditorState.IsLockedInHierarchy(actor);

        public static void SetActors(IReadOnlyList<ActorID> actors, EditorIcon? icon)
        {
            ActorID[] editable = actors.Where(CanEdit).ToArray();
            ActorEditorState.SetIcon(editable, icon);
            Debug.Log($"[Debug] Icon set {Label(icon)} on {editable.Length} item(s)");
        }

        public static void SetFolder(EditorScene scene, SceneFolder folder, EditorIcon? icon)
        {
            if (SceneManager.IsFolderLocked(scene, folder.Id))
                return;
            SceneManager.SetFolderIcon(scene, folder, icon);
            Debug.Log($"[Debug] Icon set {Label(icon)} on 1 item(s)");
        }
    }
}
