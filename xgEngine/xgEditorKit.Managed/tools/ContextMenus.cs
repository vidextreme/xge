// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.imui;
using xg.math;

namespace xg.editor
{
    // A tool that adds entries to context menus. Any concrete class can implement it: the tools Tools already holds
    // ([EditorTool], [InspectorTool], open panel tools) are asked directly, and Tools creates one instance of every other
    // concrete IContextMenuTool with a parameterless constructor. When a panel opens a menu (ContextMenuHost.Open), every
    // tool is asked whether it can add entries for that context, and the usable ones draw them, ordered by priority and
    // separated by group.
    public interface IContextMenuTool : ITool
    {
        // Usable: the entries show. Unusable: they show disabled. NotApplicable: nothing is added.
        ToolUsability OnContextMenu(ContextMenuContext context);

        // Adds this tool's entries for the context.
        void OnContextMenuGUI(ContextMenu menu, ContextMenuContext context);

        // Entries of different groups are separated; lower priorities come first.
        string ContextMenuGroup => GetType().Name;
        int ContextMenuPriority => 0;

        ToolUsability ITool.OnSelectItems(object[] selection) => ToolUsability.NotApplicable;
    }

    // What a context menu was opened on.
    public sealed class ContextMenuContext
    {
        public ContextMenuContext(string source, object[] targets, object? owner)
        {
            Source = source;
            Targets = targets;
            Owner = owner;
        }

        // Where the menu was opened, e.g. "navigator.item" or "navigator.background".
        public string Source { get; }
        // The right-clicked items (AssetRef, folder path, ActorID, ...).
        public object[] Targets { get; }
        // The panel that opened the menu.
        public object? Owner { get; }

        // Owner as T, else the first target of type T.
        public bool TryGet<T>([System.Diagnostics.CodeAnalysis.MaybeNullWhen(false)] out T value)
        {
            if (Owner is T owner)
            {
                value = owner;
                return true;
            }
            foreach (object target in Targets)
            {
                if (target is T match)
                {
                    value = match;
                    return true;
                }
            }
            value = default;
            return false;
        }

        public T? Get<T>() where T : class => TryGet(out T? value) ? value : null;
    }

    // The entries of one context menu, written by context menu tools and drawn by ContextMenuHost.
    public sealed class ContextMenu
    {
        public const float Width = 180.0f;
        public const float RowHeight = 22.0f;
        public const float Padding = 4.0f;
        public const float Gap = 2.0f;
        public const float SeparatorHeight = 7.0f;

        internal sealed class Entry
        {
            public string? Label;
            public Icon Icon;
            public Action? Clicked;
            public bool Enabled;
            public string? Tooltip;
            public float Height;
            public Action<Painter, Rect>? Draw;
            public bool Separator;
        }

        internal readonly List<Entry> Entries = new();
        internal bool Disabled;

        public int Count => Entries.Count(e => !e.Separator);

        public void Item(string label, Icon icon, Action clicked, bool enabled = true, string? tooltip = null) =>
            Entries.Add(new Entry { Label = label, Icon = icon, Clicked = clicked, Enabled = enabled && !Disabled, Tooltip = tooltip, Height = RowHeight });

        public void Separator()
        {
            if (Entries.Count > 0 && !Entries[^1].Separator)
                Entries.Add(new Entry { Separator = true, Height = SeparatorHeight });
        }

        // A row drawn by the tool in rect (menu canvas space).
        public void Custom(float height, Action<Painter, Rect> draw) =>
            Entries.Add(new Entry { Height = Math.Max(height, 1.0f), Draw = draw });

        internal float Height()
        {
            float height = Padding * 2.0f;
            for (int i = 0; i < Entries.Count; i++)
                height += Entries[i].Height + (i > 0 ? Gap : 0.0f);
            return height;
        }
    }

