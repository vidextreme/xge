// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // The Camera card: draws CameraData in sections (main and priority; Projection; Background; Output) with segments for
    // the projection and clear modes, a deg notch on the field of view, near/far notch tags on the clipping planes, the
    // colour picker for the background and x/y/w/h tags on the viewport.
    public sealed class CameraDataEditor : IComponentDataEditor
    {
        private const float RowHeight = 22.0f;
        private static readonly string[] _fovAxes = { "Vertical", "Horizontal" };
        private static readonly string[] _targets = { "Game view" };
        private static readonly Icon[] _projectionIcons = { Icon.Sphere, Icon.TwoD };
        private static readonly string[] _projectionNames = { "Perspective", "Orthographic" };
        private static readonly Icon[] _clearIcons = { Icon.Material, Icon.Globe, Icon.Stack };
        private static readonly string[] _clearNames = { "Solid color: clear to the background color", "Skybox", "None: keep the previous contents" };
        private static readonly bool[] _clearEnabled = { true, false, true };
        private const float RadioCell = 34.0f;

        public string DataType => typeof(CameraData).FullName!;

        public object OnGUI(ActorID actor, object data)
        {
            CameraData camera = data is CameraData own ? own : (CameraData)(TypeBridge.Convert(data, typeof(CameraData)) ?? CameraData.Default);

            bool main = (camera.Flags & CameraFlags.Main) != 0;
            if (Gui.Checkbox("Main camera", ref main))
                camera.Flags = main ? camera.Flags | CameraFlags.Main : camera.Flags & ~CameraFlags.Main;
            Gui.Tooltip("The Game view renders through the main camera with the highest priority");
            Gui.Field("Priority", ref camera.Priority);

            Header("Projection");
            int projection = (int)camera.Projection;
            if (RadioIcons("Mode", _projectionIcons, _projectionNames, ref projection, null))
                camera.Projection = (CameraProjection)projection;
            if (camera.Projection == CameraProjection.Perspective)
            {
                Span<float> fov = stackalloc float[1] { camera.FieldOfView };
                if (Gui.Field("Field of view", fov, new NumberFieldStyle { NotchTags = "deg", Flags = NumberFieldFlags.Range, Min = 1.0f, Max = 179.0f }, 0.0f))
                    camera.FieldOfView = Math.Clamp(fov[0], 1.0f, 179.0f);
                int axis = (int)camera.FovAxis;
                if (Gui.Combo("FOV axis", ref axis, _fovAxes))
                    camera.FovAxis = (CameraFovAxis)axis;
            }
            else
            {
                Span<float> size = stackalloc float[1] { camera.OrthographicSize };
                if (Gui.Field("Size", size, new NumberFieldStyle { NotchTags = "half h" }, 0.05f))
                    camera.OrthographicSize = MathF.Max(size[0], 0.001f);
                Gui.Tooltip("Half the view's height in world units");
            }
            Span<float> clip = stackalloc float[2] { camera.Near, camera.Far };
            if (Gui.Field("Clipping planes", clip, new NumberFieldStyle { NotchTags = "near=#c8a040, far=#a060c8" }, 0.05f))
            {
                camera.Near = MathF.Max(clip[0], 0.0001f);
                camera.Far = MathF.Max(clip[1], camera.Near + 0.001f);
            }

            Header("Background");
            int clear = (int)camera.Clear;
            if (RadioIcons("Clear", _clearIcons, _clearNames, ref clear, _clearEnabled))
                camera.Clear = (CameraClear)clear;
            Span<float> background = stackalloc float[3] { camera.Background.r, camera.Background.g, camera.Background.b };
            using (Gui.Disabled(camera.Clear != CameraClear.Color))
            {
                if (Gui.Field("Background", background, new NumberFieldStyle { NotchTags = "r,g,b", Flags = NumberFieldFlags.Color | NumberFieldFlags.Range, Min = 0.0f, Max = 1.0f }, 0.0f))
                    camera.Background = new Color(background[0], background[1], background[2], 1.0f);
            }

            Header("Output");
            Span<float> viewport = stackalloc float[4] { camera.Viewport.x, camera.Viewport.y, camera.Viewport.z, camera.Viewport.w };
            if (Gui.Field("Viewport rect", viewport, new NumberFieldStyle { NotchTags = "x,y,w,h", Flags = NumberFieldFlags.Range, Min = 0.0f, Max = 1.0f }, 0.0f))
                camera.Viewport = new Vector4(viewport[0], viewport[1], MathF.Max(viewport[2], 0.01f), MathF.Max(viewport[3], 0.01f));
            int target = (int)camera.Target;
            if (Gui.Combo("Target", ref target, _targets))
                camera.Target = (CameraTarget)target;
            Gui.Tooltip("Render textures come later");
            bool frustum = (camera.Flags & CameraFlags.ShowFrustum) != 0;
            if (Gui.Checkbox("Show frustum", ref frustum))
                camera.Flags = frustum ? camera.Flags | CameraFlags.ShowFrustum : camera.Flags & ~CameraFlags.ShowFrustum;
            Gui.Tooltip("Draw the camera's frustum in the Scene view");

            return data is CameraData ? camera : TypeBridge.Convert(camera, data.GetType()) ?? data;
        }

        private static void Header(string text)
        {
            Gui.Space(4.0f);
            using (Gui.Font(FontStyle.Bold))
                Gui.Label(text);
        }

        // A property row of icon-only radio cells (the Ribbon toggle look with the circle dot; disabled ones dimmed, names
        // in the tooltips). Returns true when another one was picked.
        private static bool RadioIcons(string label, Icon[] icons, string[] names, ref int index, bool[]? enabled)
        {
            float width = MathF.Max(Gui.AvailableSize.x, 160.0f);
            float labelWidth = MathF.Round(width * 0.4f);
            using var id = Gui.Id("radio_" + label);
            using var canvas = Gui.Canvas("##row", new Vector2(width, RowHeight + 2.0f));
            Painter draw = canvas.Draw;
            draw.Text(new Vector2(0.0f, MathF.Round((RowHeight - Gui.MeasureText("A").y) * 0.5f) + 1.0f), Style.Color(StyleColor.Text), label);
            Rect segment = Rect.FromXYWH(labelWidth, 0.0f, icons.Length * RadioCell + 2.0f, RowHeight + 2.0f);
            draw.RectFilled(segment, Style.Color(StyleColor.SegmentBg), 3.0f);
            bool changed = false;
            for (int i = 0; i < icons.Length; i++)
            {
                bool on = index == i;
                bool usable = enabled is null || enabled[i];
                string tip = usable ? names[i] : names[i] + " (later)";
                Rect cell = Rect.FromXYWH(segment.min.x + 1.0f + i * RadioCell, 1.0f, RadioCell - 2.0f, RowHeight);
                if (ToggleButton.Draw(draw, "##option" + i, cell, icons[i], null, null, null, tip, on, usable, ToggleDot.Radio).Clicked() && usable && !on)
                {
                    index = i;
                    changed = true;
                }
            }
            return changed;
        }
    }

    // The Camera preview card: renders the scene through the selected camera at 16:9 in its own render view.
    public sealed unsafe class CameraPreview : IComponentPreview
    {
        private const float Aspect = 16.0f / 9.0f;

        private SceneViewState _state;
        private ulong _view;
        private int _width;
        private int _height;
        private float _sentOrthographic = -1.0f;

        public string ComponentType => typeof(Camera).FullName!;
        public string Title => "Camera Preview";

        public void OnGUI(ActorID actor, object component)
        {
            if (!ComponentData.TryGet(actor, out CameraData data) || !Transform.Exists(actor))
            {
                Gui.Label("The camera needs a transform.");
                return;
            }
            float width = MathF.Max(Gui.AvailableSize.x, 64.0f);
            Vector2 size = new(width, MathF.Round(width / Aspect));
            _state.TextureId = _view;
            Gui.SceneView("##camera_preview", ref _state, "No 3D renderer", size);
            Rect rect = Gui.ItemRect;
            string label = data.Projection == CameraProjection.Orthographic ? $"16:9 · ortho {data.OrthographicSize:0.##}" : $"16:9 · {data.FieldOfView:0.#}°";
            Vector2 measure = Gui.MeasureText(label);
            Gui.Draw.Text(new Vector2(rect.max.x - measure.x - 6.0f, rect.max.y - measure.y - 4.0f), Style.Color(StyleColor.TextDim), label);
            Render(actor, data);
        }

        private void Render(ActorID actor, in CameraData data)
        {
            int width = _state.PixelWidth;
            int height = _state.PixelHeight;
            if (SceneViews.HasRenderer() == 0 || width <= 0 || height <= 0)
                return;
            if (_view == 0)
            {
                _view = SceneViews.Create(width, height);
                (_width, _height) = (width, height);
            }
            else if (width != _width || height != _height)
            {
                SceneViews.Resize(_view, width, height);
                (_width, _height) = (width, height);
            }
            if (_view == 0)
                return;
            TransformData world = Transform.Of(actor).World;
            Camera.ViewBasis(world, out Vector3 forward, out Vector3 up);
            SceneViews.Camera camera = default;
            camera.Eye = world.Position;
            camera.Target = new Vector3(world.PositionX + forward.x, world.PositionY + forward.y, world.PositionZ + forward.z);
            camera.Up = up;
            camera.FovY = data.VerticalFovRadians(Aspect);
            camera.NearZ = MathF.Max(data.Near, 0.0001f);
            camera.FarZ = MathF.Max(data.Far, camera.NearZ + 0.001f);
            camera.ClearColor = data.Clear == CameraClear.Color ? data.Background : new Color(0.0f, 0.0f, 0.0f, 1.0f);
            SceneViews.SetCamera(_view, &camera);
            float orthographic = data.Projection == CameraProjection.Orthographic ? MathF.Max(data.OrthographicSize, 0.001f) : 0.0f;
            if (orthographic != _sentOrthographic)
            {
                SceneViews.SetProjection(_view, orthographic);
                _sentOrthographic = orthographic;
            }
        }

        public void Release()
        {
            if (_view != 0)
                SceneViews.Destroy(_view);
            _view = 0;
            _sentOrthographic = -1.0f;
        }
    }

    // Camera frustums drawn over the Scene view: every camera with Show frustum (dimmed), the selected one bright; the far
    // end is cut at DrawDepth.
    public static class CameraGizmos
    {
        // Frustums are drawn at most this deep past the near plane, so a far plane of 1000 doesn't cover the view.
        private const float DrawDepth = 10.0f;

        public static void Draw(Rect viewRect, Vector3 eye, Vector3 target, Vector3 up, float fovY)
        {
            Span<CameraData> cameras = ComponentData.All<CameraData>(out ReadOnlySpan<ActorID> owners);
            if (cameras.Length == 0)
                return;
            ActorID selected = Selection.ActiveActor;
            ViewProjector projector = new(viewRect, eye, target, up, fovY);
            Painter draw = Gui.Draw;
            for (int i = 0; i < cameras.Length; i++)
            {
                bool isSelected = owners[i].Value == selected.Value;
                if ((cameras[i].Flags & CameraFlags.ShowFrustum) == 0 && !isSelected)
                    continue;
                if (!Transform.Exists(owners[i]))
                    continue;
                Color32 color = isSelected ? new Color32(111, 179, 232, 255) : new Color32(111, 179, 232, 110);
                DrawFrustum(draw, projector, Transform.Of(owners[i]).World, cameras[i], color);
            }
        }

        private static void DrawFrustum(Painter draw, in ViewProjector projector, in TransformData world, in CameraData camera, Color32 color)
        {
            Camera.ViewBasis(world, out Vector3 forward, out Vector3 up);
            Vector3 right = new(up.y * forward.z - up.z * forward.y, up.z * forward.x - up.x * forward.z, up.x * forward.y - up.y * forward.x);
            const float aspect = 16.0f / 9.0f;
            float near = MathF.Max(camera.Near, 0.0001f);
            float far = MathF.Min(MathF.Max(camera.Far, near + 0.001f), near + DrawDepth);
            Span<Vector3> corners = stackalloc Vector3[8];
            for (int plane = 0; plane < 2; plane++)
            {
                float distance = plane == 0 ? near : far;
                float halfHeight = camera.Projection == CameraProjection.Orthographic
                    ? MathF.Max(camera.OrthographicSize, 0.001f)
                    : MathF.Tan(camera.VerticalFovRadians(aspect) * 0.5f) * distance;
                float halfWidth = halfHeight * aspect;
                for (int c = 0; c < 4; c++)
                {
                    float sx = (c == 1 || c == 2) ? 1.0f : -1.0f;
                    float sy = c >= 2 ? 1.0f : -1.0f;
                    corners[plane * 4 + c] = new Vector3(
                        world.PositionX + forward.x * distance + right.x * halfWidth * sx + up.x * halfHeight * sy,
                        world.PositionY + forward.y * distance + right.y * halfWidth * sx + up.y * halfHeight * sy,
                        world.PositionZ + forward.z * distance + right.z * halfWidth * sx + up.z * halfHeight * sy);
                }
            }
            for (int c = 0; c < 4; c++)
            {
                Segment(draw, projector, corners[c], corners[(c + 1) % 4], color);
                Segment(draw, projector, corners[4 + c], corners[4 + (c + 1) % 4], color);
                Segment(draw, projector, corners[c], corners[4 + c], color);
            }
        }

        private static void Segment(Painter draw, in ViewProjector projector, Vector3 a, Vector3 b, Color32 color)
        {
            if (projector.Clip(ref a, ref b) && projector.Project(a, out Vector2 pa) && projector.Project(b, out Vector2 pb))
                draw.Line(pa, pb, color, 1.5f);
        }
    }

    // Projects world points into a view rect for a look-at camera (perspective, vertical field of view), with near-plane
    // clipping for segments.
    public readonly struct ViewProjector
    {
        private const float NearPlane = 0.01f;
        private readonly Rect _rect;
        private readonly Vector3 _eye;
        private readonly Vector3 _forward;
        private readonly Vector3 _right;
        private readonly Vector3 _up;
        private readonly float _tan;

        public ViewProjector(Rect rect, Vector3 eye, Vector3 target, Vector3 up, float fovY)
        {
            _rect = rect;
            _eye = eye;
            _forward = Normalize(new Vector3(target.x - eye.x, target.y - eye.y, target.z - eye.z));
            _right = Normalize(Cross(up, _forward));
            _up = Cross(_forward, _right);
            _tan = MathF.Tan(fovY * 0.5f);
        }

        private float Depth(Vector3 p) => Dot(new Vector3(p.x - _eye.x, p.y - _eye.y, p.z - _eye.z), _forward);

        // Cuts the segment to the part in front of the camera; false when none is.
        public bool Clip(ref Vector3 a, ref Vector3 b)
        {
            float da = Depth(a), db = Depth(b);
            if (da < NearPlane && db < NearPlane)
                return false;
            if (da < NearPlane || db < NearPlane)
            {
                float t = (NearPlane - da) / (db - da);
                Vector3 cut = new(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t);
                if (da < NearPlane)
                    a = cut;
                else
                    b = cut;
            }
            return true;
        }

        public bool Project(Vector3 p, out Vector2 screen)
        {
            screen = default;
            Vector3 d = new(p.x - _eye.x, p.y - _eye.y, p.z - _eye.z);
            float z = Dot(d, _forward);
            if (z < NearPlane * 0.5f || _rect.Height <= 0.0f)
                return false;
            float aspect = _rect.Width / _rect.Height;
            float nx = Dot(d, _right) / (z * _tan * aspect);
            float ny = Dot(d, _up) / (z * _tan);
            screen = new Vector2(_rect.min.x + (nx * 0.5f + 0.5f) * _rect.Width, _rect.min.y + (0.5f - ny * 0.5f) * _rect.Height);
            return true;
        }

        private static Vector3 Cross(Vector3 a, Vector3 b) => new(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
        private static float Dot(Vector3 a, Vector3 b) => a.x * b.x + a.y * b.y + a.z * b.z;

        private static Vector3 Normalize(Vector3 v)
        {
            float length = MathF.Sqrt(Dot(v, v));
            return length > 0.0f ? new Vector3(v.x / length, v.y / length, v.z / length) : new Vector3(0.0f, 0.0f, 1.0f);
        }
    }
}
