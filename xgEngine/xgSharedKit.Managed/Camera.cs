// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg.math;

namespace xg
{
    [Flags]
    public enum CameraFlags : uint
    {
        None = 0,
        // A candidate for the main camera (the Game view uses the main camera with the highest priority).
        Main = 1u << 0,
        // The editor draws the camera's frustum in the Scene view.
        ShowFrustum = 1u << 1,
    }

    public enum CameraProjection : uint
    {
        Perspective = 0,
        Orthographic = 1,
    }

    public enum CameraFovAxis : uint
    {
        Vertical = 0,
        Horizontal = 1,
    }

    public enum CameraClear : uint
    {
        Color = 0,
        // Reserved until the engine has a sky.
        Skybox = 1,
        None = 2,
    }

    public enum CameraTarget : uint
    {
        GameView = 0,
    }

    // Camera settings in the "xg.Camera" pool (one per actor), readable from any module. The camera looks
    // down its actor's +Z with +Y up, from the actor's world position.
    [StructLayout(LayoutKind.Sequential)]
    [ComponentDataKey("xg.Camera")]
    public struct CameraData : IComponentData
    {
        public CameraFlags Flags;
        public int Priority;
        public CameraProjection Projection;
        // Degrees, along FovAxis.
        public float FieldOfView;
        public CameraFovAxis FovAxis;
        // Half the view's height in world units (orthographic).
        public float OrthographicSize;
        public float Near;
        public float Far;
        public CameraClear Clear;
        public Color Background;
        // The part of the target the camera draws into, in 0..1 (x, y from the top-left, width, height).
        public Vector4 Viewport;
        public CameraTarget Target;
        [HideInInspector]
        public uint Reserved;

        public static CameraData Default => new()
        {
            Flags = CameraFlags.Main | CameraFlags.ShowFrustum,
            Projection = CameraProjection.Perspective,
            FieldOfView = 60.0f,
            OrthographicSize = 5.0f,
            Near = 0.1f,
            Far = 1000.0f,
            Clear = CameraClear.Color,
            Background = new Color(0.16f, 0.24f, 0.32f, 1.0f),
            Viewport = new Vector4(0.0f, 0.0f, 1.0f, 1.0f),
        };

        // The vertical field of view in radians for a target of this aspect ratio (width / height).
        public readonly float VerticalFovRadians(float aspect)
        {
            float degrees = Math.Clamp(FieldOfView, 1.0f, 179.0f);
            float radians = degrees * MathF.PI / 180.0f;
            if (FovAxis == CameraFovAxis.Horizontal && aspect > 0.0f)
                radians = 2.0f * MathF.Atan(MathF.Tan(radians * 0.5f) / aspect);
            return radians;
        }
    }

    // A camera: projection, clipping, background, viewport and priority, placed by its actor's transform. The Game view
    // renders through Camera.Main (the Main camera with the highest Priority).
    [ComponentIcon("Camera", Color = "#6FB3E8")]
    [RequireComponent(typeof(Transform))]
    public sealed class Camera : ComponentDataSet<CameraData>
    {
        protected override void OnAttached()
        {
            ref CameraData data = ref Data1;
            if (data.Far <= 0.0f && data.FieldOfView <= 0.0f)
                data = CameraData.Default;
        }

        public bool IsMain
        {
            get => (Data1.Flags & CameraFlags.Main) != 0;
            set => Data1.Flags = value ? Data1.Flags | CameraFlags.Main : Data1.Flags & ~CameraFlags.Main;
        }

        public int Priority
        {
            get => Data1.Priority;
            set => Data1.Priority = value;
        }

        public CameraProjection Projection
        {
            get => Data1.Projection;
            set => Data1.Projection = value;
        }

        public float FieldOfView
        {
            get => Data1.FieldOfView;
            set => Data1.FieldOfView = Math.Clamp(value, 1.0f, 179.0f);
        }

        public float OrthographicSize
        {
            get => Data1.OrthographicSize;
            set => Data1.OrthographicSize = MathF.Max(value, 0.001f);
        }

        public float Near
        {
            get => Data1.Near;
            set => Data1.Near = value;
        }

        public float Far
        {
            get => Data1.Far;
            set => Data1.Far = value;
        }

        public Color Background
        {
            get => Data1.Background;
            set => Data1.Background = value;
        }

        // Where a camera at this world pose looks: forward is the rotated +Z, up the rotated +Y.
        public static void ViewBasis(in TransformData world, out Vector3 forward, out Vector3 up)
        {
            float x = world.RotationX, y = world.RotationY, z = world.RotationZ, w = world.RotationW;
            forward = new Vector3(2.0f * (x * z + w * y), 2.0f * (y * z - w * x), 1.0f - 2.0f * (x * x + y * y));
            up = new Vector3(2.0f * (x * y - w * z), 1.0f - 2.0f * (x * x + z * z), 2.0f * (y * z + w * x));
        }

        // The actor of the Main camera with the highest Priority, from any module; ActorID.InvalidID when there is none.
        public static ActorID Main
        {
            get
            {
                if (ScriptRuntime._enginePtr == 0)
                    return ActorID.InvalidID;
                Span<CameraData> cameras = ComponentData.All<CameraData>(out ReadOnlySpan<ActorID> owners);
                ActorID best = ActorID.InvalidID;
                int priority = int.MinValue;
                for (int i = 0; i < cameras.Length; i++)
                {
                    if ((cameras[i].Flags & CameraFlags.Main) == 0 || cameras[i].Priority < priority)
                        continue;
                    if (cameras[i].Priority == priority && best.Value != 0)
                        continue;
                    best = owners[i];
                    priority = cameras[i].Priority;
                }
                return best;
            }
        }
    }
}
