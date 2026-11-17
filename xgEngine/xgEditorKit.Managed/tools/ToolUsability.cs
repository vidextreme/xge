// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // What a tool (from OnSelectItems) or a button/toggle (from its validator) can do with the current selection. Many
    // tools can be usable at once; only one of them is active.
    public enum ToolUsability
    {
        // Enabled: the user can activate the tool (replacing the active one) or click the control.
        Usable = 0,
        // Visible but disabled; if the tool is active, the default tool takes over.
        Unusable,
        // Hidden; if the tool is active, the default tool takes over.
        NotApplicable,
    }
}
