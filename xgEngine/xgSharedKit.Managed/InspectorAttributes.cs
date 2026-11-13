// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg
{
    // Base for field attributes that an editor PropertyDrawer can target with [CustomPropertyDrawer(typeof(...))].
    // Attributes are runtime metadata only; drawers live in editor assemblies (Assets/**/Editor/).
    [AttributeUsage(AttributeTargets.Field, Inherited = true, AllowMultiple = false)]
    public abstract class PropertyAttribute : Attribute
    {
        public int Order { get; set; }
    }

    // Shows a float or int field as a slider between Min and Max.
    public sealed class RangeAttribute : PropertyAttribute
    {
        public RangeAttribute(float min, float max)
        {
            Min = min;
            Max = max;
        }

        public float Min { get; }
        public float Max { get; }
    }

    [AttributeUsage(AttributeTargets.Field, AllowMultiple = false)]
    public sealed class TooltipAttribute : Attribute
    {
        public TooltipAttribute(string tooltip) => Tooltip = tooltip;
        public string Tooltip { get; }
    }

    // Draws a bold header label above the field.
    [AttributeUsage(AttributeTargets.Field, AllowMultiple = true)]
    public sealed class HeaderAttribute : Attribute
    {
        public HeaderAttribute(string header) => Header = header;
        public string Header { get; }
    }

    // Hides a public field from the Inspector.
    [AttributeUsage(AttributeTargets.Field)]
    public sealed class HideInInspectorAttribute : Attribute
    {
    }

    // Shows a non-public field in the Inspector.
    [AttributeUsage(AttributeTargets.Field)]
    public sealed class SerializeFieldAttribute : Attribute
    {
    }
}
