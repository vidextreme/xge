// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Globalization;
using xg.imui;
using xg.math;

namespace xg.editor
{
    // Inspector drawing on top of xg.imui.Gui: PropertyField picks the field's drawer and adds [Header] and [Tooltip].
    public static class EditorGUI
    {
        private static readonly HashSet<string> _reported = new();

        // Called with a field's label and value text when its row is right-clicked (a read-only Inspector offers Copy
        // value); null ignores right-clicks.
        public static Action<string, string>? RowContext { get; set; }

        public static void PropertyField(SerializedProperty property, string? label = null)
        {
            foreach (object attribute in property.FieldInfo.GetCustomAttributes(typeof(HeaderAttribute), true))
            {
                Gui.Space(4.0f);
                using (Gui.Font(FontStyle.Bold))
                    Gui.Label(((HeaderAttribute)attribute).Header);
            }

            string text = label ?? property.DisplayName;
            using (Gui.Id(property.PropertyPath))
            {
                try
                {
                    Gui.TakeRowRightClick();
                    DrawerRegistry.GetDrawer(property).OnGUI(property, text);
                    if (Gui.TakeRowRightClick() && RowContext is { } context)
                        context(text, ValueText(property.Value));
                }
                catch (Exception e)
                {
                    Gui.PropertyLabel(text, $"<{e.GetType().Name}>");
                    if (_reported.Add(property.PropertyPath + e.GetType().FullName))
                        Debug.LogError($"[Inspector] Drawing {property.PropertyPath} threw (reported once): {e}");
                }
            }

            if (property.GetAttribute<TooltipAttribute>() is TooltipAttribute tooltip)
                Gui.Tooltip(tooltip.Tooltip);
        }

        // A value as copied text: numbers with up to 4 decimals, vectors and colours comma-separated, else ToString.
        public static string ValueText(object? value) => value switch
        {
            null => string.Empty,
            float f => f.ToString("0.####", CultureInfo.InvariantCulture),
            double d => d.ToString("0.####", CultureInfo.InvariantCulture),
            Vector2 v => Join(v.x, v.y),
            Vector3 v => Join(v.x, v.y, v.z),
            Vector4 v => Join(v.x, v.y, v.z, v.w),
            Color c => Join(c.r, c.g, c.b, c.a),
            IFormattable formattable => formattable.ToString(null, CultureInfo.InvariantCulture),
            _ => value.ToString() ?? string.Empty,
        };

        public static string Join(params float[] values) => string.Join(", ", values.Select(v => v.ToString("0.####", CultureInfo.InvariantCulture)));

        // Draws every visible field of the object.
        public static void ObjectFields(SerializedObject target)
        {
            foreach (SerializedProperty property in target.Properties)
                PropertyField(property);
        }
    }
}
