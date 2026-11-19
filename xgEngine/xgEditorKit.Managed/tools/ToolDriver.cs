// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // The data side of a tool for one selection item: it loads, edits and saves what the tool presents (a texture's
    // import settings, an actor's components). A tool creates it through ITool.CreateDriver; the ToolSelection that
    // holds it disposes it once its item is deselected and no view keeps that selection. Drivers never draw or touch
    // the UI (no Gui, Painter or Texture), so batch processes can create and apply them without an editor window.
    public interface IToolDriver : IDisposable
    {
        // Edits that Apply would save and Revert would drop.
        bool HasPendingChanges => false;

        void Apply() { }

        void Revert() { }

        void IDisposable.Dispose() { }
    }
}
