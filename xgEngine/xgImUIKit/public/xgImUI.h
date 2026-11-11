// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
//
// xgImUIKit: immediate-mode UI (docking panels, widgets, immediate drawing) for tools and game runtime.
// This is the only native header other projects include; xgImUIGui.hpp adds a header-only C++ layer on top.
// ABI rules: XG_MODULE_EXPORT functions, struct arguments by const pointer (read with interop::Load), descriptor
// structs start with a uint32_t Size and only grow at the end, UTF-8 const char*.
// Coordinates are logical pixels. Draw positions are relative to the current canvas, or to the panel content origin
// when no canvas is open.
#pragma once
#include "xgBase.h"
#include "xgColor.h"
#include "xgInteropUtil.h"
#include "xgRect.h"
#include "xgVec2.h"

#include <cstdint>

namespace xg
{
    class Window;
    class UIRenderTarget;
    class PlatformServices;
    struct xgEvent;
}

namespace xg::imui
{
    inline constexpr uint32_t ApiVersion = 8;

    struct Context;
    struct PanelEntry;
    struct MenuItemEntry;
    struct TextureEntry;
    using PanelHandle = PanelEntry*;
    using MenuItemHandle = MenuItemEntry*;
    using TextureHandle = TextureEntry*;

    // Every widget returns a combination of these bits.
    enum class Result : uint8_t
    {
        None          = 0,
        Changed       = 1 << 0,
        Clicked       = 1 << 1,
        Hovered       = 1 << 2,
        Active        = 1 << 3,
        DoubleClicked = 1 << 4,
        RightClicked  = 1 << 5,
    };
    XG_ENABLE_FLAGS(Result)

    enum class DockSlot : int32_t
    {
        Floating = 0,
        Center,
        Left,
        Right,
        Bottom,
    };

    enum class PanelFlags : uint8_t
    {
        None        = 0,
        NoScrollbar = 1 << 0,
        NoPadding   = 1 << 1,
        StartClosed = 1 << 2,
        // Covers the main window's work area: no title, tab, docking, moving or resizing.
        FillViewport = 1 << 3,
    };
    XG_ENABLE_FLAGS(PanelFlags)

    // Kit fonts: Noto Sans, Noto Sans Bold and Noto Sans Mono. Order is part of the ABI: append only.
    enum class FontStyle : int32_t
    {
        Default = 0,
        Bold,
        Mono,
    };

    enum class DrawLayer : int32_t
    {
        Panel = 0,
        Foreground,
    };

    // Built-in icon set. Order is part of the ABI: append only.
    enum class Icon : int32_t
    {
        None = 0,
        Hierarchy, SceneGrid, Game, Inspector, Project, Console, Lighting,
        Plus, Search, Star, Folder, Kebab, Account, Cloud, Branch,
        Play, Pause, Step,
        Hand, Move, Rotate, Scale, Rect, Transform,
        PivotCenter, PivotLocal, Globe, Magnet, Sphere, Wire, TwoD,
        Audio, Effects, EyeOff, Gizmo, Stats,
        Scene, Camera, Light, Mesh, Object, Component, Physics,
        Data, Material, Script,
        Info, Warning, Error,
        WinMinimize, WinMaximize, WinRestore, WinClose,
        Image, Trash,
        Count
    };

    // Theme tokens. Order is part of the ABI: append only.
    enum class StyleColor : int32_t
    {
        Text = 0,
        TextDim,
        TextSelected,
        Accent,
        AccentHover,
        Selection,
        ActiveObject,
        PanelBg,
        PanelHeaderBg,
        AreaBg,
        HeaderBg,
        ToolbarBg,
        ViewportBg,
        Outline,
        Icon,
        IconObject,
        IconData,
        IconComponent,
        IconMaterial,
        IconWarning,
        AxisX,
        AxisY,
        AxisZ,
        Grid,
        Count
    };

    enum class HostFlags : uint8_t
    {
        None           = 0,
        PrototypeShell = 1 << 0,
        // Panels dragged outside the main window become OS windows. Needs Platform and a render target with surfaces.
        MultiViewport  = 1 << 1,
    };
    XG_ENABLE_FLAGS(HostFlags)

    enum class SceneViewFlags : uint8_t
    {
        None    = 0,
        Hovered = 1 << 0,
        Active  = 1 << 1,
    };
    XG_ENABLE_FLAGS(SceneViewFlags)

    using PanelGUIFn = void (*)(Context* ctx, void* user);
    using PanelEventFn = void (*)(void* user);
    using MenuFn = void (*)(void* user);

