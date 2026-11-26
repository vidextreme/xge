// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.imui;
using xg.math;

namespace xgEditor.CoreCLR
{
    // What the confirmation dialog asks: Title ("Remove Component"), the bold Message, a dim Detail line, the 48 px preview
    // (default: a warning icon), the button labels and Confirm (runs on Yes).
    public sealed class ConfirmRequest
    {
        public string Title { get; init; } = "Confirm";
        public required string Message { get; init; }
        public string Detail { get; init; } = string.Empty;
        public string YesLabel { get; init; } = "Yes";
        public string NoLabel { get; init; } = "No";
        public Action<Painter, Rect>? DrawPreview { get; init; }
        public required Action Confirm { get; init; }
    }

    // The editor's Yes/No dialog, usable from any editor: ConfirmDialog.Open(request). Same look as RenameDialog: the
    // preview box, the message and detail, then No / Yes (Enter = Yes, Escape = No). The shell draws it every frame.
    public static class ConfirmDialog
    {
        private const string ModalId = "xg_confirm";
        private const float Width = 380.0f;
        private const float PreviewSize = 48.0f;
        private const float ButtonWidth = 72.0f;

        private static ConfirmRequest? _pending;
        private static int _delay;
        private static ConfirmRequest? _request;
        private static bool _open;
        private static int _shownFrames;

        public static bool IsOpen => _request is not null;

        // Opens after delayFrames frames (a context menu closing in the same frame would close a modal opened with it).
        public static void Open(ConfirmRequest request, int delayFrames = 2)
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
                _open = true;
                _shownFrames = 0;
            }
            if (_open)
            {
                _open = false;
                Gui.OpenPopup(ModalId);
            }
            using var modal = Gui.Modal(ModalId, _request?.Title ?? "Confirm", new Vector2(Width, 0.0f));
            if (!modal.IsOpen)
            {
                _request = null;
                return;
            }
            if (_request is not ConfirmRequest request)
            {
                Gui.ClosePopup();
                return;
            }
            bool yes = false;
            bool no = false;
            using (Gui.Horizontal(10.0f))
            {
                using (var preview = Gui.Canvas("##confirm_preview", new Vector2(PreviewSize, PreviewSize)))
                {
                    Rect rect = Rect.FromXYWH(0.0f, 0.0f, PreviewSize, PreviewSize);
                    preview.Draw.RectFilled(rect, Style.Color(StyleColor.SegmentBg), 4.0f);
                    Rect inner = Rect.FromXYWH(4.0f, 4.0f, PreviewSize - 8.0f, PreviewSize - 8.0f);
                    if (request.DrawPreview is { } draw)
                        draw(preview.Draw, inner);
                    else
                        preview.Draw.Icon(Icon.Warning, inner.Center, 26.0f, Style.Color(StyleColor.IconWarning));
                    preview.Draw.Rect(rect, Style.Color(StyleColor.Border), 4.0f);
                }
                using (Gui.Vertical())
                {
                    using (Gui.Font(FontStyle.Bold))
                        Gui.Label(request.Message);
                    if (request.Detail.Length > 0)
                    {
                        using var detail = Gui.Canvas("##confirm_detail", new Vector2(0.0f, 18.0f));
                        detail.Draw.Text(new Vector2(0.0f, 2.0f), Style.Color(StyleColor.TextDim), request.Detail);
                    }
                    Gui.Space(4.0f);
                    yes = Gui.KeyPressed(Key.Enter, false);
                    no = Gui.KeyPressed(Key.Escape, false);
                    float width = MathF.Max(Gui.AvailableSize.x, ButtonWidth * 2.0f + 4.0f);
                    using var buttons = Gui.Canvas("##confirm_buttons", new Vector2(width, 22.0f));
                    Painter painter = buttons.Draw;
                    no |= ToggleButton.Draw(painter, "##confirm_no", Rect.FromXYWH(width - ButtonWidth * 2.0f - 4.0f, 0.0f, ButtonWidth, 22.0f), Icon.None, null,
                        request.NoLabel, null, null, false, true, ToggleDot.None).Clicked();
                    yes |= ToggleButton.Draw(painter, "##confirm_yes", Rect.FromXYWH(width - ButtonWidth, 0.0f, ButtonWidth, 22.0f), Icon.None, null,
                        request.YesLabel, null, null, true, true, ToggleDot.None).Clicked();
                }
            }
            if (++_shownFrames == 40 && Environment.GetEnvironmentVariable("XG_UI_TEST_CONFIRM") is { Length: > 0 } answer)
            {
                yes |= answer == "yes";
                no |= answer == "no";
            }
            if (!yes && !no)
                return;
            Gui.ClosePopup();
            _request = null;
            if (!yes)
                return;
            try
            {
                request.Confirm();
            }
            catch (Exception exception)
            {
                Debug.LogException(exception);
            }
        }
    }
}
