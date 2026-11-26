// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // A project folder field: a picker row showing the folder ("Assets/Scenes"); clicking opens a popup with a filter,
    // a New folder button, the folder tree of Assets/ (expand arrows, the chosen folder marked with the selection bar)
    // and a footer with the path and Choose. New folder adds an inline name row under the selected folder; Enter
    // creates it through AssetDatabase.CreateFolder and selects it. One instance per drawn field (it keeps the popup
    // state).
    public sealed class FolderField
    {
        private const float PopupWidth = 260.0f;
        private const float RowHeight = 22.0f;
        private const float Indent = 14.0f;
        private const float ListHeight = 220.0f;

        private readonly TextBuffer _filter = new(64);
        private readonly TextBuffer _newName = new(128);
        private readonly HashSet<string> _expanded = new(StringComparer.OrdinalIgnoreCase);
        private string _selected = AssetIndex.RootFolder;
        private Vector2 _position;
        private bool _open;
        private bool _creating;
        private bool _focusName;
        private string? _error;
        private int _openDelay = -1;
        private bool _startCreating;
        private bool _createRequested;
        private int _confirmIn = -1;

        // True while the popup is open (hosts keep Enter and Escape for it).
        public bool IsOpen => _open;

        // Opens the popup after a number of frames, optionally with the new folder row, and optionally confirms the
        // row 20 frames later as Enter would (test hooks).
        public void OpenLater(int frames, bool newFolder, bool confirm = false)
        {
            _openDelay = frames;
            _startCreating = newFolder;
            _confirmIn = confirm ? 20 : -1;
        }

        // folder: project-relative ("Assets/..."). Returns true when the user chose another folder.
        public bool Field(string id, string label, ref string folder)
        {
            bool changed = false;
            using (Gui.Id(id))
            {
                Result row = Gui.PickerField(label, folder, Icon.Folder, Style.Color(StyleColor.IconObject), null, null, _open);
                Gui.Tooltip("Click to choose a folder or create one");
                if ((row & Result.Clicked) != 0 || _openDelay == 0)
                {
                    Rect item = Gui.ItemRect;
                    _position = new Vector2(MathF.Max(item.max.x - PopupWidth, item.min.x), item.max.y + 2.0f);
                    Begin(folder);
                    Gui.OpenPopup("folder_field");
                }
                if (_openDelay >= 0)
                    _openDelay--;
                changed = DrawPopup(ref folder);
            }
            return changed;
        }

        private void Begin(string folder)
        {
            _selected = AssetIndex.Normalize(folder.Length > 0 ? folder : AssetIndex.RootFolder);
            _filter.Text = string.Empty;
            _creating = _startCreating;
            _focusName = _startCreating;
            _startCreating = false;
            _newName.Text = "New Folder";
            _error = null;
            _expanded.Add(AssetIndex.RootFolder);
            for (string path = _selected; path.Contains('/'); path = path[..path.LastIndexOf('/')])
                _expanded.Add(path);
            _expanded.Add(_selected);
        }

        private bool DrawPopup(ref string folder)
        {
            using var popup = Gui.Popup("folder_field", _position, new Vector2(PopupWidth, 0.0f));
            _open = popup.IsOpen;
            if (!popup.IsOpen)
                return false;
            if (ProjectService.Assets is not { } assets)
            {
                Gui.Label(" No project is open.");
                return false;
            }
            bool changed = false;
            Gui.Space(4.0f);
            using (Gui.Horizontal(EditorStyle.BarGap))
            {
                Gui.Space(2.0f);
                Gui.SearchField("##folder_filter", _filter, 0.0f);
                if (Gui.IconButton("##folder_new", Icon.Plus, 14.0f, Style.Color(StyleColor.Text)))
                {
                    _creating = true;
                    _focusName = true;
                    _newName.Text = "New Folder";
                    _error = null;
                    _expanded.Add(_selected);
                }
                Gui.Tooltip("New folder in " + AssetNames.DisplayName(_selected, true));
                Gui.Space(2.0f);
            }

            List<string> directories = assets.Index.Directories.ToList();
            string filter = _filter.Text.Trim();
            using (var list = Gui.ScrollArea("##folder_list", new Vector2(PopupWidth, ListHeight)))
            {
                if (list.IsOpen)
                {
                    float width = MathF.Max(Gui.AvailableSize.x, 1.0f);
                    foreach (string path in directories)
                    {
                        if (filter.Length > 0 ? !AssetNames.DisplayName(path, true).Contains(filter, StringComparison.OrdinalIgnoreCase) : !Visible(path))
                            continue;
                        int depth = filter.Length > 0 ? 0 : path.Count(c => c == '/');
                        bool hasChildren = directories.Any(d => d.StartsWith(path + "/", StringComparison.OrdinalIgnoreCase));
                        if (DrawRow(path, depth, hasChildren, width))
                        {
                            folder = _selected;
                            changed = true;
                            Gui.ClosePopup();
                        }
                        if (_creating && string.Equals(path, _selected, StringComparison.OrdinalIgnoreCase))
                            DrawNewFolder(depth + 1);
                    }
                }
            }
            if (_createRequested)
                CreateFolder(assets);
            if (_error is not null)
            {
                using var error = Gui.Canvas("##folder_error", new Vector2(PopupWidth, 18.0f));
                error.Draw.Text(new Vector2(6.0f, 2.0f), Style.Color(StyleColor.IconWarning), _error);
            }
            using (Gui.Horizontal(EditorStyle.BarGap))
            {
                Gui.Space(4.0f);
                using (Gui.Font(FontStyle.Mono))
                    Gui.Label(_selected);
                Gui.FlexibleSpace();
                if (Gui.Button("Choose"))
                {
                    changed = !string.Equals(folder, _selected, StringComparison.OrdinalIgnoreCase);
                    folder = _selected;
                    Gui.ClosePopup();
                }
                Gui.Space(2.0f);
            }
            Gui.Space(4.0f);
            return changed;
        }

        private bool Visible(string path)
        {
            for (string parent = path; parent.Contains('/');)
            {
                parent = parent[..parent.LastIndexOf('/')];
                if (!_expanded.Contains(parent))
                    return false;
            }
            return true;
        }

        // One folder row; returns true when it was double-clicked (choose it).
        private bool DrawRow(string path, int depth, bool hasChildren, float width)
        {
            using var id = Gui.Id(path);
            using var canvas = Gui.Canvas("##folder_row", new Vector2(width, RowHeight));
            Painter draw = canvas.Draw;
            Rect row = Rect.FromXYWH(0.0f, 0.0f, width, RowHeight);
            float x = 6.0f + depth * Indent;
            Result arrowHit = hasChildren ? Gui.HitArea("##arrow", Rect.FromXYWH(x - 2.0f, 2.0f, 16.0f, RowHeight - 4.0f)) : Result.None;
            Result hit = Gui.HitArea("##row", Rect.FromXYWH(x + 14.0f, 0.0f, width - x - 14.0f, RowHeight));
            Gui.Tooltip(path);
            bool selected = string.Equals(path, _selected, StringComparison.OrdinalIgnoreCase);
            bool expanded = _expanded.Contains(path);
            if (selected)
            {
                draw.RectFilled(row, new Color32(47, 75, 112, 255));
                EditorStyle.SelectionBar(draw, 0.0f, RowHeight);
            }
            else if (hit.Hovered())
            {
                draw.RectFilled(row, Style.Color(StyleColor.ButtonHover));
            }
            float cy = RowHeight * 0.5f;
            if (hasChildren)
            {
                Color32 arrow = Style.Color(arrowHit.Hovered() ? StyleColor.Text : StyleColor.TextDim);
                if (expanded)
                    draw.TriangleFilled(new Vector2(x + 2.0f, cy - 2.5f), new Vector2(x + 10.0f, cy - 2.5f), new Vector2(x + 6.0f, cy + 2.5f), arrow);
                else
                    draw.TriangleFilled(new Vector2(x + 3.5f, cy - 4.0f), new Vector2(x + 8.5f, cy), new Vector2(x + 3.5f, cy + 4.0f), arrow);
            }
            draw.Icon(Icon.Folder, new Vector2(x + 22.0f, cy), 13.0f, Style.Color(StyleColor.IconObject));
            string name = AssetNames.DisplayName(path, true);
            draw.Text(new Vector2(x + 33.0f, MathF.Round(cy - Gui.MeasureText(name).y * 0.5f)), Style.Color(selected ? StyleColor.TextSelected : StyleColor.Text), name);
            if (arrowHit.Clicked() && !_expanded.Remove(path))
                _expanded.Add(path);
            if (hit.Clicked())
            {
                _selected = path;
                _creating = false;
                _error = null;
            }
            return (hit & Result.DoubleClicked) != 0;
        }

        private void DrawNewFolder(int depth)
        {
            using (Gui.Horizontal(4.0f))
            {
                Gui.Space(6.0f + depth * Indent + 14.0f);
                using (var icon = Gui.Canvas("##new_icon", new Vector2(16.0f, RowHeight)))
                    icon.Draw.Icon(Icon.Plus, new Vector2(8.0f, RowHeight * 0.5f), 12.0f, new Color32(143, 209, 158, 255));
                if (_focusName)
                    Gui.FocusNext();
                TextFieldFlags flags = _focusName ? TextFieldFlags.SetText : TextFieldFlags.None;
                _focusName = false;
                if ((Gui.TextField("##new_folder", _newName, "Folder name", 0.0f, flags) & Result.Changed) != 0)
                    _error = AssetDatabase.ValidateName(_newName.Text.Trim());
                Gui.Space(4.0f);
            }
            if (Gui.KeyPressed(Key.Escape, false))
            {
                _creating = false;
                _error = null;
            }
            else if (Gui.KeyPressed(Key.Enter, false) || (_confirmIn >= 0 && _confirmIn-- == 0))
            {
                _createRequested = true;
            }
        }

        // Runs after the tree is drawn: creating the folder refreshes the asset index, which rebuilds its folder list.
        private void CreateFolder(AssetDatabase assets)
        {
            _createRequested = false;
            if (assets.CreateFolder(_selected, _newName.Text.Trim(), out string created) is string failed)
            {
                _error = failed;
                return;
            }
            _selected = created;
            _expanded.Add(created);
            _creating = false;
            _error = null;
        }
    }
}