    // Host setup, owned by the application module that drives the frame.
    struct HostDesc
    {
        uint32_t Size;
        Window* MainWindow;
        UIRenderTarget* RenderTarget;
        HostFlags Flags;
        const char* FontDirectory;
        PlatformServices* Platform;
        // Window layout file (relative to the working directory). Null = "imgui.ini"; "" = don't load or save a layout.
        const char* LayoutFile;
    };

    struct PanelDesc
    {
        uint32_t Size;
        const char* Title;
        const char* MenuPath;
        DockSlot Slot;
        Icon PanelIcon;
        PanelFlags Flags;
        PanelGUIFn OnGUI;
        PanelEventFn OnOpen;
        PanelEventFn OnClose;
        void* User;
        Rect InitialRect;
    };

    // Scene view input/output. Set TextureId (and optionally Placeholder) before the call; read the rest after.
    // The owner reads PixelSize and the input, resizes/renders its view, and the texture shows from the next call.
    struct SceneViewState
    {
        uint32_t Size = sizeof(SceneViewState);
        uint64_t TextureId = 0;
        const char* Placeholder = nullptr;
        Vec2 PixelSize;
        Vec2 MousePosition;
        Vec2 DragLeft;
        Vec2 DragRight;
        Vec2 DragMiddle;
        float Wheel = 0.0f;
        SceneViewFlags Flags = SceneViewFlags::None;

        bool Hovered() const noexcept { return HasFlag(Flags, SceneViewFlags::Hovered); }
        bool Active() const noexcept { return HasFlag(Flags, SceneViewFlags::Active); }
        int PixelWidth() const noexcept { return static_cast<int>(PixelSize.x); }
        int PixelHeight() const noexcept { return static_cast<int>(PixelSize.y); }
    };

    enum class ListFlags : uint8_t
    {
        None           = 0,
        ScrollToBottom = 1 << 0,
        AtBottom       = 1 << 1,
    };
    XG_ENABLE_FLAGS(ListFlags)

    // Virtualized list input/output. Set Count and RowHeight (and ScrollToBottom) before BeginList; read First, Last
    // and AtBottom after. Submit rows First..Last-1 with BeginListRow/EndListRow; the rest is padded, not laid out.
    struct ListState
    {
        uint32_t Size = sizeof(ListState);
        int32_t Count = 0;
        float RowHeight = 22.0f;
        int32_t First = 0;
        int32_t Last = 0;
        ListFlags Flags = ListFlags::None;

        bool AtBottom() const noexcept { return HasFlag(Flags, ListFlags::AtBottom); }
    };

    enum class TreeRowFlags : uint8_t
    {
        None        = 0,
        HasChildren = 1 << 0,
        Expanded    = 1 << 1,
        Selected    = 1 << 2,
    };
    XG_ENABLE_FLAGS(TreeRowFlags)
}

namespace xg
{
    XG_MODULE_EXPORT uint32_t XG_ImUIGetApiVersion();

    // Host
    XG_MODULE_EXPORT bool XG_ImUIHostInit(const imui::HostDesc* desc);
    XG_MODULE_EXPORT void XG_ImUIHostEvent(const xgEvent* e);
    XG_MODULE_EXPORT void XG_ImUIHostFrame(float deltaTime);
    XG_MODULE_EXPORT void XG_ImUIHostSetAppIcon(const uint8_t* rgba, int width, int height);
    XG_MODULE_EXPORT void XG_ImUIHostShutdown();

    // Panels and menus
    XG_MODULE_EXPORT imui::PanelHandle XG_ImUIRegisterPanel(const imui::PanelDesc* desc);
    XG_MODULE_EXPORT void XG_ImUIUnregisterPanel(imui::PanelHandle panel);
    XG_MODULE_EXPORT void XG_ImUIOpenPanel(imui::PanelHandle panel);
    XG_MODULE_EXPORT void XG_ImUIClosePanel(imui::PanelHandle panel);
    XG_MODULE_EXPORT bool XG_ImUIIsPanelOpen(imui::PanelHandle panel);
    XG_MODULE_EXPORT void XG_ImUIFocusPanel(imui::PanelHandle panel);
    XG_MODULE_EXPORT void XG_ImUIRequestRepaint(imui::PanelHandle panel);

    XG_MODULE_EXPORT imui::MenuItemHandle XG_ImUIRegisterMenuItem(const char* path, const char* shortcut, imui::MenuFn callback, void* user);
    XG_MODULE_EXPORT void XG_ImUIUnregisterMenuItem(imui::MenuItemHandle item);

