// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // An editor tool: anything that follows the selection. Tools are presentation: every tool has one instance, made
    // and held by Tools ([EditorTool], [InspectorTool]; panel tools are their own windows), and one instance can serve
    // several selections at once (the editor selection and a locked Inspector's). The data a tool works on lives in a
    // driver (IToolDriver) that the tool creates for a selection item; view state lives in the view (ToolView).
    // A tool that only implements ITool is passive: it answers OnSelectItems and nothing else.
    public interface ITool
    {
        // Can the tool work with these objects? Called on every selection change, also for selections the editor
        // selection no longer has (a locked Inspector), so answer from the arguments and never store them. For an
        // activatable tool the result decides whether it can be activated, is disabled, or is hidden.
        ToolUsability OnSelectItems(object[] selection);

        // The driver holding this tool's data for the selection, or null when the tool keeps no data. Called once per
        // DriverKey; the driver lives until its item is deselected and no view keeps the selection.
        IToolDriver? CreateDriver(ToolSelection selection) => null;

        // The item the driver works on (e.g. the shown AssetRef), so the driver carries over while that item stays
        // selected. Null means no driver. The default is the whole selection: a new driver for every change.
        object? DriverKey(ToolSelection selection) => selection;
    }
}
