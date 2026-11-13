// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Runtime.CompilerServices;
using System.Runtime.InteropServices;
using xg.math;

[assembly: DisableRuntimeMarshalling]

namespace xg.imui
{
    // Mirrors xg::imui::PanelDesc.
    [StructLayout(LayoutKind.Sequential)]
    internal unsafe struct NativePanelDesc
    {
        public uint Size;
        public byte* Title;
        public byte* MenuPath;
        public DockSlot Slot;
        public Icon PanelIcon;
        public PanelFlags Flags;
        public delegate* unmanaged<nint, nint, void> OnGUI;
        public delegate* unmanaged<nint, void> OnOpen;
        public delegate* unmanaged<nint, void> OnClose;
        public nint User;
        public Rect InitialRect;
    }

    // 1:1 bindings to xgImUI.h. Every XG_ImUI export used by managed code is declared here.
    // C++ bool crosses as byte; struct arguments cross by pointer.
    internal static unsafe partial class Native
    {
        private const string DLL = "xgImUIKit";
        public const uint ApiVersion = 10;

        [LibraryImport(DLL, EntryPoint = "XG_ImUIGetApiVersion")]
        public static partial uint GetApiVersion();

        [LibraryImport(DLL, EntryPoint = "XG_ImUIRegisterPanel")]
        public static partial nint RegisterPanel(NativePanelDesc* desc);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIUnregisterPanel")]
        public static partial void UnregisterPanel(nint panel);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIOpenPanel")]
        public static partial void OpenPanel(nint panel);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIClosePanel")]
        public static partial void ClosePanel(nint panel);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIIsPanelOpen")]
        public static partial byte IsPanelOpen(nint panel);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIFocusPanel")]
        public static partial void FocusPanel(nint panel);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIRequestRepaint")]
        public static partial void RequestRepaint(nint panel);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIRegisterMenuItem")]
        public static partial nint RegisterMenuItem(byte* path, byte* shortcut, delegate* unmanaged<nint, void> callback, nint user);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIUnregisterMenuItem")]
        public static partial void UnregisterMenuItem(nint item);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIPushID")]
        public static partial void PushID(nint ctx, byte* id);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIPushIDPtr")]
        public static partial void PushIDPtr(nint ctx, nint id);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIPushIDInt")]
        public static partial void PushIDInt(nint ctx, int id);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIPopID")]
        public static partial void PopID(nint ctx);

        [LibraryImport(DLL, EntryPoint = "XG_ImUILabel")]
        public static partial Result Label(nint ctx, byte* text);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIButton")]
        public static partial Result Button(nint ctx, byte* label);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIFieldFloat")]
        public static partial Result FieldFloat(nint ctx, byte* label, float* value, float speed);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIFieldInt")]
        public static partial Result FieldInt(nint ctx, byte* label, int* value, float speed);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIFieldVector")]
        public static partial Result FieldVector(nint ctx, byte* label, float* values, int count, float speed);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIFieldText")]
        public static partial Result FieldText(nint ctx, byte* label, byte* buffer, int capacity);

        [LibraryImport(DLL, EntryPoint = "XG_ImUISliderFloat")]
        public static partial Result SliderFloat(nint ctx, byte* label, float* value, float min, float max);

        [LibraryImport(DLL, EntryPoint = "XG_ImUISliderInt")]
        public static partial Result SliderInt(nint ctx, byte* label, int* value, int min, int max);

        [LibraryImport(DLL, EntryPoint = "XG_ImUICheckbox")]
        public static partial Result Checkbox(nint ctx, byte* label, byte* value);

        [LibraryImport(DLL, EntryPoint = "XG_ImUICombo")]
        public static partial Result Combo(nint ctx, byte* label, int* index, byte** items, int count);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIFoldout")]
        public static partial Result Foldout(nint ctx, byte* label, byte* open);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIPropertyLabel")]
        public static partial Result PropertyLabel(nint ctx, byte* label, byte* value);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIToggleButton")]
        public static partial Result ToggleButton(nint ctx, byte* label, byte* on);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIIconToggle")]
        public static partial Result IconToggle(nint ctx, byte* id, Icon icon, byte* on, Color32 color, byte* text);

        [LibraryImport(DLL, EntryPoint = "XG_ImUISearchField")]
        public static partial Result SearchField(nint ctx, byte* id, byte* buffer, int capacity, float width);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIIconButton")]
        public static partial Result IconButton(nint ctx, byte* id, Icon icon, Vector2* position, float size, Color32 color);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIHitArea")]
        public static partial Result HitArea(nint ctx, byte* id, Rect* rect);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIItemTooltip")]
        public static partial void ItemTooltip(nint ctx, byte* text);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIOpenPopup")]
        public static partial void OpenPopup(nint ctx, byte* id);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIBeginModal")]
        public static partial byte BeginModal(nint ctx, byte* id, byte* title, Vector2* size);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIEndModal")]
        public static partial void EndModal(nint ctx);

        [LibraryImport(DLL, EntryPoint = "XG_ImUICloseModal")]
        public static partial void CloseModal(nint ctx);