    // One context menu of a panel: Open it from the right-click, then call OnGUI every frame from the same ID scope (and
    // draw space) the position refers to.
    public sealed class ContextMenuHost
    {
        private readonly string _id;
        private ContextMenuContext? _context;
        private Vector2 _position;
        private bool _atMouse;
        private bool _open;

        public ContextMenuHost(string id) => _id = id;

        public ContextMenuContext? Context => _context;
        public bool IsOpen { get; private set; }

        // Opens the menu for context at the mouse cursor on the next OnGUI.
        public void Open(ContextMenuContext context)
        {
            _context = context;
            _atMouse = true;
            _open = true;
        }

        // Opens the menu for context at position (OnGUI's draw space) on the next OnGUI.
        public void Open(ContextMenuContext context, Vector2 position)
        {
            _context = context;
            _position = position;
            _atMouse = false;
            _open = true;
        }

        // Builds the entries the context menu tools add for context.
        public static ContextMenu Build(ContextMenuContext context)
        {
            ContextMenu menu = new();
            Tools? tools = Subsystem.Get<Tools>();
            if (tools is null)
                return menu;
            string? group = null;
            foreach (IContextMenuTool tool in tools.ContextMenuTools)
            {
                ToolUsability usability = tools.SafeMenuUsability(tool, context);
                if (usability == ToolUsability.NotApplicable)
                    continue;
                if (group is not null && group != tool.ContextMenuGroup)
                    menu.Separator();
                int before = menu.Entries.Count;
                menu.Disabled = usability == ToolUsability.Unusable;
                tools.SafeMenuGUI(tool, menu, context);
                menu.Disabled = false;
                if (menu.Entries.Count > before)
                    group = tool.ContextMenuGroup;
            }
            if (menu.Entries.Count > 0 && menu.Entries[^1].Separator)
                menu.Entries.RemoveAt(menu.Entries.Count - 1);
            return menu;
        }

        public void OnGUI()
        {
            ContextMenu? menu = null;
            if (_open)
            {
                _open = false;
                if (_context is not null)
                {
                    if (_atMouse)
                        _position = Gui.MousePosition;
                    menu = Build(_context);
                    Debug.Log($"[Debug] Context menu {_context.Source}: {menu.Count} entries");
                    if (menu.Count > 0)
                        Gui.OpenPopup(_id);
                }
            }
            using var popup = Gui.Popup(_id, _position, new Vector2(ContextMenu.Width, 0.0f));
            IsOpen = popup.IsOpen;
            if (!popup.IsOpen || _context is null)
                return;
            menu ??= Build(_context);
            using var canvas = Gui.Canvas(_id + "_rows", new Vector2(ContextMenu.Width, menu.Height()));
            Painter draw = canvas.Draw;
            float inner = ContextMenu.Width - ContextMenu.Padding * 2.0f;
            float y = ContextMenu.Padding;
            Action? clicked = null;
            for (int i = 0; i < menu.Entries.Count; i++)
            {
                ContextMenu.Entry entry = menu.Entries[i];
                Rect rect = Rect.FromXYWH(ContextMenu.Padding, y, inner, entry.Height);
                if (entry.Separator)
                    draw.Line(new Vector2(rect.min.x + 2.0f, MathF.Round(rect.min.y + entry.Height * 0.5f) + 0.5f),
                        new Vector2(rect.max.x - 2.0f, MathF.Round(rect.min.y + entry.Height * 0.5f) + 0.5f), Style.Color(StyleColor.Border));
                else if (entry.Draw is not null)
                    entry.Draw(draw, rect);
                else
                {
                    using (Gui.Id(i))
                    {
                        if (ToggleButton.Draw(draw, "##entry", rect, entry.Icon, null, entry.Label, null, entry.Tooltip, false, entry.Enabled, ToggleDot.None).Clicked())
                            clicked = entry.Clicked;
                    }
                }
                y += entry.Height + ContextMenu.Gap;
            }
            if (clicked is null)
                return;
            Gui.ClosePopup();
            clicked();
        }
    }
}
