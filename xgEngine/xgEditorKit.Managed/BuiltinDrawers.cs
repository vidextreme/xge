// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.imui;
using xg.math;

namespace xg.editor
{
    [CustomPropertyDrawer(typeof(float))]
    public sealed class FloatDrawer : PropertyDrawer
    {
        public override void OnGUI(SerializedProperty property, string label)
        {
            float value = property.FloatValue;
            if (Gui.Field(label, ref value))
                property.FloatValue = value;
        }
    }

    [CustomPropertyDrawer(typeof(double))]
    public sealed class DoubleDrawer : PropertyDrawer
    {
        public override void OnGUI(SerializedProperty property, string label)
        {
            float value = property.FloatValue;
            if (Gui.Field(label, ref value))
                property.Value = (double)value;
        }
    }

    [CustomPropertyDrawer(typeof(int))]
    [CustomPropertyDrawer(typeof(short))]
    [CustomPropertyDrawer(typeof(byte))]
    [CustomPropertyDrawer(typeof(uint))]
    public sealed class IntDrawer : PropertyDrawer
    {
        public override void OnGUI(SerializedProperty property, string label)
        {
            int value = property.IntValue;
            if (Gui.Field(label, ref value))
                property.Value = value;
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

    [CustomPropertyDrawer(typeof(Vector2))]
    public sealed class Vector2Drawer : PropertyDrawer
    {
        public override void OnGUI(SerializedProperty property, string label)
        {
            Vector2 value = property.Vector2Value;
            if (Gui.Field(label, ref value))
                property.Vector2Value = value;
        }
    }

    [CustomPropertyDrawer(typeof(Vector3))]
    public sealed class Vector3Drawer : PropertyDrawer
    {
        public override void OnGUI(SerializedProperty property, string label)
        {
            Vector3 value = property.Vector3Value;
            if (Gui.Field(label, ref value))
                property.Vector3Value = value;
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

    // [Range(min, max)] on a float or int field.
    [CustomPropertyDrawer(typeof(RangeAttribute))]
    public sealed class RangeDrawer : PropertyDrawer
    {
        public override void OnGUI(SerializedProperty property, string label)
        {
            RangeAttribute range = (RangeAttribute)Attribute!;
            if (property.FieldType == typeof(int))
            {
                int value = property.IntValue;
                if (Gui.Slider(label, ref value, (int)range.Min, (int)range.Max))
                    property.IntValue = value;
                return;
            }
            float f = property.FloatValue;
            if (Gui.Slider(label, ref f, range.Min, range.Max))
                property.FloatValue = f;
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
