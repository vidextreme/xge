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

    // What the Rename dialog renames: Title ("Rename Actor"), the dim line under it, the starting name and a read-only
    // extension, plus delegates for the preview (48 px square), validation (null = fine) and the rename itself (null =
    // done, otherwise the error to show). Assets get theirs from RenameDialog.Open(path); other callers build one.
    public sealed class RenameRequest
    {
        public string Title { get; init; } = "Rename";
        public string Context { get; init; } = string.Empty;
        public string Name { get; init; } = string.Empty;
        public string Extension { get; init; } = string.Empty;
        public Action<Painter, Rect>? DrawPreview { get; init; }
        public Func<string, string?>? Validate { get; init; }
        public required Func<string, string?> Commit { get; init; }
    }

    // The editor's Rename dialog, usable from any panel or tool: RenameDialog.Open(path) for an asset or folder (title
    // "Rename <kind>", its thumbnail, the name without its extension and the extension read-only; renames through
    // AssetDatabase.Rename, the file and its meta), or RenameDialog.Open(request) for anything else (actors). The shell
    // draws it every frame (OnGUI from EditorShell).
    public static class RenameDialog
    {
        private const string ModalId = "xg_rename";
        private const float Width = 400.0f;
        private const float ThumbnailSize = 48.0f;

        private static readonly TextBuffer _text = new(256);
        private static Func<RenameRequest?>? _pending;
        private static int _delay;
        private static bool _open;
        private static RenameRequest? _request;
        private static string? _error;
        private static bool _setText;

        public static bool IsOpen => _request is not null;

        public static void Open(string relPath, RenameOptions? options = null)
        {
            string path = AssetIndex.Normalize(relPath);
            RenameOptions settings = options ?? new RenameOptions();
            Open(() => AssetRequest(path, settings), settings.DelayFrames);
        }

        public static void Open(RenameRequest request, int delayFrames = 0) => Open(() => request, delayFrames);

        private static void Open(Func<RenameRequest?> request, int delayFrames)
        {
            _pending = request;
            _delay = Math.Max(delayFrames, 0);
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
            using var modal = Gui.Modal(ModalId, _request?.Title ?? "Rename", new Vector2(Width, 0.0f));
            if (!modal.IsOpen)
            {
                _request = null;
                return;
            }
            if (_request is not RenameRequest request)
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
                    request.DrawPreview?.Invoke(thumb.Draw, Rect.FromXYWH(4.0f, 4.0f, ThumbnailSize - 8.0f, ThumbnailSize - 8.0f));
                    thumb.Draw.Rect(rect, Style.Color(StyleColor.Border), 4.0f);
                }
                using (Gui.Vertical())
                    DrawBody(request);
            }
        }

        private static void Begin()
        {
            Func<RenameRequest?> make = _pending!;
            _pending = null;
            if (make() is not RenameRequest request)
                return;
            _request = request;
            _text.Text = request.Name;
            _error = null;
            _setText = true;
            _open = true;
        }

        // An asset or folder: kind title, thumbnail, display path, the extension kept (unless options say otherwise).
        private static RenameRequest? AssetRequest(string path, RenameOptions options)
        {
            AssetEntry? entry = ProjectService.Assets?.Index.Find(path);
            if (entry is null)
            {
                Debug.LogWarning($"[Assets] Can't rename '{path}': it isn't in the project");
                return null;
            }
            string extension = options.KeepExtension ? entry.DisplayExtension : string.Empty;
            return new RenameRequest
            {
                Title = "Rename " + AssetIndex.LabelOf(entry.Kind),
                Context = AssetNames.DisplayPath(entry.RelPath, entry.IsFolder),
                Name = options.KeepExtension ? entry.DisplayName : entry.Name,
                Extension = extension,
                DrawPreview = (draw, rect) => AssetThumbnails.Draw(draw, rect, entry, 28.0f),
                Validate = name => AssetDatabase.ValidateName(name + extension),
                Commit = name =>
                {
                    if (ProjectService.Assets is not { } assets)
                        return "No project is open";
                    string newName = name + extension;
                    if (assets.Rename(path, newName) is string failed)
                        return failed;
                    string newPath = (entry.Parent.Length > 0 ? entry.Parent + "/" : string.Empty) + newName;
                    Debug.Log($"[Debug] Rename {path} -> {newPath}");
                    options.Renamed?.Invoke(newPath);
                    return null;
                },
            };
        }

        private static void DrawBody(RenameRequest request)
        {
            if (request.Context.Length > 0)
            {
                using (Gui.Font(FontStyle.Mono))
                    Gui.Label(request.Context);
            }
            using (Gui.Horizontal(0.0f))
            {
                if (_setText)
                    Gui.FocusNext();
                TextFieldFlags flags = _setText ? TextFieldFlags.SetText : TextFieldFlags.None;
                _setText = false;
                if ((Gui.TextField("##rename_name", _text, "New name", 0.0f, flags) & Result.Changed) != 0)
                    _error = Check(request, _text.Text.Trim());
                if (request.Extension.Length > 0)
                    DrawExtension(request.Extension);
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
                    _request = null;
                    return;
                }
                commit |= Gui.Button("Rename");
            }
            if (commit)
                Commit(request);
        }

        private static string? Check(RenameRequest request, string name) =>
            name.Length == 0 ? "Enter a name" : request.Validate?.Invoke(name);

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

        private static void Commit(RenameRequest request)
        {
            string name = _text.Text.Trim();
            if (Check(request, name) is string invalid)
            {
                _error = invalid;
                return;
            }
            if (request.Commit(name) is string failed)
            {
                _error = failed;
                return;
            }
            Gui.ClosePopup();
            _request = null;
        }
    }
}