    // Identity
    XG_MODULE_EXPORT void XG_ImUIPushID(imui::Context* ctx, const char* id);
    XG_MODULE_EXPORT void XG_ImUIPushIDPtr(imui::Context* ctx, const void* id);
    XG_MODULE_EXPORT void XG_ImUIPushIDInt(imui::Context* ctx, int id);
    XG_MODULE_EXPORT void XG_ImUIPopID(imui::Context* ctx);

    // Widgets
    XG_MODULE_EXPORT imui::Result XG_ImUILabel(imui::Context* ctx, const char* text);
    XG_MODULE_EXPORT imui::Result XG_ImUIButton(imui::Context* ctx, const char* label);
    XG_MODULE_EXPORT imui::Result XG_ImUIFieldFloat(imui::Context* ctx, const char* label, float* value, float speed);
    XG_MODULE_EXPORT imui::Result XG_ImUIToggleButton(imui::Context* ctx, const char* label, bool* on);
    // Icon toggle with optional text after it (e.g. a count). Dimmed while off.
    XG_MODULE_EXPORT imui::Result XG_ImUIIconToggle(imui::Context* ctx, const char* id, imui::Icon icon, bool* on, Color32 color, const char* text);
    // Single-line search box editing a UTF-8 buffer in place. Returns Changed when the text changed.
    XG_MODULE_EXPORT imui::Result XG_ImUISearchField(imui::Context* ctx, const char* id, char* buffer, int capacity, float width);
    // Single-line text box editing a UTF-8 buffer in place; hint shows while empty. width <= 0 fills the line.
    // Returns Changed when the text changed.
    XG_MODULE_EXPORT imui::Result XG_ImUITextField(imui::Context* ctx, const char* id, const char* hint, char* buffer, int capacity, float width);
    // Square flat icon button. position null lays it out inline; otherwise it is placed at that point in the current
    // draw space (row or canvas space, like the Draw* calls) over the row, and the layout cursor is left unchanged.
    // size <= 0 uses the frame height. Returns Clicked (and Changed) when pressed.
    XG_MODULE_EXPORT imui::Result XG_ImUIIconButton(imui::Context* ctx, const char* id, imui::Icon icon, const Vec2* position, float size, Color32 color);
    // Invisible interactive rectangle in the current draw space (Hovered, Clicked, DoubleClicked, RightClicked).
    XG_MODULE_EXPORT imui::Result XG_ImUIHitArea(imui::Context* ctx, const char* id, const Rect* rect);
    // Tooltip shown while the previous item is hovered.
    XG_MODULE_EXPORT void XG_ImUIItemTooltip(imui::Context* ctx, const char* text);

    // Modals: OpenPopup(id) once, then each frame if (BeginModal(id, title, size)) { ...; EndModal(); }.
    // The modal is centered on the main window and dims it; size.y <= 0 fits the content. Escape closes it.
    XG_MODULE_EXPORT void XG_ImUIOpenPopup(imui::Context* ctx, const char* id);
    XG_MODULE_EXPORT bool XG_ImUIBeginModal(imui::Context* ctx, const char* id, const char* title, const Vec2* size);
    XG_MODULE_EXPORT void XG_ImUIEndModal(imui::Context* ctx);
    XG_MODULE_EXPORT void XG_ImUICloseModal(imui::Context* ctx);

    // Read-only, selectable multi-line text (Ctrl+A / Ctrl+C copy). size <= 0 fills the available space.
    XG_MODULE_EXPORT imui::Result XG_ImUITextArea(imui::Context* ctx, const char* id, const char* text, const Vec2* size);

    // Fonts: affects widgets and Painter text until the matching PopFont.
    XG_MODULE_EXPORT void XG_ImUIPushFont(imui::Context* ctx, imui::FontStyle style);
    XG_MODULE_EXPORT void XG_ImUIPopFont(imui::Context* ctx);

    // Layout
    XG_MODULE_EXPORT void XG_ImUIBeginToolbar(imui::Context* ctx);
    XG_MODULE_EXPORT void XG_ImUIEndToolbar(imui::Context* ctx);
    // Places the next item on the same line. spacing < 0 uses the default spacing.
    XG_MODULE_EXPORT void XG_ImUISameLine(imui::Context* ctx, float spacing);
    // Moves the cursor so the next width pixels of items end at the right edge.
    XG_MODULE_EXPORT void XG_ImUIAlignRight(imui::Context* ctx, float width);
    XG_MODULE_EXPORT interop::Vec2Abi XG_ImUIAvailableSize(imui::Context* ctx);
    // Horizontal bar; dragging it changes *size (the height of the area below it), clamped to [minSize, maxSize].
    XG_MODULE_EXPORT imui::Result XG_ImUISplitter(imui::Context* ctx, const char* id, float* size, float minSize, float maxSize);

