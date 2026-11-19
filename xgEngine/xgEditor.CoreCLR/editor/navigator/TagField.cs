// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    public enum TagFieldMode
    {
        // A tag filter (TagQuery): "+" chips, "@" attribute chips and the match tooltip.
        Query,
        // The user tags of one asset, written to its meta file on every change, with its read-only import tags on demand.
        Asset,
    }

    // The standard tag control: chips that wrap, plus an autocomplete field. Enter, Tab or a clicked suggestion adds a
    // chip, the chip's x removes it, and Backspace in the empty field removes the last one.
    //
    // Query mode: plain chips (Accent) must all match, "+" chips (teal) match on their own, and "@group:value" chips are
    // attribute tags (@type, @ext and import tags such as @texture:bc1). Typing "@" completes attribute tags, other text
    // completes user tags, and hovering the chips shows the match logic.
    //
    // Asset mode: the chips are the asset's user tags. A toggle at the end of the field shows its read-only import tags
    // first, without an x.
    public sealed class TagField
    {
        private const float ChipHeight = 18.0f;
        private const float ChipGap = 4.0f;
        private const float RowGap = 4.0f;
        private const float MinFieldWidth = 90.0f;
        private const float ToggleWidth = 26.0f;
        private static readonly Color32 _orColor = new(47, 125, 114, 255);
        private static readonly Color32 _orText = new(226, 245, 241, 255);

        private readonly AutoCompleteField _field;
        private AssetTags? _tags;
        private AssetIndex? _index;
        private string? _path;
        private string? _pendingType;
        private int _pendingTypeFrames;

        public TagField(string id, TagFieldMode mode = TagFieldMode.Query)
        {
            Mode = mode;
            _field = new AutoCompleteField(id + "_field", 256);
            _field.Source = Suggest;
        }

        public TagFieldMode Mode { get; }

        // Query mode: the current filter.
        public TagQuery Query { get; private set; } = new();

        // Asset mode: shows the read-only import tags before the user tags.
        public bool ShowImportTags { get; set; }

        public void SetQuery(string text) => Query = TagQuery.Parse(text);

        public void Focus() => _field.Focus();

        public void Reset(bool focus) => _field.SetText(string.Empty, focus);

        // Types text into the field and opens its suggestions once the field has been on screen for delayFrames frames
        // (test hook).
        public void Type(string text, int delayFrames = 0)
        {
            _pendingType = text;
            _pendingTypeFrames = delayFrames;
        }

        public void AddChip(string tag, bool or)
        {
            tag = AssetTags.NormalizeTag(tag);
            if (tag.Length > 0 && !Query.Chips.Any(c => c.Tag == tag))
                Query.Chips.Add(new TagChip(tag, or));
        }

        // Query mode. Returns true when the chips changed.
        public bool OnGUI(AssetTags tags, AssetIndex index, string matchSummary)
        {
            _tags = tags;
            _index = index;
            _path = null;
            string tooltip = Query.IsEmpty ? "Type a tag; +tag adds an OR tag, @ shows type, extension and import tags" : Query.Describe() + "\n" + matchSummary;
            string? hint = Query.IsEmpty ? "Add tag (+tag = or, @ = type)" : null;
            Edit edit = Draw(Query.Chips, tooltip, hint, false);

            bool changed = false;
            if (edit.Commit is not null)
            {
                string tag = AssetTags.NormalizeTag(edit.Commit.TrimStart('+'));
                if (tag.Length > 0 && !Query.Chips.Any(c => c.Tag == tag))
                {
                    Query.Chips.Add(new TagChip(tag, edit.Or));
                    changed = true;
                }
            }
            else if (edit.BackspaceOnEmpty && Query.Chips.Count > 0)
            {
                Query.Chips.RemoveAt(Query.Chips.Count - 1);
                changed = true;
            }
            if (edit.RemoveAt >= 0 && edit.RemoveAt < Query.Chips.Count)
            {
                Query.Chips.RemoveAt(edit.RemoveAt);
                changed = true;
            }
            return changed;
        }

        // Asset mode: edits the user tags of the asset at path.
        public void OnAssetGUI(AssetTags tags, AssetIndex? index, string path)
        {
            _tags = tags;
            _index = index;
            _path = path;
            IReadOnlyList<string> user = tags.Get(path);
            IReadOnlyList<string> imported = tags.ImportTags(path);
            List<TagChip> chips = new();
            if (ShowImportTags)
                chips.AddRange(imported.Select(t => new TagChip(t, false, true)));
            int firstUser = chips.Count;
            chips.AddRange(user.Select(t => new TagChip(t, false)));

            Edit edit = Draw(chips, null, user.Count == 0 ? "Add a tag" : null, imported.Count > 0);
            if (edit.Commit is not null)
                tags.Add(path, edit.Commit.TrimStart('+'));
            else if (edit.BackspaceOnEmpty && user.Count > 0)
                tags.Remove(path, user[^1]);
            if (edit.RemoveAt >= firstUser && edit.RemoveAt < chips.Count)
                tags.Remove(path, chips[edit.RemoveAt].Tag);
        }

        private readonly record struct Edit(int RemoveAt, string? Commit, bool Or, bool BackspaceOnEmpty);

        private Edit Draw(IReadOnlyList<TagChip> source, string? tooltip, string? hint, bool importToggle)
        {
            float width = Math.Max(Gui.AvailableSize.x - 8.0f, 60.0f);
            float lastRowWidth = width - (importToggle ? ToggleWidth : 0.0f);
            List<(TagChip Chip, float Width)> chips = source.Select(c => (c, ChipWidth(c))).ToList();

            List<List<int>> lines = new() { new List<int>() };
            float x = 0.0f;
            for (int i = 0; i < chips.Count; i++)
            {
                float w = chips[i].Width;
                if (x > 0.0f && x + w > width)
                {
                    lines.Add(new List<int>());
                    x = 0.0f;
                }
                lines[^1].Add(i);
                x += w + ChipGap;
            }
            if (x > 0.0f && lastRowWidth - x < MinFieldWidth)
                lines.Add(new List<int>());

            int removeAt = -1;
            if (lines.Count > 1)
            {
                using var canvas = Gui.Canvas("##chips_full", new Vector2(width, (lines.Count - 1) * (ChipHeight + RowGap)));
                Painter draw = canvas.Draw;
                if (tooltip is not null)
                {
                    Gui.HitArea("##chips_bg", new Rect(new Vector2(0.0f, 0.0f), canvas.Size));
                    Gui.Tooltip(tooltip);
                }
                for (int line = 0; line < lines.Count - 1; line++)
                    removeAt = DrawLine(draw, chips, lines[line], line * (ChipHeight + RowGap), removeAt);
            }

            string? commit = null;
            bool or = false;
            bool backspace = false;
            using (Gui.Horizontal(ChipGap))
            {
                List<int> last = lines[^1];
                if (last.Count > 0)
                {
                    float lastWidth = last.Sum(i => chips[i].Width + ChipGap) - ChipGap;
                    using var canvas = Gui.Canvas("##chips_last", new Vector2(lastWidth, ChipHeight + 4.0f));
                    Painter draw = canvas.Draw;
                    if (tooltip is not null)
                    {
                        Gui.HitArea("##chips_bg_last", new Rect(new Vector2(0.0f, 0.0f), canvas.Size));
                        Gui.Tooltip(tooltip);
                    }
                    removeAt = DrawLine(draw, chips, last, 2.0f, removeAt);
                }
                AutoCompleteEvent ev = _field.OnGUI(hint, 0.0f, out string value);
                if (_pendingType is not null && Gui.ItemRect.Height > 0.0f && --_pendingTypeFrames <= 0)
                {
                    _field.SetText(_pendingType, true);
                    _field.ShowList(Suggest(_pendingType));
                    _pendingType = null;
                }
                if (ev == AutoCompleteEvent.Commit)
                {
                    or = Mode == TagFieldMode.Query && (_field.Text.Text.TrimStart().StartsWith('+') || value.StartsWith('+'));
                    commit = value;
                    _field.SetText(string.Empty, true);
                }
                else if (ev == AutoCompleteEvent.BackspaceOnEmpty)
                {
                    backspace = true;
                }
                if (importToggle)
                {
                    bool show = ShowImportTags;
                    if (Gui.IconToggle("##import_tags", show ? Icon.Tag : Icon.EyeOff, ref show, Style.Color(show ? StyleColor.Accent : StyleColor.TextDim)))
                        ShowImportTags = show;
                    Gui.Tooltip(show ? "Hide import tags" : "Show import tags (read-only, from the last import)");
                }
            }
            return new Edit(removeAt, commit, or, backspace);
        }

        private static int DrawLine(Painter draw, List<(TagChip Chip, float Width)> chips, List<int> line, float y, int removeAt)
        {
            float x = 0.0f;
            foreach (int i in line)
            {
                (TagChip chip, float w) = chips[i];
                if (DrawChip(draw, i, chip, Rect.FromXYWH(x, y, w, ChipHeight)))
                    removeAt = i;
                x += w + ChipGap;
            }
            return removeAt;
        }

        private static float ChipWidth(TagChip chip) =>
            MathF.Ceiling(Gui.MeasureText(Label(chip)).x + 8.0f + (chip.ReadOnly ? 8.0f : 16.0f) + (chip.IsAttribute ? 3.0f : 0.0f));

        private static string Label(TagChip chip) => chip.Or ? "+ " + chip.Tag : chip.Tag;

        private static bool DrawChip(Painter draw, int index, TagChip chip, Rect rect)
        {
            const float radius = ChipHeight * 0.5f;
            Color32 text;
            float textX = rect.min.x + 8.0f;
            if (chip.IsAttribute)
            {
                Color32 edge = chip.Tag.StartsWith("@type:") ? Style.Color(StyleColor.IconData) : Style.Color(StyleColor.IconComponent);
                draw.RectFilled(rect, edge, radius);
                draw.RectFilled(new Rect(new Vector2(rect.min.x + 3.0f, rect.min.y), rect.max), Style.Color(StyleColor.PanelHeaderBg), radius);
                draw.Rect(new Rect(new Vector2(rect.min.x + 3.0f, rect.min.y), rect.max), Style.Color(StyleColor.Border), radius);
                text = Style.Color(chip.ReadOnly ? StyleColor.TextDim : StyleColor.Text);
                textX += 3.0f;
            }
            else if (chip.Or)
            {
                draw.RectFilled(rect, _orColor, radius);
                text = _orText;
            }
            else
            {
                draw.RectFilled(rect, Style.Color(StyleColor.Accent), radius);
                text = Style.Color(StyleColor.TextOnAccent);
            }
            float lineHeight = Gui.MeasureText("A").y;
            draw.Text(new Vector2(textX, MathF.Round(rect.min.y + (ChipHeight - lineHeight) * 0.5f)), text, Label(chip));
            if (chip.ReadOnly)
                return false;

            Rect close = Rect.FromXYWH(rect.max.x - 15.0f, rect.min.y + 2.0f, 13.0f, ChipHeight - 4.0f);
            Result hit;
            using (Gui.Id(index))
                hit = Gui.HitArea("##chip_close", close);
            Gui.Tooltip("Remove " + chip.Tag);
            Vector2 c = close.Center;
            Color32 cross = text;
            if (!hit.Hovered())
                cross.a = 170;
            else
                draw.CircleFilled(c, 6.0f, new Color32(255, 255, 255, 40));
            draw.Line(new Vector2(c.x - 3.0f, c.y - 3.0f), new Vector2(c.x + 3.0f, c.y + 3.0f), cross, 1.2f);
            draw.Line(new Vector2(c.x + 3.0f, c.y - 3.0f), new Vector2(c.x - 3.0f, c.y + 3.0f), cross, 1.2f);
            return hit.Clicked();
        }

        private IEnumerable<Suggestion> Suggest(string text)
        {
            if (_tags is null)
                yield break;
            string typed = text.Trim();
            bool or = typed.StartsWith('+');
            string query = AssetTags.NormalizeTag(or ? typed[1..] : typed);
            if (query.Length == 0)
                yield break;
            bool attribute = query.StartsWith('@');
            HashSet<string> used;
            if (Mode == TagFieldMode.Asset)
            {
                if (attribute || or || _path is null)
                    yield break;
                used = _tags.Get(_path).ToHashSet();
            }
            else
            {
                if (attribute && _index is null)
                    yield break;
                used = Query.Chips.Select(c => c.Tag).ToHashSet();
            }
            IReadOnlyList<TagCount> source = attribute ? _tags.AttributeTags(_index!) : _tags.UserTags();
            List<TagCount> prefix = new();
            List<TagCount> contains = new();
            foreach (TagCount tag in source)
            {
                if (used.Contains(tag.Tag))
                    continue;
                if (tag.Tag.StartsWith(query, StringComparison.OrdinalIgnoreCase))
                    prefix.Add(tag);
                else if (!attribute && tag.Tag.Contains(query, StringComparison.OrdinalIgnoreCase))
                    contains.Add(tag);
                else if (attribute && query.Length > 1 && tag.Tag[(tag.Tag.IndexOf(':') + 1)..].Contains(query[1..], StringComparison.OrdinalIgnoreCase))
                    contains.Add(tag);
            }
            foreach (TagCount tag in prefix.Concat(contains))
                yield return new Suggestion(tag.Tag, tag.Count.ToString(), IconFor(tag.Tag), ColorFor(tag.Tag));
        }

        private static Icon IconFor(string tag)
        {
            if (tag.StartsWith("@type:") && AssetTags.TryParseType(tag[6..], out AssetKind kind))
                return AssetIndex.IconOf(kind);
            if (tag.StartsWith("@ext:"))
                return Icon.File;
            if (tag.StartsWith("@texture:"))
                return Icon.Image;
            return Icon.Tag;
        }

        private static Color32 ColorFor(string tag)
        {
            if (tag.StartsWith("@type:") && AssetTags.TryParseType(tag[6..], out AssetKind kind))
                return AssetIndex.IconColorOf(kind);
            return tag.StartsWith('@') ? Style.Color(StyleColor.Icon) : Style.Color(StyleColor.IconComponent);
        }
    }
}
