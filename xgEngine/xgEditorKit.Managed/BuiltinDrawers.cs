// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.imui;
using xg.math;

namespace xg.editor
{
    // Number fields (float, double, integers, Vector2/3/4, Color) drawn by the kit's number row: [NotchTags] puts a tag in
    // front of each element, [Range] clamps and draws a range line per element, Color and [ColorField] add a swatch.
    public static class NumberFieldDrawing
    {
        public static NumberFieldStyle StyleOf(SerializedProperty property, NumberFieldFlags flags = NumberFieldFlags.None, string? defaultTags = null)
        {
            NumberFieldStyle style = new() { Flags = flags, NotchTags = property.GetAttribute<NotchTagsAttribute>()?.Tags ?? defaultTags };
            if (property.GetAttribute<RangeAttribute>() is RangeAttribute range && range.Max > range.Min)
            {
                style.Flags |= NumberFieldFlags.Range;
                style.Min = range.Min;
                style.Max = range.Max;
            }
            return style;
        }

        // Draws the property if it is a number type; returns false for other types.
        public static bool Draw(SerializedProperty property, string label)
        {
            Type type = property.FieldType;
            object? value = property.Value;
            Span<float> v = stackalloc float[4];
            if (type == typeof(float) || type == typeof(double))
            {
                v[0] = property.FloatValue;
                if (Gui.Field(label, v[..1], StyleOf(property)))
                    property.Value = type == typeof(double) ? (double)v[0] : v[0];
                return true;
            }
            if (type == typeof(int) || type == typeof(short) || type == typeof(byte) || type == typeof(uint))
            {
                v[0] = property.IntValue;
                if (Gui.Field(label, v[..1], StyleOf(property, NumberFieldFlags.Integer)))
                    property.Value = (int)MathF.Round(v[0]);
                return true;
            }
            if (value is Vector2 v2)
            {
                v[0] = v2.x; v[1] = v2.y;
                if (Gui.Field(label, v[..2], StyleOf(property)))
                    property.Value = new Vector2(v[0], v[1]);
                return true;
            }
            if (value is Vector3 v3)
            {
                bool color = property.GetAttribute<ColorFieldAttribute>() is not null;
                v[0] = v3.x; v[1] = v3.y; v[2] = v3.z;
                if (Gui.Field(label, v[..3], color ? ColorStyle(property, "r,g,b") : StyleOf(property)))
                    property.Value = new Vector3(v[0], v[1], v[2]);
                return true;
            }
            if (value is Vector4 v4)
            {
                bool color = property.GetAttribute<ColorFieldAttribute>() is not null;
                v[0] = v4.x; v[1] = v4.y; v[2] = v4.z; v[3] = v4.w;
                if (Gui.Field(label, v, color ? ColorStyle(property, "r,g,b,a") : StyleOf(property)))
                    property.Value = new Vector4(v[0], v[1], v[2], v[3]);
                return true;
            }
            if (value is Color c)
            {
                v[0] = c.r; v[1] = c.g; v[2] = c.b; v[3] = c.a;
                if (Gui.Field(label, v, ColorStyle(property, "r,g,b,a")))
                    property.Value = new Color(v[0], v[1], v[2], v[3]);
                return true;
            }
            return false;
        }

        private static NumberFieldStyle ColorStyle(SerializedProperty property, string tags)
        {
            NumberFieldStyle style = StyleOf(property, NumberFieldFlags.Color, tags);
            if ((style.Flags & NumberFieldFlags.Range) == 0)
            {
                style.Flags |= NumberFieldFlags.Range;
                style.Min = 0.0f;
                style.Max = 1.0f;
            }
            return style;
        }
    }

    [CustomPropertyDrawer(typeof(float))]
    [CustomPropertyDrawer(typeof(double))]
    [CustomPropertyDrawer(typeof(int))]
    [CustomPropertyDrawer(typeof(short))]
    [CustomPropertyDrawer(typeof(byte))]
    [CustomPropertyDrawer(typeof(uint))]
    [CustomPropertyDrawer(typeof(Vector2))]
    [CustomPropertyDrawer(typeof(Vector3))]
    [CustomPropertyDrawer(typeof(Vector4))]
    [CustomPropertyDrawer(typeof(Color))]
    public sealed class NumberDrawer : PropertyDrawer
    {
        public override void OnGUI(SerializedProperty property, string label)
        {
            if (!NumberFieldDrawing.Draw(property, label))
                Gui.PropertyLabel(label, property.Value?.ToString() ?? "null");
        }
    }

    [CustomPropertyDrawer(typeof(bool))]
    public sealed class BoolDrawer : PropertyDrawer
    {
        public override void OnGUI(SerializedProperty property, string label)
        {
            bool value = property.BoolValue;
            if (Gui.Checkbox(label, ref value))
                property.BoolValue = value;
        }
    }

    [CustomPropertyDrawer(typeof(string))]
    public sealed class StringDrawer : PropertyDrawer
    {
        private readonly TextBuffer _text = new(256);
        private string? _shown;

        public override void OnGUI(SerializedProperty property, string label)
        {
            string value = property.StringValue;
            if (value != _shown)
            {
                _text.Text = value;
                _shown = value;
            }
            if (Gui.Field(label, _text))
            {
                _shown = _text.Text;
                property.StringValue = _shown;
            }
        }
    }

    [CustomPropertyDrawer(typeof(Enum), true)]
    public sealed class EnumDrawer : PropertyDrawer
    {
        public override void OnGUI(SerializedProperty property, string label)
        {
            int index = property.EnumIndex;
            if (Gui.Combo(label, ref index, property.EnumNames))
                property.EnumIndex = index;
        }
    }

    // [Range(min, max)] on a number field (float, int, Vector2/3/4, Color): clamped, with a range line per element.
    [CustomPropertyDrawer(typeof(RangeAttribute))]
    public sealed class RangeDrawer : PropertyDrawer
    {
        public override void OnGUI(SerializedProperty property, string label)
        {
            if (!NumberFieldDrawing.Draw(property, label))
                Gui.PropertyLabel(label, property.Value?.ToString() ?? "null");
        }
    }

    // Nested structs: a foldout, then the struct's fields indented below it.
    public sealed class StructDrawer : PropertyDrawer
    {
        private bool _open = true;

        public override void OnGUI(SerializedProperty property, string label)
        {
            if (!Gui.Foldout(label, ref _open))
                return;
            using (Gui.Horizontal(0.0f))
            {
                Gui.Space(14.0f);
                using (Gui.Vertical())
                {
                    foreach (SerializedProperty child in property.Children)
                        EditorGUI.PropertyField(child);
                }
            }
        }
    }

    // Fallback for field types without a drawer: shows the value as text.
    public sealed class ValueLabelDrawer : PropertyDrawer
    {
        public override void OnGUI(SerializedProperty property, string label)
        {
            Gui.PropertyLabel(label, property.Value?.ToString() ?? "null");
        }
    }
}
