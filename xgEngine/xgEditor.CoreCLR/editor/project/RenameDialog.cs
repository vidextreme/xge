// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;
using xg.math;
using Debug = xg.Debug;

namespace xgEditor.CoreCLR
{
    public sealed class RenameOptions
    {
        // Keep the file's extension: the user edits only the name and sees the extension read-only. Code that needs to
        // change the extension sets this to false (the field then holds the whole file name).
        public bool KeepExtension { get; init; } = true;
        // Called with the new project-relative path after a successful rename.
        public Action<string>? Renamed { get; init; }
        // Frames to wait before opening (an asset created this frame isn't in the index yet).
        public int DelayFrames { get; init; }
    }

    // The editor's Rename dialog for an asset or folder, usable from any panel or tool: RenameDialog.Open(path). The
    // shell draws it every frame (OnGUI from EditorShell). Title "Rename <kind>", the item's thumbnail, the name without
    // its extension and the extension read-only; renames through AssetDatabase.Rename (the file and its meta).
    public static class RenameDialog
    {
        private const string ModalId = "xg_rename";
        private const float Width = 400.0f;
        private const float ThumbnailSize = 48.0f;

        private static readonly TextBuffer _text = new(256);
        private static string? _pending;
        private static RenameOptions _pendingOptions = new();
        private static int _delay;
        private static bool _open;
        private static string? _path;
        private static RenameOptions _options = new();
        private static string? _error;
        private static bool _setText;

        public static bool IsOpen => _path is not null;

        public static void Open(string relPath, RenameOptions? options = null)
        {
            _pending = AssetIndex.Normalize(relPath);
            _pendingOptions = options ?? new RenameOptions();
            _delay = Math.Max(_pendingOptions.DelayFrames, 0);
        }

        public static void OnGUI()
        {
            if (_pending is not null && (_delay <= 0 || --_delay == 0))
                Begin();
            if (_open)
            {
                _open = false;
                Gui.OpenPopup(ModalId);
            }
            AssetEntry? entry = _path is null ? null : ProjectService.Assets?.Index.Find(_path);
            string title = entry is null ? "Rename" : "Rename " + AssetIndex.LabelOf(entry.Kind);
            using var modal = Gui.Modal(ModalId, title, new Vector2(Width, 0.0f));
            if (!modal.IsOpen)
            {
                _path = null;
                return;
            }
            if (_path is null || entry is null)
            {
                Gui.ClosePopup();
                return;
            }
            using (Gui.Horizontal(10.0f))
            {
                using (var thumb = Gui.Canvas("##rename_thumb", new Vector2(ThumbnailSize, ThumbnailSize)))
                {
                    Rect rect = Rect.FromXYWH(0.0f, 0.0f, ThumbnailSize, ThumbnailSize);
                    thumb.Draw.RectFilled(rect, Style.Color(StyleColor.SegmentBg), 4.0f);
                    AssetThumbnails.Draw(thumb.Draw, Rect.FromXYWH(4.0f, 4.0f, ThumbnailSize - 8.0f, ThumbnailSize - 8.0f), entry, 28.0f);
                    thumb.Draw.Rect(rect, Style.Color(StyleColor.Border), 4.0f);
                }
                using (Gui.Vertical())
                    DrawBody(entry);
            }
        }

        private static void Begin()
        {
            string path = _pending!;
            _pending = null;
            AssetEntry? entry = ProjectService.Assets?.Index.Find(path);
            if (entry is null)
            {
                Debug.LogWarning($"[Assets] Can't rename '{path}': it isn't in the project");
                return;
            }
            _path = path;
            _options = _pendingOptions;
            _text.Text = _options.KeepExtension ? entry.DisplayName : entry.Name;
            _error = null;
            _setText = true;
            _open = true;
        }

        private static void DrawBody(AssetEntry entry)
        {
            string extension = _options.KeepExtension ? entry.DisplayExtension : string.Empty;
            using (Gui.Font(FontStyle.Mono))
                Gui.Label(AssetNames.DisplayPath(entry.RelPath, entry.IsFolder));
            using (Gui.Horizontal(0.0f))
            {
                if (_setText)
                    Gui.FocusNext();
                TextFieldFlags flags = _setText ? TextFieldFlags.SetText : TextFieldFlags.None;
                _setText = false;
                if ((Gui.TextField("##rename_name", _text, "New name", 0.0f, flags) & Result.Changed) != 0)
                    _error = AssetDatabase.ValidateName(_text.Text.Trim() + extension);
                if (extension.Length > 0)
                    DrawExtension(extension);
            }
            if (_error is not null)
            {
                using var error = Gui.Canvas("##rename_error", new Vector2(0.0f, 18.0f));
                error.Draw.Text(new Vector2(0.0f, 2.0f), Style.Color(StyleColor.IconWarning), _error);
            }
            Gui.Space(4.0f);
            bool commit = Gui.KeyPressed(Key.Enter, false);
            using (Gui.Horizontal())
            {
                Gui.FlexibleSpace();
                if (Gui.Button("Cancel") || Gui.KeyPressed(Key.Escape, false))
                {
                    Gui.ClosePopup();
                    _path = null;
                    return;
                }
                commit |= Gui.Button("Rename");
            }
            if (commit)
                Commit(entry, extension);
        }

        private static void DrawExtension(string extension)
        {
            float height = MathF.Round(Gui.MeasureText("Ag").y) + 8.0f;
            float width;
            using (Gui.Font(FontStyle.Mono))
                width = MathF.Ceiling(Gui.MeasureText(extension).x) + 16.0f;
            using var canvas = Gui.Canvas("##rename_ext", new Vector2(width, height));
            Rect rect = Rect.FromXYWH(0.0f, 0.0f, width, height);
            canvas.Draw.RectFilled(rect, Style.Color(StyleColor.SegmentBg), 3.0f);
            canvas.Draw.Rect(rect, Style.Color(StyleColor.Border), 3.0f);
            using (Gui.Font(FontStyle.Mono))
            {
                Vector2 size = Gui.MeasureText(extension);
                canvas.Draw.Text(new Vector2(8.0f, MathF.Round((height - size.y) * 0.5f)), Style.Color(StyleColor.TextDim), extension);
            }
            Gui.Tooltip("The extension stays the same");
        }

        private static void Commit(AssetEntry entry, string extension)
        {
            AssetDatabase? assets = ProjectService.Assets;
            if (assets is null || _path is null)
                return;
            string newName = _text.Text.Trim() + extension;
            if (AssetDatabase.ValidateName(newName) is string invalid)
            {
                _error = invalid;
                return;
            }
            string oldPath = _path;
            if (assets.Rename(oldPath, newName) is string failed)
            {
                _error = failed;
                return;
            }
            string newPath = (entry.Parent.Length > 0 ? entry.Parent + "/" : string.Empty) + newName;
            Debug.Log($"[Debug] Rename {oldPath} -> {newPath}");
            Gui.ClosePopup();
            _path = null;
            _options.Renamed?.Invoke(newPath);
        }
    }
}
