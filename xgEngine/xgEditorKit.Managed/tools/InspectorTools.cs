// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // A tool that draws an Inspector panel's content. Mark the class (with a parameterless constructor) with
    // [InspectorTool]; Tools creates its one instance. Every Inspector asks the tools, lowest priority first, through
    // its own selection (OnSelectItems) and draws the first usable one. The tool is presentation only: its data lives
    // in its driver (ITool.CreateDriver, context.Driver; Apply/Revert and pending changes are the driver's), and its
    // per-Inspector UI state in context.View. Inspector tools are independent of the active scene tool.
    public interface IInspectorTool : ITool
    {
        void OnInspectorGUI(InspectorToolContext context);
    }

    public sealed class InspectorToolContext
    {
        public InspectorToolContext(ToolSelection selection, IToolDriver? driver, ToolView view, bool locked, Action drawLock, Action lockAndFocus)
        {
            Selection = selection;
            Driver = driver;
            View = view;
            Locked = locked;
            DrawLock = drawLock;
            LockAndFocus = lockAndFocus;
        }

        // The Inspector's selection: the editor selection, or the one it kept when it was locked.
        public ToolSelection Selection { get; }
        // The tool's driver for that selection (Selection.DriverFor(tool)).
        public IToolDriver? Driver { get; }
        // This Inspector's state for this tool.
        public ToolView View { get; }
        public bool Locked { get; }
        public Action DrawLock { get; }
        public Action LockAndFocus { get; }
    }

    [AttributeUsage(AttributeTargets.Class, Inherited = false)]
    public sealed class InspectorToolAttribute : Attribute
    {
        public InspectorToolAttribute(string name) => Name = name;
        public string Name { get; }
        public string? Icon { get; set; }
        public string? Tooltip { get; set; }
        public int Priority { get; set; }
    }
}
