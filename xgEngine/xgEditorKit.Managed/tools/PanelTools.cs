// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.imui;

namespace xg.editor
{
    // A panel that is a passive tool: it follows the selection through OnSelectItems. Mark the Panel subclass (with a
    // parameterless constructor) with [PanelTool]; the Tools ribbon lists it under View > Tool Windows. Tools tracks
    // every registered instance by itself (no registration code in the panel) and assigns InstanceIndex to the
    // instances it creates: 0 for the first, 1+ for extra instances of a non-unique panel tool.
    public interface IPanelTool : ITool
    {
        int InstanceIndex { get; set; }
    }

    // Unique = one instance, shown in the ribbon as a toggle (with the state dot) that shows and hides it. Otherwise the
    // ribbon shows a button that opens another instance (or reopens the first one when it's closed).
    [AttributeUsage(AttributeTargets.Class, Inherited = false)]
    public sealed class PanelToolAttribute : Attribute
    {
        public PanelToolAttribute(string name) => Name = name;
        public string Name { get; }
        public string? Icon { get; set; }
        public string? IconColor { get; set; }
        public string? Tooltip { get; set; }
        public bool Unique { get; set; }
        public int Priority { get; set; }
    }

    public static class PanelTools
    {
        public const string Category = "View";
        public const string Group = "Tool Windows";

        // Panel title for an instance: every instance shows the same name; extra instances get a unique window id.
        public static string Title(string name, int instanceIndex) =>
            instanceIndex <= 0 ? name : $"{name}###{name}#{instanceIndex}";
    }
}
