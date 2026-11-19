// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // A tool the user can activate from the ribbon. Only one is active at a time; the active tool receives the event
    // interfaces in ToolEvents.cs and changes how the editor behaves.
    public interface IActivatableTool : ITool
    {
        void OnActivate(ToolContext context);

        void OnDeactivate();
    }

    // Passed to an activated tool: the system, its undo recorder, the tool itself, and the editor selection with the
    // tool's driver for it.
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

        public ToolSelection Selection => global::xg.editor.Selection.Current;

        public IToolDriver? Driver => global::xg.editor.Selection.Current.DriverFor(Tool);
    }

    // Convenience base for activatable tools: usable with any selection, no-op activation, and IsActive/Context while
    // it's the active tool.
    public abstract class EditorTool : IActivatableTool
    {
        public bool IsActive { get; internal set; }
        public ToolContext? Context { get; private set; }

        public virtual ToolUsability OnSelectItems(object[] selection) => ToolUsability.Usable;

        public virtual void OnActivate(ToolContext context) => Context = context;

        public virtual void OnDeactivate() => Context = null;
    }
}