        [LibraryImport(DLL, EntryPoint = "XG_ImUITextField")]
        public static partial Result TextField(nint ctx, byte* id, byte* hint, byte* buffer, int capacity, float width);

        [LibraryImport(DLL, EntryPoint = "XG_ImUITextArea")]
        public static partial Result TextArea(nint ctx, byte* id, byte* text, Vector2* size);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIPushFont")]
        public static partial void PushFont(nint ctx, FontStyle style);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIPopFont")]
        public static partial void PopFont(nint ctx);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIBeginToolbar")]
        public static partial void BeginToolbar(nint ctx);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIEndToolbar")]
        public static partial void EndToolbar(nint ctx);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIBeginHorizontal")]
        public static partial void BeginHorizontal(nint ctx, float spacing);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIEndHorizontal")]
        public static partial void EndHorizontal(nint ctx);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIBeginVertical")]
        public static partial void BeginVertical(nint ctx);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIEndVertical")]
        public static partial void EndVertical(nint ctx);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIFlexibleSpace")]
        public static partial void FlexibleSpace(nint ctx);

        [LibraryImport(DLL, EntryPoint = "XG_ImUISpace")]
        public static partial void Space(nint ctx, float pixels);

        [LibraryImport(DLL, EntryPoint = "XG_ImUISameLine")]
        public static partial void SameLine(nint ctx, float spacing);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIAlignRight")]
        public static partial void AlignRight(nint ctx, float width);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIAvailableSize")]
        public static partial Vector2 AvailableSize(nint ctx);

        [LibraryImport(DLL, EntryPoint = "XG_ImUISplitter")]
        public static partial Result Splitter(nint ctx, byte* id, float* size, float minSize, float maxSize);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIBeginList")]
        public static partial byte BeginList(nint ctx, byte* id, Vector2* size, ListState* state);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIEndList")]
        public static partial void EndList(nint ctx);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIBeginListRow")]
        public static partial Result BeginListRow(nint ctx, int index, byte selected);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIEndListRow")]
        public static partial void EndListRow(nint ctx);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIBeginTreeRow")]
        public static partial Result BeginTreeRow(nint ctx, int index, int depth, TreeRowFlags flags);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIEndTreeRow")]
        public static partial void EndTreeRow(nint ctx);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIBeginCanvas")]
        public static partial Result BeginCanvas(nint ctx, byte* id, Vector2* size, Rect* outRect);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIEndCanvas")]
        public static partial void EndCanvas(nint ctx);

        [LibraryImport(DLL, EntryPoint = "XG_ImUISetDrawLayer")]
        public static partial void SetDrawLayer(nint ctx, DrawLayer layer);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIPushClipRect")]
        public static partial void PushClipRect(nint ctx, Rect* rect, byte intersectWithCurrent);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIPopClipRect")]
        public static partial void PopClipRect(nint ctx);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIDrawLine")]
        public static partial void DrawLine(nint ctx, Vector2* from, Vector2* to, Color32 color, float thickness);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIDrawRect")]
        public static partial void DrawRect(nint ctx, Rect* rect, Color32 color, float rounding, float thickness);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIDrawRectFilled")]
        public static partial void DrawRectFilled(nint ctx, Rect* rect, Color32 color, float rounding);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIDrawCircle")]
        public static partial void DrawCircle(nint ctx, Vector2* center, float radius, Color32 color, float thickness);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIDrawCircleFilled")]
        public static partial void DrawCircleFilled(nint ctx, Vector2* center, float radius, Color32 color);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIDrawTriangleFilled")]
        public static partial void DrawTriangleFilled(nint ctx, Vector2* a, Vector2* b, Vector2* c, Color32 color);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIDrawPolyline")]
        public static partial void DrawPolyline(nint ctx, Vector2* points, int count, Color32 color, byte closed, float thickness);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIDrawText")]
        public static partial void DrawText(nint ctx, Vector2* position, Color32 color, byte* text);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIDrawIcon")]
        public static partial void DrawIcon(nint ctx, Icon icon, Vector2* center, float size, Color32 color);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIDrawImage")]
        public static partial void DrawImage(nint ctx, nint texture, Rect* rect, Vector2* uv0, Vector2* uv1, Color32 tint);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIDrawExternalTexture")]
        public static partial void DrawExternalTexture(nint ctx, ulong textureId, Rect* rect, Vector2* uv0, Vector2* uv1, Color32 tint);

        [LibraryImport(DLL, EntryPoint = "XG_ImUISceneView")]
        public static partial Result SceneView(nint ctx, byte* id, Vector2* size, SceneViewState* state);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIStyleToColor")]
        public static partial void StyleToColor(StyleColor token, Color32* outColor);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIMeasureText")]
        public static partial Vector2 MeasureText(byte* text);

        [LibraryImport(DLL, EntryPoint = "XG_ImUICreateTexture")]
        public static partial nint CreateTexture(byte* rgba, int width, int height);

        [LibraryImport(DLL, EntryPoint = "XG_ImUIDestroyTexture")]
        public static partial void DestroyTexture(nint texture);
    }
}
