// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg.editor
{
    // Hook for the future undo system: tools record every change with an undo and a redo action. Until an undo system
    // replaces the Tools subsystem's Undo, NullUndoRecorder records nothing.
    public interface IUndoRecorder
    {
        void Record(string label, Action undo, Action redo);

        // Groups the records made until Dispose into one undo step.
        IDisposable Group(string label);
    }

    public sealed class NullUndoRecorder : IUndoRecorder
    {
        public static readonly NullUndoRecorder Instance = new();

        private sealed class NullGroup : IDisposable
        {
            public static readonly NullGroup Instance = new();
            public void Dispose() { }
        }

        public void Record(string label, Action undo, Action redo) { }

        public IDisposable Group(string label) => NullGroup.Instance;
    }
}
