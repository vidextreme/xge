// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.math;

namespace xg.imui
{
    public enum ToggleDot
    {
        None,
        // Filled while on, a hollow ring while off (filter toggles).
        State,
        // A small accent dot (something is active behind this button, e.g. a filter while its options are hidden).
        Accent,
    }

    // The raised button look shared by the Ribbon items and other toggle rows (Navigator filters and option tabs):
    // Button grey with a darker bottom edge, Accent while on, flat and dim while disabled.
    public static class ToggleButton
    {
        public const float Rounding = 3.0f;
        public const float IconSize = 14.0f;

        public static void Background(Painter draw, Rect rect, bool on, bool enabled, bool hovered, bool pressed)
        {
            if (!enabled)
            {
                draw.RectFilled(rect, Style.Color(StyleColor.PanelHeaderHover), Rounding);
                return;
            }
            Color32 fill = on
                ? Style.Color(hovered ? StyleColor.AccentHover : StyleColor.Accent)
                : Style.Color(hovered ? StyleColor.ButtonHover : StyleColor.Button);
            draw.RectFilled(rect, Style.Color(StyleColor.SegmentBg), Rounding);
            draw.RectFilled(new Rect(rect.min, new Vector2(rect.max.x, rect.max.y - 1.0f)), fill, Rounding);
            if (pressed)
                draw.RectFilled(rect, new Color32(255, 255, 255, 20), Rounding);
        }

        public static void Colors(bool on, bool enabled, Color32? iconColor, out Color32 icon, out Color32 text)
        {
            if (!enabled)
            {
                Color32 dim = Style.Color(StyleColor.TextDim);
                dim.a = 120;
                icon = dim;
                text = dim;
            }
            else if (on)
            {
                icon = Style.Color(StyleColor.TextOnAccent);
                text = icon;
            }
            else
            {
                icon = iconColor ?? Style.Color(StyleColor.Icon);
                text = Style.Color(StyleColor.Text);
            }
        }

        public static void Dot(Painter draw, Rect rect, ToggleDot dot, bool on, bool enabled)
        {
            if (dot == ToggleDot.None)
                return;
            Vector2 center = new(rect.max.x - 5.0f, rect.min.y + 5.0f);
            if (dot == ToggleDot.Accent)
            {
                draw.CircleFilled(center, 2.5f, Style.Color(on ? StyleColor.TextOnAccent : StyleColor.Accent));
                return;
            }
            if (on)
            {
                draw.CircleFilled(center, 2.0f, Style.Color(StyleColor.TextOnAccent));
                return;
            }
            Color32 ring = Style.Color(StyleColor.TextDim);
            if (!enabled)
                ring.a = 120;
            draw.Circle(center, 2.0f, ring, 1.0f);
        }

        // Width of a button showing icon, label and badge (label/badge may be null).
        public static float Measure(string? label, string? badge, bool iconOnly)
        {
            float width = 6.0f + IconSize + 6.0f;
            if (!iconOnly && !string.IsNullOrEmpty(label))
                width += Gui.MeasureText(label).x + 6.0f;
            if (!iconOnly && !string.IsNullOrEmpty(badge))
                width += Gui.MeasureText(badge).x + 6.0f;
            return MathF.Ceiling(width + 4.0f);
        }

        // Draws the button in rect (current draw space) and returns its hit result. A null label draws the icon only;
        // badge is dim text after the label (a count). The tooltip shows on hover.
        public static Result Draw(Painter draw, string id, Rect rect, Icon icon, Color32? iconColor, string? label, string? badge,
            string? tooltip, bool on, bool enabled, ToggleDot dot)
        {
            Result hit = Gui.HitArea(id, rect);
            if (!string.IsNullOrEmpty(tooltip))
                Gui.Tooltip(tooltip);
            Background(draw, rect, on, enabled, enabled && hit.Hovered(), enabled && (hit & Result.Active) != 0);
            Colors(on, enabled, iconColor, out Color32 iconTint, out Color32 text);
            float cy = rect.min.y + rect.Height * 0.5f;
            float lineHeight = Gui.MeasureText("A").y;
            bool iconOnly = string.IsNullOrEmpty(label);
            float x = iconOnly ? rect.min.x + (rect.Width - IconSize) * 0.5f : rect.min.x + 6.0f;
            if (icon != Icon.None)
                draw.Icon(icon, new Vector2(x + IconSize * 0.5f, cy), IconSize, iconTint);
            x += IconSize + 6.0f;
            if (!iconOnly)
            {
                draw.Text(new Vector2(x, MathF.Round(cy - lineHeight * 0.5f)), text, label!);
                x += Gui.MeasureText(label!).x + 6.0f;
                if (!string.IsNullOrEmpty(badge))
                {
                    Color32 badgeColor = on ? text : Style.Color(StyleColor.TextDim);
                    if (on)
                        badgeColor.a = 200;
                    draw.Text(new Vector2(x, MathF.Round(cy - lineHeight * 0.5f)), badgeColor, badge!);
                }
            }
            Dot(draw, rect, dot, on, enabled);
            return enabled ? hit : Result.None;
        }
    }
}