    // Virtualized lists
    XG_MODULE_EXPORT bool XG_ImUIBeginList(imui::Context* ctx, const char* id, const Vec2* size, imui::ListState* state);
    XG_MODULE_EXPORT void XG_ImUIEndList(imui::Context* ctx);
    // One full-width row; Painter draws in row space ((0,0) = row top-left) until EndListRow.
    // Hovered is set while the mouse is over the row, including over icon buttons or hit areas placed on it.
    XG_MODULE_EXPORT imui::Result XG_ImUIBeginListRow(imui::Context* ctx, int index, bool selected);
    XG_MODULE_EXPORT void XG_ImUIEndListRow(imui::Context* ctx);
    // Tree row inside a list: indented by depth, with an expand arrow when HasChildren. Clicked/DoubleClicked are the
    // row; Changed means the arrow was clicked (toggle expansion). Painter draws in row space after the arrow.
    XG_MODULE_EXPORT imui::Result XG_ImUIBeginTreeRow(imui::Context* ctx, int index, int depth, imui::TreeRowFlags flags);
    XG_MODULE_EXPORT void XG_ImUIEndTreeRow(imui::Context* ctx);

    // Immediate drawing
    XG_MODULE_EXPORT imui::Result XG_ImUIBeginCanvas(imui::Context* ctx, const char* id, const Vec2* size, Rect* outRect);
    XG_MODULE_EXPORT void XG_ImUIEndCanvas(imui::Context* ctx);
    XG_MODULE_EXPORT void XG_ImUISetDrawLayer(imui::Context* ctx, imui::DrawLayer layer);
    XG_MODULE_EXPORT void XG_ImUIPushClipRect(imui::Context* ctx, const Rect* rect, bool intersectWithCurrent);
    XG_MODULE_EXPORT void XG_ImUIPopClipRect(imui::Context* ctx);

    XG_MODULE_EXPORT void XG_ImUIDrawLine(imui::Context* ctx, const Vec2* from, const Vec2* to, Color32 color, float thickness);
    XG_MODULE_EXPORT void XG_ImUIDrawRect(imui::Context* ctx, const Rect* rect, Color32 color, float rounding, float thickness);
    XG_MODULE_EXPORT void XG_ImUIDrawRectFilled(imui::Context* ctx, const Rect* rect, Color32 color, float rounding);
    XG_MODULE_EXPORT void XG_ImUIDrawCircle(imui::Context* ctx, const Vec2* center, float radius, Color32 color, float thickness);
    XG_MODULE_EXPORT void XG_ImUIDrawCircleFilled(imui::Context* ctx, const Vec2* center, float radius, Color32 color);
    XG_MODULE_EXPORT void XG_ImUIDrawTriangleFilled(imui::Context* ctx, const Vec2* a, const Vec2* b, const Vec2* c, Color32 color);
    XG_MODULE_EXPORT void XG_ImUIDrawPolyline(imui::Context* ctx, const Vec2* points, int count, Color32 color, bool closed, float thickness);
    XG_MODULE_EXPORT void XG_ImUIDrawText(imui::Context* ctx, const Vec2* position, Color32 color, const char* text);
    XG_MODULE_EXPORT void XG_ImUIDrawIcon(imui::Context* ctx, imui::Icon icon, const Vec2* center, float size, Color32 color);
    XG_MODULE_EXPORT void XG_ImUIDrawImage(imui::Context* ctx, imui::TextureHandle texture, const Rect* rect, const Vec2* uv0, const Vec2* uv1, Color32 tint);
    // Draws a texture owned by the render target (for example an offscreen render view id).
    XG_MODULE_EXPORT void XG_ImUIDrawExternalTexture(imui::Context* ctx, uint64_t textureId, const Rect* rect, const Vec2* uv0, const Vec2* uv1, Color32 tint);

    XG_MODULE_EXPORT void XG_ImUIStyleToColor(imui::StyleColor token, Color32* outColor);
    XG_MODULE_EXPORT interop::Vec2Abi XG_ImUIMeasureText(const char* text);

    // Scene views: a canvas that shows an externally rendered texture and reports size and input.
    XG_MODULE_EXPORT imui::Result XG_ImUISceneView(imui::Context* ctx, const char* id, const Vec2* size, imui::SceneViewState* state);

    // Textures (RGBA8, row pitch = width * 4)
    XG_MODULE_EXPORT imui::TextureHandle XG_ImUICreateTexture(const uint8_t* rgba, int width, int height);
    XG_MODULE_EXPORT void XG_ImUIDestroyTexture(imui::TextureHandle texture);
}
