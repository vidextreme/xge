// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // What the Save As dialog saves: Title ("Save Scene As"), the dim context line, the starting name, the extension
    // (read-only chip), the starting folder, a 48 px preview, and Commit(path) (null = saved, otherwise the error to
    // show). Existing files are refused unless AllowOverwrite.
    public sealed class SaveAsRequest
    {
        public string Title { get; init; } = "Save As";
        public string Context { get; init; } = string.Empty;
        public string Name { get; init; } = string.Empty;
        public string Extension { get; init; } = string.Empty;
        public string Folder { get; init; } = AssetIndex.RootFolder;
        public bool AllowOverwrite { get; init; }
        public Action<Painter, Rect>? DrawPreview { get; init; }
        public required Func<string, string?> Commit { get; init; }
    }

    // The editor's Save As dialog, usable from any editor: SaveAsDialog.Open(request). Same look as RenameDialog: the
    // preview box, the context line, the name with the extension chip, the folder (FolderField: change it or create a
    // folder), the target path, errors, then Cancel / Save. The shell draws it every frame.
    public static class SaveAsDialog
    {
        private const string ModalId = "xg_save_as";
        private const float Width = 420.0f;
        private const float PreviewSize = 48.0f;
        private const float ButtonWidth = 72.0f;

        private static readonly TextBuffer _name = new(256);
        private static readonly FolderField _folderField = new();
        private static SaveAsRequest? _pending;
        private static int _delay;
        private static SaveAsRequest? _request;
        private static string _folder = AssetIndex.RootFolder;
        private static string? _error;
        private static bool _open;
        private static bool _setText;

        public static bool IsOpen => _request is not null;
        public static FolderField Folder => _folderField;

        public static void Open(SaveAsRequest request, int delayFrames = 0)
        {
            _pending = request;
            _delay = Math.Max(delayFrames, 0);
        }

        public static void OnGUI()
        {
            if (_pending is not null && (_delay <= 0 || --_delay == 0))
            {
                _request = _pending;
                _pending = null;
                _name.Text = _request.Name;
                _folder = AssetIndex.Normalize(_request.Folder.Length > 0 ? _request.Folder : AssetIndex.RootFolder);
                _error = null;
                _setText = true;
                _open = true;
            }
            if (_open)
            {
                _open = false;
                Gui.OpenPopup(ModalId);
            }
            using var modal = Gui.Modal(ModalId, _request?.Title ?? "Save As", new Vector2(Width, 0.0f));
            if (!modal.IsOpen)
            {
                _request = null;
                return;
            }
            if (_request is not SaveAsRequest request)
            {
                Gui.ClosePopup();
                return;
            }
            using (Gui.Horizontal(10.0f))
            {
                using (var preview = Gui.Canvas("##save_preview", new Vector2(PreviewSize, PreviewSize)))
                {
                    Rect rect = Rect.FromXYWH(0.0f, 0.0f, PreviewSize, PreviewSize);
                    preview.Draw.RectFilled(rect, Style.Color(StyleColor.SegmentBg), 4.0f);
                    request.DrawPreview?.Invoke(preview.Draw, Rect.FromXYWH(4.0f, 4.0f, PreviewSize - 8.0f, PreviewSize - 8.0f));
                    preview.Draw.Rect(rect, Style.Color(StyleColor.Border), 4.0f);
                }
                using (Gui.Vertical())
                    DrawBody(request);
            }
        }

        private static string TargetPath(SaveAsRequest request) => _folder + "/" + _name.Text.Trim() + request.Extension;

        private static string? Check(SaveAsRequest request)
        {
            string name = _name.Text.Trim();
            if (AssetDatabase.ValidateName(name + request.Extension) is string invalid)
                return name.Length == 0 ? "Enter a name" : invalid;
            if (!request.AllowOverwrite && ProjectService.Assets is { } assets && File.Exists(assets.FullPath(TargetPath(request))))
                return $"{name}{request.Extension} exists, choose another name";
            return null;
        }

        private static void DrawBody(SaveAsRequest request)
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
                if ((Gui.TextField("##save_name", _name, "Name", 0.0f, flags) & Result.Changed) != 0)
                    _error = Check(request);
                if (request.Extension.Length > 0)
                    DrawChip(request.Extension);
            }
            if (_folderField.Field("save_folder", "Folder", ref _folder))
                _error = Check(request);
            using (Gui.Font(FontStyle.Mono))
            {
                string target = "→ " + TargetPath(request);
                using var line = Gui.Canvas("##save_target", new Vector2(0.0f, 18.0f));
                line.Draw.Text(new Vector2(0.0f, 2.0f), Style.Color(StyleColor.TextDim), target);
            }
            if (_error is not null)
            {
                using var error = Gui.Canvas("##save_error", new Vector2(0.0f, 18.0f));
                error.Draw.Icon(Icon.Warning, new Vector2(7.0f, 9.0f), 12.0f, Style.Color(StyleColor.IconWarning));
                error.Draw.Text(new Vector2(18.0f, 2.0f), Style.Color(StyleColor.IconWarning), _error);
            }
            Gui.Space(4.0f);
            bool keys = !_folderField.IsOpen;
            bool save = keys && Gui.KeyPressed(Key.Enter, false);
            bool cancel = keys && Gui.KeyPressed(Key.Escape, false);
            float width = MathF.Max(Gui.AvailableSize.x, ButtonWidth * 2.0f + 4.0f);
            using (var buttons = Gui.Canvas("##save_buttons", new Vector2(width, 22.0f)))
            {
                Painter draw = buttons.Draw;
                cancel |= ToggleButton.Draw(draw, "##save_cancel", Rect.FromXYWH(width - ButtonWidth * 2.0f - 4.0f, 0.0f, ButtonWidth, 22.0f), Icon.None, null,
                    "Cancel", null, null, false, true, ToggleDot.None).Clicked();
                save |= ToggleButton.Draw(draw, "##save_save", Rect.FromXYWH(width - ButtonWidth, 0.0f, ButtonWidth, 22.0f), Icon.Save, null,
                    "Save", null, TargetPath(request), true, true, ToggleDot.None).Clicked();
            }
            if (cancel)
            {
                Gui.ClosePopup();
                _request = null;
                return;
            }
            if (!save)
                return;
            _error = Check(request);
            if (_error is not null)
                return;
            if (request.Commit(TargetPath(request)) is string failed)
            {
                _error = failed;
                return;
            }
            Gui.ClosePopup();
            _request = null;
        }

        private static void DrawChip(string extension)
        {
            float height = MathF.Round(Gui.MeasureText("Ag").y) + 8.0f;
            float width;
            using (Gui.Font(FontStyle.Mono))
                width = MathF.Ceiling(Gui.MeasureText(extension).x) + 16.0f;
            using var canvas = Gui.Canvas("##save_ext", new Vector2(width, height));
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
    }
}
