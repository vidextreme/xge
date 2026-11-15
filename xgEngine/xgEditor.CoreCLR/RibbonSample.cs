// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // Sample Ribbon with placeholder commands (the tool system will provide the real ones). Clicks log to the Console.
    public sealed class RibbonPanel : Panel
    {
        private enum ViewLayout { Single, Columns, Rows, LeftSplit, Quad, TopSplit }

        private readonly Ribbon _ribbon = new();
        private int _tool = 1;
        private ViewLayout _layout = ViewLayout.Single;
        private bool _liveUpdate = true;
        private bool _playing;
        private bool _autoBuild = true;
        private bool _showGrid = true;
        private bool _showCameras;
        private bool _showLights = true;
        private bool _showAudio;
        private bool _hideGizmos;

        public override string Title => "Tools";
        public override string? MenuPath => "General/Tools";
        public override DockSlot Slot =>
            Environment.GetEnvironmentVariable("XG_UI_TEST_RIBBON_DOCK") == "bottom" ? DockSlot.Bottom : DockSlot.Left;
        public override Icon PanelIcon => Icon.Gizmo;
        public override PanelFlags Flags => PanelFlags.NoPadding | PanelFlags.NoScrollbar;

        public Ribbon Ribbon => _ribbon;

        public RibbonPanel()
        {
            Color32 objectColor = Style.Color(StyleColor.IconObject);
            Color32 dataColor = Style.Color(StyleColor.IconData);
            Color32 componentColor = Style.Color(StyleColor.IconComponent);
            Color32 materialColor = Style.Color(StyleColor.IconMaterial);
            Color32 warningColor = Style.Color(StyleColor.IconWarning);

            RibbonCategory scene = Category("Scene", Icon.Scene, null);
            RibbonGroup file = Group(scene, "File");
            file.Items.Add(Wide("Save scene", Icon.Data, dataColor, enabled: () => false));
            file.Items.Add(Wide("Reload scene", Icon.Scene, objectColor));
            file.Items.Add(Wide("Export selection", Icon.Folder, warningColor));
            file.Items.Add(Wide("Scene settings", Icon.Inspector, null));
            file.Items.Add(Wide("Revert scene", Icon.Scene, null));
            file.Items.Add(Wide("Save as...", Icon.Data, dataColor));

            RibbonGroup transform = Group(scene, "Transform", 4);
            Icon[] tools = { Icon.Hand, Icon.Move, Icon.Rotate, Icon.Scale, Icon.Rect, Icon.Transform };
            string[] toolNames = { "View", "Move", "Rotate", "Scale", "Rect", "Transform" };
            for (int i = 0; i < tools.Length; i++)
            {
                int index = i;
                transform.Items.Add(Cell(toolNames[i], tools[i], null, () => _tool = index, toggled: () => _tool == index));
            }

            RibbonGroup layouts = Group(scene, "Viewport layout", 3);
            foreach (ViewLayout layout in Enum.GetValues<ViewLayout>())
            {
                ViewLayout value = layout;
                RibbonItem item = Cell(layout.ToString(), Icon.None, null, () => _layout = value, toggled: () => _layout == value);
                item.DrawIcon = (draw, rect, color) => DrawLayoutGlyph(draw, rect, color, value);
                layouts.Items.Add(item);
            }

            RibbonGroup simulation = Group(scene, "Simulation");
            simulation.Items.Add(Wide("Play in editor", Icon.Play, componentColor, () => _playing = !_playing, toggled: () => _playing));
            RibbonItem live = Wide("Live update", Icon.Effects, warningColor, () => _liveUpdate = !_liveUpdate, toggled: () => _liveUpdate);
            live.HasOptions = true;
            simulation.Items.Add(live);
            simulation.Items.Add(Wide("Step frame", Icon.Step, null, enabled: () => _playing));

            RibbonGroup gizmos = Group(scene, "Gizmos", 4);
            gizmos.Width = 130.0f;
            gizmos.Items.Add(Cell("Grid", Icon.SceneGrid, null, () => _showGrid = !_showGrid, toggled: () => _showGrid));
            gizmos.Items.Add(Cell("Cameras", Icon.Camera, objectColor, () => _showCameras = !_showCameras, toggled: () => _showCameras));
            RibbonItem lights = Cell("Lights", Icon.Light, warningColor, () => _showLights = !_showLights, toggled: () => _showLights);
            lights.HasOptions = true;
            gizmos.Items.Add(lights);
            gizmos.Items.Add(Cell("Audio", Icon.Audio, materialColor, () => _showAudio = !_showAudio, toggled: () => _showAudio));
            gizmos.Items.Add(Wide("Hide all gizmos", Icon.EyeOff, null, () => _hideGizmos = !_hideGizmos, toggled: () => _hideGizmos));

            RibbonGroup debug = Group(scene, "Debug");
            debug.Collapsed = true;
            debug.Items.Add(Wide("Frame stats", Icon.Stats, dataColor));
            debug.Items.Add(Wide("Physics shapes", Icon.Physics, componentColor));
            debug.Items.Add(Wide("Wireframe", Icon.Wire, null));

            RibbonCategory create = Category("Create", Icon.Plus, dataColor);
            RibbonGroup primitives = Group(create, "Primitives", 3);
            primitives.Items.Add(Cell("Cube", Icon.Mesh, objectColor));
            primitives.Items.Add(Cell("Sphere", Icon.Sphere, objectColor));
            primitives.Items.Add(Cell("Plane", Icon.SceneGrid, objectColor));
            RibbonGroup objects = Group(create, "Objects");
            objects.Items.Add(Wide("Empty actor", Icon.Object, null));
            objects.Items.Add(Wide("Camera", Icon.Camera, objectColor));
            objects.Items.Add(Wide("Light", Icon.Light, warningColor));
            objects.Items.Add(Wide("Audio source", Icon.Audio, materialColor, enabled: () => false));
            RibbonGroup assets = Group(create, "Assets");
            assets.Items.Add(Wide("Material", Icon.Material, materialColor));
            assets.Items.Add(Wide("Script", Icon.Script, componentColor));
            assets.Items.Add(Wide("Folder", Icon.Folder, warningColor));

            RibbonCategory build = Category("Build", Icon.Script, componentColor);
            RibbonGroup scripts = Group(build, "Scripts");
            scripts.Items.Add(Wide("Build scripts", Icon.Script, componentColor));
            scripts.Items.Add(Wide("Open C# project", Icon.Project, null));
            RibbonItem auto = Wide("Auto build", Icon.Branch, null, () => _autoBuild = !_autoBuild, toggled: () => _autoBuild);
            auto.HasOptions = true;
            scripts.Items.Add(auto);
            RibbonGroup platforms = Group(build, "Platforms");
            platforms.Width = 170.0f;
            platforms.Items.Add(Wide("Windows", Icon.Game, objectColor));
            platforms.Items.Add(Wide("Linux", Icon.Game, objectColor));
            platforms.Items.Add(Wide("macOS", Icon.Game, null, enabled: () => false));
            platforms.Items.Add(Wide("Android", Icon.Game, null, enabled: () => false));
            platforms.Items.Add(Wide("Package", Icon.Folder, warningColor));
            platforms.Items.Add(Wide("Build settings", Icon.Inspector, null));
            RibbonGroup bake = Group(build, "Bake", 3);
            bake.Items.Add(Cell("Lighting", Icon.Lighting, warningColor));
            bake.Items.Add(Cell("Navigation", Icon.Globe, componentColor));
            bake.Items.Add(Cell("Physics", Icon.Physics, componentColor));
            bake.Items.Add(Cell("Reflections", Icon.Sphere, objectColor));
            bake.Items.Add(Cell("Occlusion", Icon.EyeOff, null));
            bake.Items.Add(Cell("Previews", Icon.Image, materialColor));
            RibbonGroup deploy = Group(build, "Deploy");
            deploy.Items.Add(Wide("Deploy to device", Icon.Cloud, objectColor));

            RibbonCategory view = Category("View", Icon.SceneGrid, objectColor);
            RibbonGroup panels = Group(view, "Panels");
            (string Name, Icon Icon)[] panelItems =
            {
                ("Hierarchy", Icon.Hierarchy), ("Inspector", Icon.Inspector), ("Console", Icon.Console), ("Project", Icon.Project),
                ("Lighting", Icon.Lighting), ("Game view", Icon.Game), ("Scene view", Icon.Scene), ("Statistics", Icon.Stats),
            };
            foreach ((string name, Icon icon) in panelItems)
                panels.Items.Add(Wide(name, icon, null));
            RibbonGroup overlays = Group(view, "Overlays", 4);
            Icon[] overlayIcons = { Icon.Gizmo, Icon.Magnet, Icon.PivotCenter, Icon.PivotLocal, Icon.Globe, Icon.TwoD, Icon.Wire, Icon.Star };
            foreach (Icon icon in overlayIcons)
                overlays.Items.Add(Cell(icon.ToString(), icon, null));
            RibbonGroup windows = Group(view, "Windows");
            windows.Items.Add(Wide("Reset layout", Icon.WinRestore, null));
            windows.Items.Add(Wide("Maximize view", Icon.WinMaximize, null));
            windows.Items.Add(Wide("Account", Icon.Account, null));
            windows.Items.Add(Wide("Search commands", Icon.Search, null));
        }

        protected override void OnGUI() => _ribbon.OnGUI();

        private RibbonCategory Category(string name, Icon icon, Color32? color)
        {
            RibbonCategory category = new() { Name = name, Icon = icon, IconColor = color };
            _ribbon.Categories.Add(category);
            return category;
        }

        private static RibbonGroup Group(RibbonCategory category, string name, int iconColumns = 4)
        {
            RibbonGroup group = new() { Name = name, IconColumns = iconColumns };
            category.Groups.Add(group);
            return group;
        }

        private static RibbonItem Wide(string label, Icon icon, Color32? color, Action? clicked = null, Func<bool>? enabled = null, Func<bool>? toggled = null) =>
            Item(RibbonItemStyle.Wide, label, icon, color, clicked, enabled, toggled);

        private static RibbonItem Cell(string label, Icon icon, Color32? color, Action? clicked = null, Func<bool>? enabled = null, Func<bool>? toggled = null) =>
            Item(RibbonItemStyle.Icon, label, icon, color, clicked, enabled, toggled);

        private static RibbonItem Item(RibbonItemStyle style, string label, Icon icon, Color32? color, Action? clicked, Func<bool>? enabled, Func<bool>? toggled) => new()
        {
            Style = style,
            Label = label,
            Icon = icon,
            IconColor = color,
            IsEnabled = enabled,
            IsToggled = toggled,
            Clicked = () =>
            {
                clicked?.Invoke();
                Debug.Log($"[Ribbon] {label}");
            },
        };

        private static void DrawLayoutGlyph(Painter draw, Rect rect, Color32 color, ViewLayout layout)
        {
            Rect r = Rect.FromCenterSize(rect.Center, new Vector2(14.0f, 12.0f));
            float midX = r.min.x + r.Width * 0.5f;
            float midY = r.min.y + r.Height * 0.5f;
            float thirdX = r.min.x + r.Width * 0.45f;
            float thirdY = r.min.y + r.Height * 0.45f;
            draw.Rect(r, color, 1.0f);
            switch (layout)
            {
                case ViewLayout.Columns:
                    draw.Line(new Vector2(midX, r.min.y), new Vector2(midX, r.max.y), color);
                    break;
                case ViewLayout.Rows:
                    draw.Line(new Vector2(r.min.x, midY), new Vector2(r.max.x, midY), color);
                    break;
                case ViewLayout.LeftSplit:
                    draw.Line(new Vector2(thirdX, r.min.y), new Vector2(thirdX, r.max.y), color);
                    draw.Line(new Vector2(thirdX, midY), new Vector2(r.max.x, midY), color);
                    break;
                case ViewLayout.Quad:
                    draw.Line(new Vector2(midX, r.min.y), new Vector2(midX, r.max.y), color);
                    draw.Line(new Vector2(r.min.x, midY), new Vector2(r.max.x, midY), color);
                    break;
                case ViewLayout.TopSplit:
                    draw.Line(new Vector2(r.min.x, thirdY), new Vector2(r.max.x, thirdY), color);
                    draw.Line(new Vector2(midX, thirdY), new Vector2(midX, r.max.y), color);
                    break;
            }
        }
    }
}
