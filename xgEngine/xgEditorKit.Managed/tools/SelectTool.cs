// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.editor;
using xg.imui;

[assembly: ToolCategory("Scene", Icon = "Scene", Priority = 0, Tooltip = "Scene editing tools")]
[assembly: ToolGroup("Scene", "Select", IconColumns = 4, Width = 170.0f, Priority = 0, Tooltip = "Selection tool and selection commands")]

namespace xg.editor
{
    // The first editor tool and the default one. It's usable with any selection and, while active, lists the editor
    // selection in its ribbon group. Picking in the scene view comes once the scene view is wired to the tool system.
    // Like every tool it keeps no selection of its own: Items is the editor selection.
    [EditorTool("Select", Category = "Scene", Group = "Select", Icon = "Hand", Style = ToolItemStyle.Icon, ActiveByDefault = true,
        Tooltip = "Select actors and assets")]
    [ToolGUI(Group = "Select")]
    public sealed class SelectTool : EditorTool, IEditorGUIDraw
    {
        private const int ListedItems = 5;

        public IReadOnlyList<object> Items => Selection.Objects;

        public void OnGUI()
        {
            object[] items = Selection.Objects;
            int count = items.Length;
            using (Gui.Font(FontStyle.Bold))
                Gui.Label(count == 0 ? "Nothing selected" : count == 1 ? "1 selected" : $"{count} selected");
            if (count == 0)
                return;
            object? active = Selection.ActiveObject;
            if (active is not null)
                Gui.PropertyLabel("Active", NameOf(active));
            for (int i = 0; i < Math.Min(count, ListedItems); i++)
                Gui.PropertyLabel(KindOf(items[i]), NameOf(items[i]));
            if (count > ListedItems)
                Gui.Label($"... and {count - ListedItems} more");
        }

        private static string KindOf(object item) => item switch
        {
            ActorID => "Actor",
            AssetRef => "Asset",
            _ => item.GetType().Name,
        };

        private static string NameOf(object item) => item switch
        {
            ActorID actor => ActorTree.GetName(actor),
            AssetRef asset => asset.Name,
            _ => item.ToString() ?? "",
        };
    }
}
