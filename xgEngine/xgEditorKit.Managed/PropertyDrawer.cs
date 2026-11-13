// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Reflection;

namespace xg.editor
{
    // Draws one field in the Inspector. Derive from it in an editor script (Assets/**/Editor/) and mark the class with
    // [CustomPropertyDrawer(typeof(SomeAttribute))] to draw fields carrying that PropertyAttribute, or
    // [CustomPropertyDrawer(typeof(SomeType))] to draw every field of that type. Each drawn field gets its own instance,
    // so a drawer may keep per-field state. Draw with the layout API (xg.imui.Gui / EditorGUI); no rectangles needed.
    public abstract class PropertyDrawer
    {
        // The PropertyAttribute this drawer was chosen for (null when chosen by field type).
        public PropertyAttribute? Attribute { get; internal set; }

        // The drawn field, as seen from the editor (its attributes and type are the editor's copies).
        public FieldInfo FieldInfo { get; internal set; } = null!;

        public abstract void OnGUI(SerializedProperty property, string label);
    }

    // Selects the field attribute or field type a PropertyDrawer draws. With useForChildren the drawer also applies to
    // attributes or types derived from it.
    [AttributeUsage(AttributeTargets.Class, AllowMultiple = true, Inherited = false)]
    public sealed class CustomPropertyDrawerAttribute : Attribute
    {
        public CustomPropertyDrawerAttribute(Type type, bool useForChildren = false)
        {
            Type = type;
            UseForChildren = useForChildren;
        }

        public Type Type { get; }
        public bool UseForChildren { get; }
    }
}
