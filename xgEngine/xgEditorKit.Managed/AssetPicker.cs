// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Reflection;
using xg.imui;
using xg.math;

namespace xg.editor
{
    // One asset offered by a picker: its GUID (32 hex digits), project-relative path and display name.
    public readonly record struct AssetChoice(string Guid, string Path, string Name);

    // The project's assets as pickers see them. The editor sets AssetBrowser.Current when a project opens.
    public interface IAssetBrowser
    {
        // Project-relative path of an asset GUID, or null when no asset has it.
        string? PathOf(string guid);
        // Assets of a Navigator kind ("mesh", "material", "image", "shader"), sorted by path.
        IReadOnlyList<AssetChoice> List(string kind);
        // The GUID of the last selected asset of that kind, or null.
        string? SelectedGuid(string kind);
        // Icon of a kind.
        Icon IconOf(string kind);
    }

    public static class AssetBrowser
    {
        public static IAssetBrowser? Current { get; set; }

        public static string NameOf(string? guid)
        {
            if (string.IsNullOrEmpty(guid))
                return "None";
            string? path = Current?.PathOf(guid);
            return path is null ? $"Missing ({guid[..Math.Min(8, guid.Length)]})" : AssetNames.DisplayName(path);
        }
    }

    // An asset field: the value column shows the asset's name (click to pick), the popup lists the project's assets of
    // one kind with a search field, None and "Use selected". One instance per drawn field (it keeps the search text).
    public sealed class AssetPicker
    {
        private const float PopupWidth = 260.0f;
        private const float RowHeight = 22.0f;
        private const float ListMaxHeight = 220.0f;

        private readonly TextBuffer _search = new(64);
        private readonly List<AssetChoice> _filtered = new();
        private Vector2 _popupPosition;

        // guid: 32 hex digits, or empty for none. Returns true when the user picked another asset.
        public bool Field(string id, string label, string kind, ref string guid)
        {
            string popupId = "asset_picker##" + id;
            bool changed = false;
            using (Gui.Id(id))
            {
                Result row = Gui.PropertyLabel(label, AssetBrowser.NameOf(guid));
                string? path = string.IsNullOrEmpty(guid) ? null : AssetBrowser.Current?.PathOf(guid);
                Gui.Tooltip(path ?? (string.IsNullOrEmpty(guid) ? $"Click to pick a {kind}" : $"No asset has the GUID {guid}"));
                if ((row & Result.Clicked) != 0)
                {
                    Rect item = Gui.ItemRect;
                    _popupPosition = new Vector2(MathF.Max(item.max.x - PopupWidth, item.min.x), item.max.y + 2.0f);
                    _search.Text = string.Empty;
                    Gui.OpenPopup(popupId);
                }
                changed = DrawPopup(popupId, kind, ref guid);
            }
            return changed;
        }

        private bool DrawPopup(string popupId, string kind, ref string guid)
        {
            using var popup = Gui.Popup(popupId, _popupPosition, new Vector2(PopupWidth, 0.0f));
            if (!popup.IsOpen)
                return false;
            IAssetBrowser? browser = AssetBrowser.Current;
            bool changed = false;
            Gui.Space(4.0f);
            using (Gui.Horizontal())
            {
                Gui.Space(2.0f);
                Gui.SearchField("##asset_search", _search, 0.0f);
                string? selected = browser?.SelectedGuid(kind);
                if (selected is not null && Gui.Button("Use selected"))
                {
                    changed = selected != guid;
                    guid = selected;
                    Gui.ClosePopup();
                }
                Gui.Space(2.0f);
            }

            string filter = _search.Text.Trim();
            _filtered.Clear();
            _filtered.Add(new AssetChoice(string.Empty, string.Empty, "None"));
            if (browser is not null)
            {
                foreach (AssetChoice choice in browser.List(kind))
                    if (filter.Length == 0 || choice.Path.Contains(filter, StringComparison.OrdinalIgnoreCase))
                        _filtered.Add(choice);
            }

            float listHeight = _filtered.Count * (RowHeight + 2.0f) + 4.0f;
            using var list = Gui.ScrollArea("##asset_list", new Vector2(PopupWidth, MathF.Min(listHeight, ListMaxHeight)));
            if (!list.IsOpen)
                return changed;
            float width = MathF.Max(Gui.AvailableSize.x, 1.0f);
            using var canvas = Gui.Canvas("##asset_items", new Vector2(width, listHeight));
            Painter draw = canvas.Draw;
            Icon icon = browser?.IconOf(kind) ?? Icon.File;
            for (int i = 0; i < _filtered.Count; i++)
            {
                AssetChoice choice = _filtered[i];
                Rect row = Rect.FromXYWH(4.0f, 2.0f + i * (RowHeight + 2.0f), width - 8.0f, RowHeight);
                Result hit;
                using (Gui.Id(i))
                    hit = Gui.HitArea("##asset_item", row);
                if (choice.Path.Length > 0)
                    Gui.Tooltip(choice.Path);
                bool current = choice.Guid == guid;
                Color32 fill = Style.Color(current ? StyleColor.Accent : hit.Hovered() ? StyleColor.ButtonHover : StyleColor.Button);
                draw.RectFilled(row, fill, 3.0f);
                Color32 text = Style.Color(current ? StyleColor.TextOnAccent : StyleColor.Text);
                float cy = row.min.y + row.Height * 0.5f;
                if (choice.Path.Length > 0)
                    draw.Icon(icon, new Vector2(row.min.x + 12.0f, cy), 12.0f, text);
                draw.Text(new Vector2(row.min.x + 24.0f, MathF.Round(cy - Gui.MeasureText(choice.Name).y * 0.5f)), text, choice.Name);
                if (hit.Clicked())
                {
                    changed = choice.Guid != guid;
                    guid = choice.Guid;
                    Gui.ClosePopup();
                }
            }
            return changed;
        }
    }

    // Draws AssetReference fields as asset pickers; [AssetType("mesh")] on the field picks the kind (default any file).
    [CustomPropertyDrawer(typeof(AssetReference))]
    public sealed class AssetReferenceDrawer : PropertyDrawer
    {
        private readonly AssetPicker _picker = new();

        public override void OnGUI(SerializedProperty property, string label)
        {
            AssetReference value = property.Value is AssetReference reference ? reference : default;
            string kind = FieldInfo?.GetCustomAttribute<AssetTypeAttribute>()?.Kind ?? "other";
            string guid = value.ToString();
            if (_picker.Field(property.Name, label, kind, ref guid))
                property.Value = AssetReference.FromText(guid);
        }
    }
}
