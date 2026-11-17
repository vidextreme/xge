// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // An editor tool. Mark the class with [EditorTool] (and a parameterless constructor) and Tools registers it; one
    // instance exists per registration. Only one tool is active at a time: the user activates a usable tool in the
    // ribbon, and the active tool receives the event interfaces in ToolEvents.cs and changes how the editor behaves.
    public interface ITool
    {
        // Called on every selection change (and when tools register): whether the tool can be activated with this
        // selection, is disabled, or is hidden.
        ToolUsability OnSelectItems(object[] selection);

        void OnActivate(ToolContext context);

        void OnDeactivate();
    }

    // Passed to an activated tool: the system, its undo recorder and the tool itself.
    public sealed class ToolContext
    {
        internal ToolContext(Tools tools, ITool tool)
        {
            System = tools;
            Tool = tool;
        }

        public Tools System { get; }
        public ITool Tool { get; }

        // Record every state change here so the future undo system can revert it.
        public IUndoRecorder Undo => System.Undo;

        public object[] Selection => System.CurrentSelection;
    }

    // Convenience base: usable with any selection, no-op activation, and IsActive/Context while it's the active tool.
    public abstract class EditorTool : ITool
    {
        public bool IsActive { get; internal set; }
        public ToolContext? Context { get; private set; }

        public virtual ToolUsability OnSelectItems(object[] selection) => ToolUsability.Usable;

        public virtual void OnActivate(ToolContext context) => Context = context;

        public virtual void OnDeactivate() => Context = null;
    }
}
