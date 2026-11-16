// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    public readonly record struct Suggestion(string Value, string? Detail, Icon Icon, Color32 IconColor);

    public enum AutoCompleteEvent
    {
        None,
        // Enter, Tab or a click picked a value (the selected suggestion, or the typed text on Enter).
        Commit,
        // Backspace in a field that was already empty (remove the previous chip).
        BackspaceOnEmpty,
    }

    // A text field with a suggestion list under it while typing. Up/Down move through the list, Tab or Enter commit the
    // selected suggestion (Enter commits the typed text when nothing is selected), Escape closes the list. The list is
    // an overlay, so the field keeps editing while it shows.
    public sealed class AutoCompleteField
    {
        private const float RowHeight = 20.0f;
        private const int MaxRows = 16;

        private readonly string _id;
        private readonly List<Suggestion> _items = new();
        private ListState _list;
        private int _selected = -1;
        private bool _open;
        private bool _overlayHovered;
        private bool _wasActive;
        private bool _wasEmpty = true;
        private bool _focus;
        private int _focusFrames;
        private int _setTextFrames;
        private Rect _fieldRect;

        public AutoCompleteField(string id, int capacity = 512)
        {
            _id = id;
            Text = new TextBuffer(capacity);
        }

        public TextBuffer Text { get; }

        // Suggestions for the typed text.
        public Func<string, IEnumerable<Suggestion>>? Source { get; set; }

        public bool Active { get; private set; }
        public bool IsOpen => _open && _items.Count > 0;

        public void SetText(string text, bool focus)
        {
            Text.Text = text;
            _setTextFrames = 2;
            _wasEmpty = text.Length == 0;
            if (focus)
                _focus = true;
        }

        public void Focus() => _focus = true;

        // Opens the list with these items regardless of the text (a dropdown of recent entries).
        public void ShowList(IEnumerable<Suggestion> items)
        {
            _items.Clear();
            _items.AddRange(items);
            _selected = _items.Count > 0 ? 0 : -1;
            _open = _items.Count > 0;
            _focus = true;
            _setTextFrames = 2;
        }

        public void Close() => _open = false;

        public AutoCompleteEvent OnGUI(string? hint, float width, out string value)
        {
            value = string.Empty;
            if (_focus)
            {
                Gui.FocusNext();
                _focus = false;
                _focusFrames = 3;
                _setTextFrames = Math.Max(_setTextFrames, 2);
            }
            else if (_focusFrames > 0)
            {
                _focusFrames--;
            }

            TextFieldFlags flags = TextFieldFlags.CaptureNavKeys | (_setTextFrames > 0 ? TextFieldFlags.SetText : TextFieldFlags.None);
            bool settingText = _setTextFrames > 0;
            Result result = Gui.TextField(_id, Text, hint, width, flags);
            if (_setTextFrames > 0)
                _setTextFrames--;
            _fieldRect = Gui.ItemRect;
            Active = (result & Result.Active) != 0;
            bool keys = Active || _wasActive;

            if ((result & Result.Changed) != 0 && !settingText)
            {
                Requery();
                _open = true;
            }

            AutoCompleteEvent ev = AutoCompleteEvent.None;
            if (keys)
            {
                if (Gui.KeyPressed(Key.Down))
                {
                    if (!_open)
                    {
                        Requery();
                        _open = true;
                    }
                    else if (_items.Count > 0)
                        _selected = Math.Min(_selected + 1, _items.Count - 1);
                }
                else if (Gui.KeyPressed(Key.Up) && _open && _items.Count > 0)
                {
                    _selected = Math.Max(_selected - 1, 0);
                }
                else if (Gui.KeyPressed(Key.Tab, false))
                {
                    if (IsOpen)
                    {
                        value = _items[Math.Max(_selected, 0)].Value;
                        ev = AutoCompleteEvent.Commit;
                    }
                    _focus = true;
                }
                else if (Gui.KeyPressed(Key.Enter, false))
                {
                    value = IsOpen && _selected >= 0 ? _items[_selected].Value : Text.Text.Trim();
                    ev = AutoCompleteEvent.Commit;
                }
                else if (Gui.KeyPressed(Key.Escape, false))
                {
                    _open = false;
                }
                else if (Gui.KeyPressed(Key.Backspace) && Text.IsEmpty && _wasEmpty)
                {
                    ev = AutoCompleteEvent.BackspaceOnEmpty;
                    _focus = true;
                }
            }

            if (!Active && !_wasActive && !_overlayHovered && !_focus && _focusFrames == 0)
                _open = false;
            _wasActive = Active;
            _wasEmpty = Text.IsEmpty;

            if (ev == AutoCompleteEvent.Commit)
            {
                _open = false;
                return value.Length > 0 ? ev : AutoCompleteEvent.None;
            }

            if (DrawList(out string clicked))
            {
                value = clicked;
                _open = false;
                return AutoCompleteEvent.Commit;
            }
            return ev;
        }

        private void Requery()
        {
            _items.Clear();
            if (Source is not null)
                _items.AddRange(Source(Text.Text));
            _selected = _items.Count > 0 ? 0 : -1;
        }

        private bool DrawList(out string clicked)
        {
            clicked = string.Empty;
            _overlayHovered = false;
            if (!IsOpen)
                return false;

            int rows = Math.Min(_items.Count, MaxRows);
            Vector2 size = new(Math.Max(_fieldRect.Width, 160.0f), rows * RowHeight + 2.0f);
            using var overlay = Gui.Overlay(_id + "_list", new Vector2(_fieldRect.min.x, _fieldRect.max.y + 1.0f), size);
            if (!overlay.IsOpen)
                return false;

            _list.Count = _items.Count;
            _list.RowHeight = RowHeight;
            _list.Flags = ListFlags.None;
            using ListScope list = Gui.List(_id + "_rows", ref _list, size);
            if (!list.IsOpen)
                return false;

            Color32 text = Style.Color(StyleColor.Text);
            Color32 onAccent = Style.Color(StyleColor.TextOnAccent);
            Color32 dim = Style.Color(StyleColor.TextDim);
            float textY = MathF.Round((RowHeight - Gui.MeasureText("A").y) * 0.5f);
            bool picked = false;
            for (int i = _list.First; i < _list.Last && i < _items.Count; i++)
            {
                Suggestion item = _items[i];
                bool selected = i == _selected;
                using ListRowScope row = Gui.ListRow(i, false);
                if ((row.Interaction & Result.Hovered) != 0)
                {
                    _overlayHovered = true;
                    _selected = i;
                    selected = true;
                }
                if (row.Clicked && !picked)
                {
                    clicked = item.Value;
                    picked = true;
                }
                Painter draw = row.Draw;
                float rowWidth = size.x;
                if (selected)
                    draw.RectFilled(Rect.FromXYWH(0.0f, 0.0f, rowWidth, RowHeight), Style.Color(StyleColor.Accent));
                float x = 6.0f;
                if (item.Icon != Icon.None)
                {
                    draw.Icon(item.Icon, new Vector2(x + 7.0f, RowHeight * 0.5f), 13.0f, selected ? onAccent : item.IconColor);
                    x += 20.0f;
                }
                draw.Text(new Vector2(x, textY), selected ? onAccent : text, item.Value);
                if (!string.IsNullOrEmpty(item.Detail))
                {
                    float w = Gui.MeasureText(item.Detail).x;
                    draw.Text(new Vector2(rowWidth - w - 14.0f, textY), selected ? onAccent : dim, item.Detail);
                }
            }
            return picked;
        }
    }
}
