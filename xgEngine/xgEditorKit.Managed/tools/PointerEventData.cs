// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.math;

namespace xg.editor
{
    public enum PointerEventType
    {
        Down,
        Up,
        Click,
        Move,
        BeginDrag,
        Drag,
        EndDrag,
        Scroll,
    }

    public enum PointerButton
    {
        Left = 0,
        Right,
        Middle,
    }

    // One pointer event over a scene view. Call Use() to stop it from reaching the remaining active tools.
    public sealed class PointerEventData
    {
        public PointerEventData(SceneViewContext view)
        {
            View = view;
        }

        public SceneViewContext View { get; }
        // Pixels from the view's top-left.
        public Vector2 Position { get; set; }
        public Vector2 Delta { get; set; }
        public PointerButton Button { get; set; }
        public int ClickCount { get; set; }
        public float Scroll { get; set; }
        public bool Used { get; private set; }

        public void Use() => Used = true;
    }
}
