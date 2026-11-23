// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // Item style in the ribbon. Default = Icon for tools, Wide for buttons and toggles.
    public enum ToolItemStyle
    {
        Default = 0,
        Wide,
        Icon,
    }

    // Ribbon placement shared by tools, buttons and toggles. Category and Group are plain names: entries with the same
    // names share a tab and a card. Priority orders entries in a card (lower first; ties keep declaration order).
    // Icon is a built-in Icon name or a PNG/BMP path (absolute, project-relative, then relative to gameroot/editor).
    public abstract class ToolLayoutAttribute : Attribute
    {
        public string? Category { get; set; }
        public string? Group { get; set; }
        public int Priority { get; set; }
        public string? Tooltip { get; set; }
        public string? Icon { get; set; }
        public string? IconColor { get; set; }
        public ToolItemStyle Style { get; set; }
    }

    // Registers a tool class (ITool with a parameterless constructor).
    [AttributeUsage(AttributeTargets.Class, Inherited = false)]
    public sealed class EditorToolAttribute : ToolLayoutAttribute
    {
        public EditorToolAttribute(string name) => Name = name;
        public string Name { get; }
        public bool ActiveByDefault { get; set; }
    }
    public sealed class MenuSeparatorAttribute : Attribute { }
    public sealed class ContextMenuButtonAttribute : ToolLayoutAttribute
    {
        public ContextMenuButtonAttribute(string label) => Label = label;
        public string Label { get; }
        // Name of a [ToolValidator] method on the same type.
        public string? Validate { get; set; }
        public bool HasSeparator = false;
    }

    // A ribbon button running the method: static void M(), or an instance method of a tool class. Category and Group
    // default to the tool's on a tool class and are required otherwise.
    [AttributeUsage(AttributeTargets.Method, Inherited = false)]
    public sealed class ToolButtonAttribute : ToolLayoutAttribute
    {
        public ToolButtonAttribute(string label) => Label = label;
        public string Label { get; }
        // Name of a [ToolValidator] method on the same type.
        public string? Validate { get; set; }
    }

    // A ribbon toggle: bool M(bool requested) returns the accepted state, which the Tools subsystem's ToggleStore keeps.
    [AttributeUsage(AttributeTargets.Method, Inherited = false)]
    public sealed class ToolToggleAttribute : ToolLayoutAttribute
    {
        public ToolToggleAttribute(string label) => Label = label;
        public string Label { get; }
        public string? Validate { get; set; }
        public bool Default { get; set; }
    }

    // Marks ToolUsability M() as a validator a button or toggle can name. Re-run on selection changes and clicks.
    [AttributeUsage(AttributeTargets.Method, Inherited = false)]
    public sealed class ToolValidatorAttribute : Attribute
    {
    }

    // Placement of a tool's IEditorGUIDraw GUI in the ribbon. Defaults: the tool's category and group, priority + 1000.
    [AttributeUsage(AttributeTargets.Class, Inherited = false)]
    public sealed class ToolGUIAttribute : Attribute
    {
        public string? Category { get; set; }
        public string? Group { get; set; }
        public int Priority { get; set; } = int.MinValue;
    }

    // Optional look of a ribbon tab. Tabs sort by Priority; on a tie declared ones come first, then by first use.
    // Categories that are only used get defaults (priority 0).
    [AttributeUsage(AttributeTargets.Assembly, AllowMultiple = true)]
    public sealed class ToolCategoryAttribute : Attribute
    {
        public ToolCategoryAttribute(string name) => Name = name;
        public string Name { get; }
        public string? Icon { get; set; }
        public string? IconColor { get; set; }
        public int Priority { get; set; }
        public string? Tooltip { get; set; }
    }

    // Optional look of a ribbon group card in a category.
    [AttributeUsage(AttributeTargets.Assembly, AllowMultiple = true)]
    public sealed class ToolGroupAttribute : Attribute
    {
        public ToolGroupAttribute(string category, string name)
        {
            Category = category;
            Name = name;
        }

        public string Category { get; }
        public string Name { get; }
        public int IconColumns { get; set; } = 4;
        public float Width { get; set; } = 150.0f;
        public bool Collapsed { get; set; }
        public int Priority { get; set; }
        public string? Tooltip { get; set; }
    }
}
