// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // The icon picker flyout for actors and Hierarchy groups: a search field, the built-in icons and the project's icons
    // (IconLibrary: images in Assets/**/Icons), a hover line with the icon's name, tint swatches for built-in icons and
    // Reset to Default. One instance per host panel: Open(...) from anywhere in the panel, OnGUI() every frame at panel
    // level (the popup opens at the mouse cursor, or at a fixed spot for hooks).
    public sealed class IconPicker
    {
        private const float Width = 300.0f;
        private const float ListHeight = 250.0f;
        private const int Columns = 8;
        private const float CellHeight = 30.0f;
        private const float SectionHeight = 24.0f;
        private const float SwatchSize = 12.0f;
        private const float SwatchStep = 16.0f;

        private static readonly string?[] _tints = { null, "#7CC4FF", "#7BD88F", "#E9B949", "#E35D5B", "#B48EF0" };

        private readonly string _id;
        private readonly TextBuffer _filter = new(64);
        private EditorIcon? _current;
        private Icon _default;
        private Action<EditorIcon?>? _apply;
        private Vector2 _position;
        private Vector2? _fixedPosition;
        private bool _openRequested;
        private bool _focusSearch;
        private string _hover = string.Empty;
        private string? _hookSearch;
        private string? _hookPick;
        private int _hookPickIn = -1;

        private readonly struct Choice
        {
            public Choice(string name, string label, Icon icon, AssetEntry? entry)
            {
                Name = name;
                Label = label;
                Icon = icon;
                Entry = entry;
            }

            public string Name { get; }
            public string Label { get; }
            public Icon Icon { get; }
            public AssetEntry? Entry { get; }
        }

        public IconPicker(string id) => _id = id;

        public bool IsOpen { get; private set; }

        // Opens the picker on the next OnGUI. current is the target's icon (null = default), defaultIcon the glyph shown
        // for the default, apply receives the choice (null = Reset to Default). position: OnGUI's draw space, else the
        // mouse cursor.
        public void Open(EditorIcon? current, Icon defaultIcon, Action<EditorIcon?> apply, Vector2? position = null)
        {
            _current = current;
            _default = defaultIcon;
            _apply = apply;
            _fixedPosition = position;
            _openRequested = true;
        }

        // Test hook: fills the search field once open and, after 30 frames, picks the first visible icon whose name or
        // label matches pick.
        public void Hook(string? search, string? pick)
        {
            _hookSearch = search;
            _hookPick = pick;
            _hookPickIn = pick is null ? -1 : 30;
        }

        public readonly record struct HookSpec(string Target, string? Search, string? Pick);

        // XG_UI_TEST_ICON_PICKER=hierarchy|inspector[:search=<text>][:pick=<icon name or path>].
        public static HookSpec? ParseHook()
        {
            if (Environment.GetEnvironmentVariable("XG_UI_TEST_ICON_PICKER") is not { Length: > 0 } spec)
                return null;
            string[] parts = spec.Split(':');
            string? search = null;
            string? pick = null;
            foreach (string part in parts.Skip(1))
            {
                if (part.StartsWith("search=", StringComparison.Ordinal))
                    search = part[7..];
                else if (part.StartsWith("pick=", StringComparison.Ordinal))
                    pick = part[5..];
            }
            return new HookSpec(parts[0], search, pick);
        }

        public void OnGUI()
        {
            if (_openRequested)
            {
                _openRequested = false;
                Vector2 mouse = Gui.MousePosition;
                _position = _fixedPosition ?? new Vector2(mouse.x + 4.0f, mouse.y + 4.0f);
                _filter.Text = _hookSearch ?? string.Empty;
                _hookSearch = null;
                _focusSearch = true;
                _hover = string.Empty;
                Gui.OpenPopup(_id);
            }
            using var popup = Gui.Popup(_id, _position, new Vector2(Width, 0.0f));
            IsOpen = popup.IsOpen;
            if (!popup.IsOpen || _apply is null)
                return;

            Gui.Space(4.0f);
            using (Gui.Horizontal(EditorStyle.BarGap))
            {
                Gui.Space(4.0f);
                if (_focusSearch)
                    Gui.FocusNext();
                _focusSearch = false;
                Gui.SearchField("##icon_search", _filter, 0.0f);
                Gui.Space(4.0f);
            }
            Gui.Space(2.0f);

            string filter = _filter.Text.Trim();
            List<Choice> builtIn = IconLibrary.BuiltIn
                .Select(i => new Choice(i.ToString(), i.ToString(), i, null))
                .Where(c => Matches(c, filter)).ToList();
            List<Choice> project = IconLibrary.Project
                .Select(e => new Choice(e.RelPath, e.DisplayName, Icon.Image, e))
                .Where(c => Matches(c, filter)).ToList();

            string hovered = string.Empty;
            EditorIcon? picked = null;
            bool pickedAny = false;
            using (var list = Gui.ScrollArea("##icon_list", new Vector2(0.0f, ListHeight)))
            {
                if (list.IsOpen)
                {
                    float width = MathF.Max(Gui.AvailableSize.x, 1.0f);
                    DrawSection("##builtin_header", width, Icon.Blocks, Style.Color(StyleColor.Icon), "Built-in", null, builtIn.Count);
                    if (DrawGrid("##builtin", width, builtIn, ref hovered) is { } b)
                        (picked, pickedAny) = (new EditorIcon(b.Name, _current?.Color), true);
                    Gui.Space(4.0f);
                    DrawSection("##project_header", width, Icon.Folder, Style.Color(StyleColor.IconWarning), "Project", "Assets/**/" + IconLibrary.FolderName, project.Count);
                    if (project.Count > 0)
                    {
                        if (DrawGrid("##project", width, project, ref hovered) is { } p)
                            (picked, pickedAny) = (new EditorIcon(p.Name, null), true);
                    }
                    else
                    {
                        DrawNote("##project_empty", width, ProjectService.Assets is null ? "No project is open."
                            : filter.Length > 0 ? "No project icons match." : "Add PNG or BMP files to a folder named Icons under Assets.");
                    }
                }
            }
            if (hovered.Length > 0)
                _hover = hovered;
            DrawHoverLine();
            if (DrawFooter() is { } footer)
                (picked, pickedAny) = footer;

            bool close = pickedAny && picked is { } chosen && !string.Equals(chosen.Name, _current?.Name, StringComparison.OrdinalIgnoreCase);
            if (!pickedAny && _hookPickIn >= 0 && _hookPickIn-- == 0 && _hookPick is { } hookName)
            {
                _hookPick = null;
                bool found = false;
                foreach (Choice choice in builtIn.Concat(project))
                    if (string.Equals(choice.Label, hookName, StringComparison.OrdinalIgnoreCase) || string.Equals(choice.Name, hookName, StringComparison.OrdinalIgnoreCase))
                    {
                        (picked, pickedAny, found) = (new EditorIcon(choice.Name, choice.Entry is null ? _current?.Color : null), true, true);
                        break;
                    }
                if (!found)
                    Debug.LogWarning($"[Debug] Icon picker hook: no icon named {hookName}");
            }

            if (!pickedAny)
                return;
            _current = picked;
            _apply(picked);
            if (close)
                Gui.ClosePopup();
        }

        private static bool Matches(Choice choice, string filter) =>
            filter.Length == 0 || choice.Label.Contains(filter, StringComparison.OrdinalIgnoreCase) || choice.Name.Contains(filter, StringComparison.OrdinalIgnoreCase);

        private static void DrawSection(string id, float width, Icon icon, Color32 iconColor, string title, string? detail, int count)
        {
            using var canvas = Gui.Canvas(id, new Vector2(width, SectionHeight));
            Painter draw = canvas.Draw;
            float cy = SectionHeight * 0.5f;
            draw.Icon(icon, new Vector2(10.0f, cy), 12.0f, iconColor);
            float x = 20.0f;
            using (Gui.Font(FontStyle.Bold))
            {
                draw.Text(new Vector2(x, MathF.Round(cy - Gui.MeasureText(title).y * 0.5f)), Style.Color(StyleColor.Text), title);
                x += Gui.MeasureText(title).x + 6.0f;
            }
            if (detail is not null)
                using (Gui.Font(FontStyle.Mono))
                {
                    draw.Text(new Vector2(x, MathF.Round(cy - Gui.MeasureText(detail).y * 0.5f)), Style.Color(StyleColor.TextDim), detail);
                    x += Gui.MeasureText(detail).x + 6.0f;
                }
            string label = count.ToString();
            Vector2 size = Gui.MeasureText(label);
            Rect chip = Rect.FromXYWH(x, cy - 8.0f, size.x + 12.0f, 16.0f);
            draw.RectFilled(chip, Style.Color(StyleColor.SegmentBg), 8.0f);
            draw.Text(new Vector2(chip.min.x + 6.0f, MathF.Round(cy - size.y * 0.5f)), Style.Color(StyleColor.TextDim), label);
        }

        private static void DrawNote(string id, float width, string text)
        {
            using var canvas = Gui.Canvas(id, new Vector2(width, 24.0f));
            canvas.Draw.Text(new Vector2(10.0f, 4.0f), Style.Color(StyleColor.TextDim), text);
        }

        // One section's icons in rows of Columns cells; returns the clicked one.
        private Choice? DrawGrid(string id, float width, List<Choice> choices, ref string hovered)
        {
            if (choices.Count == 0)
            {
                DrawNote(id + "_empty", width, "No icons match.");
                return null;
            }
            int rows = (choices.Count + Columns - 1) / Columns;
            float cellWidth = MathF.Floor((width - 6.0f) / Columns);
            using var canvas = Gui.Canvas(id, new Vector2(width, rows * CellHeight + 6.0f));
            Painter draw = canvas.Draw;
            draw.RectFilled(Rect.FromXYWH(0.0f, 0.0f, cellWidth * Columns + 6.0f, rows * CellHeight + 6.0f), Style.Color(StyleColor.AreaBg), 3.0f);
            Choice? clicked = null;
            for (int i = 0; i < choices.Count; i++)
            {
                Choice choice = choices[i];
                Rect cell = Rect.FromXYWH(3.0f + (i % Columns) * cellWidth, 3.0f + (i / Columns) * CellHeight, cellWidth, CellHeight);
                Rect inner = Rect.FromXYWH(cell.min.x + 1.0f, cell.min.y + 1.0f, cell.Width - 2.0f, cell.Height - 2.0f);
                Result hit;
                using (Gui.Id(i))
                    hit = Gui.HitArea("##cell", cell);
                bool selected = _current is { } current && string.Equals(current.Name, choice.Name, StringComparison.OrdinalIgnoreCase);
                if (selected)
                    draw.RectFilled(inner, Style.Color(StyleColor.Accent), 3.0f);
                else if (hit.Hovered())
                    draw.RectFilled(inner, Style.Color(StyleColor.ButtonHover), 3.0f);
                if (choice.Entry is not null)
                    ComponentIcons.Resolve(choice.Name, null, Icon.Image).Draw(draw, cell.Center, 20.0f);
                else
                {
                    Color32 color = selected ? Style.Color(StyleColor.TextOnAccent) : EditorIcons.ParseColor(_current?.Color) ?? Style.Color(StyleColor.Icon);
                    draw.Icon(choice.Icon, cell.Center, 16.0f, color);
                }
                if (hit.Hovered())
                {
                    hovered = choice.Entry?.RelPath ?? choice.Label;
                    Gui.Tooltip(hovered);
                }
                if (hit.Clicked())
                    clicked = choice;
            }
            return clicked;
        }

        private void DrawHoverLine()
        {
            using var canvas = Gui.Canvas("##icon_hover", new Vector2(0.0f, 20.0f));
            string text = _hover.Length > 0 ? _hover : "Current: " + EditorIcons.Label(_current) + (_current is null ? $" ({_default})" : string.Empty);
            canvas.Draw.Text(new Vector2(8.0f, 3.0f), Style.Color(StyleColor.TextDim), text);
        }

        // Tint swatches (the first one = no tint) and Reset to Default; returns a choice when one was made.
        private (EditorIcon?, bool)? DrawFooter()
        {
            (EditorIcon?, bool)? result = null;
            using (Gui.Horizontal(EditorStyle.BarGap))
            {
                Gui.Space(4.0f);
                Gui.Label("Tint");
                float width = _tints.Length * SwatchStep;
                using (var canvas = Gui.Canvas("##icon_tints", new Vector2(width, 22.0f)))
                {
                    Painter draw = canvas.Draw;
                    string tintTarget = _current?.Name ?? _default.ToString();
                    bool tintable = Enum.TryParse(tintTarget, true, out Icon _);
                    for (int i = 0; i < _tints.Length; i++)
                    {
                        Rect swatch = Rect.FromXYWH(2.0f + i * SwatchStep, 5.0f, SwatchSize, SwatchSize);
                        Result hit;
                        using (Gui.Id(i))
                            hit = Gui.HitArea("##tint", swatch);
                        Gui.Tooltip(_tints[i] is null ? "No tint (the default colour)" : "Tint " + _tints[i]);
                        bool on = string.Equals(_current?.Color, _tints[i], StringComparison.OrdinalIgnoreCase);
                        if (EditorIcons.ParseColor(_tints[i]) is { } fill)
                            draw.RectFilled(swatch, fill, 2.0f);
                        else
                        {
                            draw.Rect(swatch, Style.Color(StyleColor.TextDim), 2.0f);
                            draw.Line(new Vector2(swatch.min.x + 2.0f, swatch.max.y - 2.0f), new Vector2(swatch.max.x - 2.0f, swatch.min.y + 2.0f), Style.Color(StyleColor.TextDim));
                        }
                        if (on || hit.Hovered())
                            draw.Rect(Rect.FromXYWH(swatch.min.x - 2.0f, swatch.min.y - 2.0f, SwatchSize + 4.0f, SwatchSize + 4.0f), Style.Color(on ? StyleColor.Accent : StyleColor.Text), 3.0f);
                        if (hit.Clicked() && tintable)
                            result = (_tints[i] is null && _current is null ? null : new EditorIcon(tintTarget, _tints[i]), true);
                    }
                }
                Gui.FlexibleSpace();
                if (Gui.Button("Reset to Default"))
                {
                    result = (null, true);
                    Gui.ClosePopup();
                }
                Gui.Space(4.0f);
            }
            Gui.Space(4.0f);
            return result;
        }
    }
}
