// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // Tag chips plus an autocomplete field. Plain chips (Accent) must all match, "+" chips (teal) match on their own,
    // and "@group:value" chips are built-in attribute tags (@type, @ext). Typing "@" completes attribute tags, other
    // text completes user tags; a leading "+" makes the chip an OR chip. Backspace in the empty field removes the last
    // chip. Hovering the chips shows the match logic.
    public sealed class TagInput
    {
        private const float ChipHeight = 18.0f;
        private const float ChipGap = 4.0f;
        private const float RowGap = 4.0f;
        private const float MinFieldWidth = 90.0f;
        private static readonly Color32 _orColor = new(47, 125, 114, 255);
        private static readonly Color32 _orText = new(226, 245, 241, 255);

        private readonly AutoCompleteField _field;
        private AssetTags? _tags;
        private AssetIndex? _index;

        public TagInput(string id)
        {
            _field = new AutoCompleteField(id + "_field", 256);
            _field.Source = Suggest;
        }

        public TagQuery Query { get; private set; } = new();

        public void SetQuery(string text) => Query = TagQuery.Parse(text);

        public void Focus() => _field.Focus();

        // Types text into the field and opens its suggestions (test hook).
        public void Type(string text) => _pendingType = text;

        private string? _pendingType;

        // Returns true when the chips changed.
        public bool OnGUI(AssetTags tags, AssetIndex index, string matchSummary)
        {
            _tags = tags;
            _index = index;
            bool changed = false;
            float width = Math.Max(Gui.AvailableSize.x - 8.0f, 60.0f);
            List<(TagChip Chip, float Width)> chips = Query.Chips.Select(c => (c, ChipWidth(c))).ToList();

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
            bool fieldOwnLine = x > 0.0f && width - x < MinFieldWidth;
            if (fieldOwnLine)
                lines.Add(new List<int>());

            string tooltip = Query.IsEmpty ? "Type a tag; +tag adds an OR tag, @ shows type and extension tags" : Query.Describe() + "\n" + matchSummary;
            int removeAt = -1;
            if (lines.Count > 1)
            {
                using var canvas = Gui.Canvas("##chips_full", new Vector2(width, (lines.Count - 1) * (ChipHeight + RowGap)));
                Painter draw = canvas.Draw;
                Gui.HitArea("##chips_bg", new Rect(new Vector2(0.0f, 0.0f), canvas.Size));
                Gui.Tooltip(tooltip);
                for (int line = 0; line < lines.Count - 1; line++)
                    removeAt = DrawLine(draw, chips, lines[line], line * (ChipHeight + RowGap), removeAt);
            }

            using (Gui.Horizontal(ChipGap))
            {
                List<int> last = lines[^1];
                if (last.Count > 0)
                {
                    float lastWidth = last.Sum(i => chips[i].Width + ChipGap) - ChipGap;
                    using var canvas = Gui.Canvas("##chips_last", new Vector2(lastWidth, ChipHeight + 4.0f));
                    Painter draw = canvas.Draw;
                    Gui.HitArea("##chips_bg_last", new Rect(new Vector2(0.0f, 0.0f), canvas.Size));
                    Gui.Tooltip(tooltip);
                    removeAt = DrawLine(draw, chips, last, 2.0f, removeAt);
                }
                if (_pendingType is not null)
                {
                    _field.SetText(_pendingType, true);
                    _field.ShowList(Suggest(_pendingType));
                    _pendingType = null;
                }
                string? hint = Query.IsEmpty ? "Add tag (+tag = or, @ = type)" : null;
                AutoCompleteEvent ev = _field.OnGUI(hint, 0.0f, out string value);
                if (ev == AutoCompleteEvent.Commit)
                {
                    bool or = _field.Text.Text.TrimStart().StartsWith('+') || value.StartsWith('+');
                    string tag = AssetTags.NormalizeTag(value.TrimStart('+'));
                    if (tag.Length > 0 && !Query.Chips.Any(c => c.Tag == tag))
                    {
                        Query.Chips.Add(new TagChip(tag, or));
                        changed = true;
                    }
                    _field.SetText(string.Empty, true);
                }
                else if (ev == AutoCompleteEvent.BackspaceOnEmpty && Query.Chips.Count > 0)
                {
                    Query.Chips.RemoveAt(Query.Chips.Count - 1);
                    changed = true;
                }
            }

            if (removeAt >= 0 && removeAt < Query.Chips.Count)
            {
                Query.Chips.RemoveAt(removeAt);
                changed = true;
            }
            return changed;
        }

        public void AddChip(string tag, bool or)
        {
            tag = AssetTags.NormalizeTag(tag);
            if (tag.Length > 0 && !Query.Chips.Any(c => c.Tag == tag))
                Query.Chips.Add(new TagChip(tag, or));
        }

        private int DrawLine(Painter draw, List<(TagChip Chip, float Width)> chips, List<int> line, float y, int removeAt)
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

        private static float ChipWidth(TagChip chip) => MathF.Ceiling(Gui.MeasureText(Label(chip)).x + 8.0f + 16.0f + (chip.IsAttribute ? 3.0f : 0.0f));

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
                text = Style.Color(StyleColor.Text);
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
            if (_tags is null || _index is null)
                yield break;
            string typed = text.Trim();
            bool or = typed.StartsWith('+');
            string query = AssetTags.NormalizeTag(or ? typed[1..] : typed);
            if (query.Length == 0)
                yield break;
            HashSet<string> used = Query.Chips.Select(c => c.Tag).ToHashSet();
            bool attribute = query.StartsWith('@');
            IReadOnlyList<TagCount> source = attribute ? _tags.AttributeTags(_index) : _tags.UserTags();
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
