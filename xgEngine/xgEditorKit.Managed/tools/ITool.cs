// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // An editor tool: anything that follows the selection. A tool that only implements ITool is passive: it gets
    // OnSelectItems on every selection change and nothing else (panel tools such as the Inspector are passive).
    // Register a class with [EditorTool] (parameterless constructor; one instance each) or as a panel with [PanelTool].
    public interface ITool
    {
        // Called on every selection change (and when the tool registers). For an activatable tool the result decides
        // whether it can be activated with this selection, is disabled, or is hidden; passive tools usually return Usable.
        ToolUsability OnSelectItems(object[] selection);
    }

    // A tool the user can activate from the ribbon. Only one is active at a time; the active tool receives the event
    // interfaces in ToolEvents.cs and changes how the editor behaves.
    public interface IActivatableTool : ITool
    {
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
