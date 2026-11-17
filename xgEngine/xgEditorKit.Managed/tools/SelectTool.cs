// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.editor;
using xg.imui;

[assembly: ToolCategory("Scene", Icon = "Scene", Priority = 0, Tooltip = "Scene editing tools")]
[assembly: ToolGroup("Scene", "Select", IconColumns = 4, Width = 170.0f, Priority = 0, Tooltip = "Selection tool and selection commands")]

namespace xg.editor
{
    // The first editor tool and the default one. Like every tool it receives each selection change through
    // OnSelectItems; it keeps the selected items in Items for any system to read and, while active, lists them in its
    // ribbon group. Picking in the scene view comes once the scene view is wired to the tool system.
    [EditorTool("Select", Category = "Scene", Group = "Select", Icon = "Hand", Style = ToolItemStyle.Icon, ActiveByDefault = true,
        Tooltip = "Select actors and assets")]
    [ToolGUI(Group = "Select")]
    public sealed class SelectTool : EditorTool, IEditorGUIDraw
    {
        private const int ListedItems = 5;

        private object[] _items = Array.Empty<object>();

        // The selected items, as last delivered by the tool system.
        public IReadOnlyList<object> Items => _items;

        public override ToolUsability OnSelectItems(object[] selection)
        {
            _items = selection;
            return ToolUsability.Usable;
        }

        public void OnGUI()
        {
            int count = _items.Length;
            using (Gui.Font(FontStyle.Bold))
                Gui.Label(count == 0 ? "Nothing selected" : count == 1 ? "1 selected" : $"{count} selected");
            if (count == 0)
                return;
            object? active = Selection.ActiveObject;
            if (active is not null)
                Gui.PropertyLabel("Active", NameOf(active));
            for (int i = 0; i < Math.Min(count, ListedItems); i++)
                Gui.PropertyLabel(KindOf(_items[i]), NameOf(_items[i]));
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
