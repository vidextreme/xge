// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // The scene modals, drawn by the shell every frame: Save scene as (the shared SaveAsDialog, in the scene's folder
    // or Assets/Scenes), Open scene (the project's .xcs files) and the unsaved-changes prompt that guards New and Open. Actors are
    // renamed through the shared RenameDialog (SceneContextMenu.RenameActor).
    public static class SceneDialogs
    {
        private const string OpenId = "scene_open";
        private const string UnsavedId = "scene_unsaved";
        private const float RowHeight = 22.0f;

        private static readonly TextBuffer _search = new(64);
        private static Action? _pending;
        private static string? _openRequest;

        // Save scene as through the shared SaveAsDialog: the scene's own folder, else Assets/Scenes when it exists.
        public static void OpenSaveAs(EditorScene scene, Action? after = null)
        {
            string folder = scene.Path is string current && current.Contains('/') ? current[..current.LastIndexOf('/')]
                : ProjectService.Assets?.Index.IsDirectory(AssetIndex.RootFolder + "/Scenes") == true ? AssetIndex.RootFolder + "/Scenes" : AssetIndex.RootFolder;
            int members = scene.Members.Count;
            SaveAsDialog.Open(new SaveAsRequest
            {
                Title = "Save Scene As",
                Context = $"{scene.Name} · {members} actor{(members == 1 ? "" : "s")}",
                Name = scene.Path is null ? "New Scene" : AssetNames.DisplayName(scene.Path),
                Extension = SceneAsset.Extension,
                Folder = folder,
                AllowOverwrite = false,
                DrawPreview = (draw, rect) => draw.Icon(Icon.Scene, rect.Center, 26.0f, Style.Color(StyleColor.IconWarning)),
                Commit = path =>
                {
                    if (!SceneManager.SaveAs(scene, path))
                        return "The scene couldn't be saved (see the Console)";
                    after?.Invoke();
                    return null;
                },
            });
        }

        public static void OpenPicker()
        {
            _search.Text = string.Empty;
            _openRequest = OpenId;
        }

        // Runs the action now, or after the user saved or discarded the loaded scenes' unsaved changes.
        public static void Guard(Action action)
        {
            if (PlayMode.IsActive)
            {
                Debug.LogWarning("[Scenes] Stop play mode first");
                return;
            }
            foreach (EditorScene scene in SceneManager.Scenes)
                SceneManager.Touch(scene);
            SceneManager.Update();
            if (!SceneManager.AnyDirty)
            {
                action();
                return;
            }
            _pending = action;
            _openRequest = UnsavedId;
        }

        public static void OnGUI()
        {
            if (_openRequest is string request)
            {
                _openRequest = null;
                Gui.OpenPopup(request);
            }
            DrawOpen();
            DrawUnsaved();
        }

        private static void DrawOpen()
        {
            using var modal = Gui.Modal(OpenId, "Open scene", new Vector2(360.0f, 0.0f));
            if (!modal.IsOpen)
                return;
            Gui.SearchField("##scene_search", _search, 0.0f);
            string filter = _search.Text.Trim();
            List<AssetChoice> scenes = (AssetBrowser.Current?.List("scene") ?? Array.Empty<AssetChoice>())
                .Where(c => SceneAsset.IsScene(c.Path) && (filter.Length == 0 || c.Path.Contains(filter, StringComparison.OrdinalIgnoreCase))).ToList();
            if (scenes.Count == 0)
                Gui.Label("No scenes in the project yet.");
            float height = scenes.Count * (RowHeight + 2.0f) + 4.0f;
            using (var list = Gui.ScrollArea("##scene_list", new Vector2(0.0f, MathF.Min(MathF.Max(height, 4.0f), 260.0f))))
            {
                if (list.IsOpen)
                {
                    float width = MathF.Max(Gui.AvailableSize.x, 1.0f);
                    using var canvas = Gui.Canvas("##scene_items", new Vector2(width, height));
                    Painter draw = canvas.Draw;
                    for (int i = 0; i < scenes.Count; i++)
                    {
                        Rect row = Rect.FromXYWH(2.0f, 2.0f + i * (RowHeight + 2.0f), width - 4.0f, RowHeight);
                        Result hit;
                        using (Gui.Id(i))
                            hit = Gui.HitArea("##scene_item", row);
                        Gui.Tooltip(scenes[i].Path);
                        draw.RectFilled(row, Style.Color(hit.Hovered() ? StyleColor.ButtonHover : StyleColor.Button), 3.0f);
                        float cy = row.min.y + RowHeight * 0.5f;
                        draw.Icon(Icon.Scene, new Vector2(row.min.x + 12.0f, cy), 13.0f, Style.Color(StyleColor.IconWarning));
                        draw.Text(new Vector2(row.min.x + 26.0f, MathF.Round(cy - Gui.MeasureText(scenes[i].Name).y * 0.5f)), Style.Color(StyleColor.Text),
                            AssetNames.DisplayName(scenes[i].Path));
                        if (hit.Clicked())
                        {
                            string path = scenes[i].Path;
                            Gui.CloseModal();
                            Guard(() => SceneManager.Open(path));
                        }
                    }
                }
            }
            using (Gui.Horizontal())
            {
                Gui.FlexibleSpace();
                if (Gui.Button("Cancel"))
                    Gui.CloseModal();
            }
        }

        private static void DrawUnsaved()
        {
            using var modal = Gui.Modal(UnsavedId, "Unsaved scene", new Vector2(360.0f, 0.0f));
            if (!modal.IsOpen || _pending is not Action pending)
                return;
            string names = string.Join(", ", SceneManager.Scenes.Where(s => s.IsDirty).Select(s => s.Name));
            Gui.Label($"{names} has unsaved changes.");
            using (Gui.Horizontal())
            {
                Gui.FlexibleSpace();
                if (Gui.Button("Cancel"))
                {
                    _pending = null;
                    Gui.CloseModal();
                }
                if (Gui.Button("Discard"))
                {
                    _pending = null;
                    Gui.CloseModal();
                    pending();
                }
                if (Gui.Button("Save"))
                {
                    _pending = null;
                    Gui.CloseModal();
                    EditorScene? untitled = SceneManager.Scenes.FirstOrDefault(s => s.IsDirty && s.Path is null);
                    foreach (EditorScene scene in SceneManager.Scenes.Where(s => s.IsDirty && s.Path is not null).ToArray())
                        SceneManager.Save(scene);
                    if (untitled is not null)
                        OpenSaveAs(untitled, pending);
                    else
                        pending();
                }
            }
        }
    }
}
