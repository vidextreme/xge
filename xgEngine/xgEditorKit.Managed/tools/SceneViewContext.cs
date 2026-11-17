// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.imui;
using xg.math;

namespace xg.editor
{
    // The scene view an event or draw call belongs to: its rect in the panel's draw space, pixel size and camera.
    public sealed class SceneViewContext
    {
        public Rect ViewRect { get; set; }
        public Vector2 PixelSize { get; set; }
        public Vector3 CameraPosition { get; set; }
        public Vector3 CameraTarget { get; set; }
        public Vector3 CameraUp { get; set; } = new(0.0f, 1.0f, 0.0f);
        public float FovY { get; set; } = 0.9f;
        public float NearZ { get; set; } = 0.1f;
        public float FarZ { get; set; } = 100.0f;

        // Painter for overlay drawing; valid only inside OnSceneGUI.
        public Painter Draw => Gui.Draw;
    }
}
