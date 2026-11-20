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
    inline constexpr uint32_t ApiVersion = 19;

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
        Cards,
        Tag, File, Filter, Sort, View,
        Lock, Unlock,
        Clock, Stack,
        PlayWindow,
        Settings, Code, Save,
        Count
    };

    // Keys for XG_ImUIIsKeyPressed. Order is part of the ABI: append only.
    enum class Key : int32_t
    {
        None = 0,
        Tab, Left, Right, Up, Down, PageUp, PageDown, Home, End, Delete, Backspace, Enter, Escape,
        Count
    };

    enum class TextFieldFlags : uint8_t
    {
        None           = 0,
        // Replaces the text being edited with the buffer's content and puts the caret at its end (after a programmatic edit).
        SetText        = 1 << 0,
        // Tab, Up and Down stay with the caller (autocomplete) instead of moving focus or navigating.
        CaptureNavKeys = 1 << 1,
    };
    XG_ENABLE_FLAGS(TextFieldFlags)

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
        TextOnAccent,
        PanelHeaderHover,
        TopBarBg,
        SegmentBg,
        Button,
        ButtonHover,
        Border,
        Count
    };

    enum class HostFlags : uint8_t
    {
        None           = 0,
        // The editor shell: title bar, main menu, dock layout from panel slots, shell bars and theme.
        EditorShell    = 1 << 0,
        // Panels dragged outside the main window become OS windows. Needs Platform and a render target with surfaces.
        MultiViewport  = 1 << 1,
        // Editor theme and window frame (title bar, window buttons, resize edges) without the rest of the shell.
        CustomFrame    = 1 << 2,
    };
    XG_ENABLE_FLAGS(HostFlags)

    enum class SceneViewFlags : uint8_t
    {
        None       = 0,
        Hovered    = 1 << 0,
        Active     = 1 << 1,
        LeftDown   = 1 << 2,
        RightDown  = 1 << 3,
        MiddleDown = 1 << 4,
    };
    XG_ENABLE_FLAGS(SceneViewFlags)

    // Bars of the editor shell whose content an application draws (ApiVersion 15).
    enum class ShellBar : int32_t
    {
        Toolbar = 0,
        StatusBar,
    };

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
        // Rows draw no alternating background (the caller tints rows itself). ApiVersion 16.
        NoStripes      = 1 << 2,
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

    enum class ScrollAreaFlags : uint8_t
    {
        None       = 0,
        // Content is a row that scrolls horizontally (and vertically on overflow); the wheel scrolls X when nothing overflows vertically.
        Horizontal = 1 << 0,
        // 6 px padding around the content.
        Padded     = 1 << 1,
    };
    XG_ENABLE_FLAGS(ScrollAreaFlags)

    enum class PanelPlacementFlags : uint8_t
    {
        None         = 0,
        Docked       = 1 << 0,
        MainViewport = 1 << 1,
    };
    XG_ENABLE_FLAGS(PanelPlacementFlags)

    // Where the current panel is, in screen coordinates. DockNode is the panel's own dock node and DockSpace the root
    // node of its dock tree (both equal to Window when floating), so comparing them tells which side it is docked on.
    struct PanelPlacement
    {
        uint32_t Size = sizeof(PanelPlacement);
        Rect Window;
        Rect DockNode;
        Rect DockSpace;
        PanelPlacementFlags Flags = PanelPlacementFlags::None;

        bool Docked() const noexcept { return HasFlag(Flags, PanelPlacementFlags::Docked); }
    };
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
    // Opens the panel docked into its slot on the next frame, even when the layout file remembers it floating (ApiVersion 19).
    XG_MODULE_EXPORT void XG_ImUIDockPanel(imui::PanelHandle panel);
    XG_MODULE_EXPORT void XG_ImUIRequestRepaint(imui::PanelHandle panel);

    XG_MODULE_EXPORT imui::MenuItemHandle XG_ImUIRegisterMenuItem(const char* path, const char* shortcut, imui::MenuFn callback, void* user);
    XG_MODULE_EXPORT void XG_ImUIUnregisterMenuItem(imui::MenuItemHandle item);

    // Editor shell (ApiVersion 15). The callback draws the bar's content, an implicit row, every frame; null clears it.
    // The toolbar takes no space without a callback.
    XG_MODULE_EXPORT void XG_ImUISetShellBar(imui::ShellBar bar, imui::PanelGUIFn callback, void* user);
    // Rebuilds the dock layout from the panel slots on the next frame.
    XG_MODULE_EXPORT void XG_ImUIResetLayout();
    // Draws a 2 px outline in color around the editor shell's dock area (ApiVersion 17); a zero alpha removes it.
    XG_MODULE_EXPORT void XG_ImUISetDockOutline(Color32 color);

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
    // Property rows (FieldFloat and the ApiVersion 10 rows below): a dim label column and the value filling the rest.
    // The value column starts at the same x in every row of a panel (40% of its width, at least 92 px), also inside
    // indented layout groups. They are the building blocks of inspector drawers.
    XG_MODULE_EXPORT imui::Result XG_ImUIFieldInt(imui::Context* ctx, const char* label, int* value, float speed);
    XG_MODULE_EXPORT imui::Result XG_ImUIFieldVector(imui::Context* ctx, const char* label, float* values, int count, float speed);
    XG_MODULE_EXPORT imui::Result XG_ImUIFieldText(imui::Context* ctx, const char* label, char* buffer, int capacity);
    XG_MODULE_EXPORT imui::Result XG_ImUISliderFloat(imui::Context* ctx, const char* label, float* value, float min, float max);
    XG_MODULE_EXPORT imui::Result XG_ImUISliderInt(imui::Context* ctx, const char* label, int* value, int min, int max);
    XG_MODULE_EXPORT imui::Result XG_ImUICheckbox(imui::Context* ctx, const char* label, bool* value);
    // items holds count UTF-8 strings; index is the selected item (-1 shows an empty box).
    XG_MODULE_EXPORT imui::Result XG_ImUICombo(imui::Context* ctx, const char* label, int* index, const char* const* items, int count);
    // Full-width collapsible header (inspector sections); *open is toggled by a click. Changed when it toggles.
    XG_MODULE_EXPORT imui::Result XG_ImUIFoldout(imui::Context* ctx, const char* label, bool* open);
    // A label in the property label column with value text (read only) next to it.
    XG_MODULE_EXPORT imui::Result XG_ImUIPropertyLabel(imui::Context* ctx, const char* label, const char* value);
    // Icon toggle with optional text after it (e.g. a count). Dimmed while off.
    XG_MODULE_EXPORT imui::Result XG_ImUIIconToggle(imui::Context* ctx, const char* id, imui::Icon icon, bool* on, Color32 color, const char* text);
    // Single-line search box editing a UTF-8 buffer in place. Returns Changed when the text changed.
    XG_MODULE_EXPORT imui::Result XG_ImUISearchField(imui::Context* ctx, const char* id, char* buffer, int capacity, float width);
    // Single-line text box editing a UTF-8 buffer in place; hint shows while empty. width <= 0 fills the line.
    // Returns Changed when the text changed.
    XG_MODULE_EXPORT imui::Result XG_ImUITextField(imui::Context* ctx, const char* id, const char* hint, char* buffer, int capacity, float width);
    // TextField with flags (ApiVersion 12). Active is set while the field has keyboard focus.
    XG_MODULE_EXPORT imui::Result XG_ImUITextFieldEx(imui::Context* ctx, const char* id, const char* hint, char* buffer, int capacity, float width, imui::TextFieldFlags flags);
    // Square flat icon button. position null lays it out inline; otherwise it is placed at that point in the current
    // draw space (row or canvas space, like the Draw* calls) over the row, and the layout cursor is left unchanged.
    // size <= 0 uses the frame height. Returns Clicked (and Changed) when pressed.
    XG_MODULE_EXPORT imui::Result XG_ImUIIconButton(imui::Context* ctx, const char* id, imui::Icon icon, const Vec2* position, float size, Color32 color);
    // Invisible interactive rectangle in the current draw space (Hovered, Clicked, DoubleClicked, RightClicked).
    XG_MODULE_EXPORT imui::Result XG_ImUIHitArea(imui::Context* ctx, const char* id, const Rect* rect);
    // Tooltip shown while the previous item is hovered.
    XG_MODULE_EXPORT void XG_ImUIItemTooltip(imui::Context* ctx, const char* text);
    // Keyboard (ApiVersion 12): whether key went down this frame (repeat: also auto-repeats while held).
    XG_MODULE_EXPORT bool XG_ImUIIsKeyPressed(imui::Context* ctx, imui::Key key, bool repeat);
    // Whether the previous item is active (a text field being edited, a button being held).
    XG_MODULE_EXPORT bool XG_ImUIIsItemActive(imui::Context* ctx);
    // Gives keyboard focus to the next item (a text field starts editing).
    XG_MODULE_EXPORT void XG_ImUISetKeyboardFocusHere(imui::Context* ctx);
    // Puts UTF-8 text on the system clipboard (ApiVersion 16).
    XG_MODULE_EXPORT void XG_ImUISetClipboardText(imui::Context* ctx, const char* text);
    // Rect of the previous item in the current draw space (to place an overlay or popup under a field).
    XG_MODULE_EXPORT bool XG_ImUIGetItemRect(imui::Context* ctx, Rect* out);

    // Modals: OpenPopup(id) once, then each frame if (BeginModal(id, title, size)) { ...; EndModal(); }.
    // The modal is centered on the main window and dims it; size.y <= 0 fits the content. Escape closes it.
    XG_MODULE_EXPORT void XG_ImUIOpenPopup(imui::Context* ctx, const char* id);
    XG_MODULE_EXPORT bool XG_ImUIBeginModal(imui::Context* ctx, const char* id, const char* title, const Vec2* size);
    XG_MODULE_EXPORT void XG_ImUIEndModal(imui::Context* ctx);
    // Closes the current popup or modal.
    XG_MODULE_EXPORT void XG_ImUICloseModal(imui::Context* ctx);
    // Non-modal popup (a flyout) opened with OpenPopup(id): placed at position in the current draw space, size.x wide
    // (size.y <= 0 fits the content). A click outside or Escape closes it. Call EndPopup only when BeginPopup returned true.
    XG_MODULE_EXPORT bool XG_ImUIBeginPopup(imui::Context* ctx, const char* id, const Vec2* position, const Vec2* size);
    XG_MODULE_EXPORT void XG_ImUIEndPopup(imui::Context* ctx);
    // Overlay (ApiVersion 12): a borderless floating box drawn above everything at position in the current draw space,
    // size.x wide (size.y <= 0 fits the content), that never takes keyboard focus when it appears, so a text field
    // keeps editing (autocomplete lists). Shown on every frame it is submitted; the caller decides when to stop.
    // Its content is a column. Call EndOverlay only when BeginOverlay returned true.
    XG_MODULE_EXPORT bool XG_ImUIBeginOverlay(imui::Context* ctx, const char* id, const Vec2* position, const Vec2* size);
    XG_MODULE_EXPORT void XG_ImUIEndOverlay(imui::Context* ctx);

    // Read-only, selectable multi-line text (Ctrl+A / Ctrl+C copy). size <= 0 fills the available space.
    XG_MODULE_EXPORT imui::Result XG_ImUITextArea(imui::Context* ctx, const char* id, const char* text, const Vec2* size);

    // Fonts: affects widgets and Painter text until the matching PopFont.
    XG_MODULE_EXPORT void XG_ImUIPushFont(imui::Context* ctx, imui::FontStyle style);
    XG_MODULE_EXPORT void XG_ImUIPopFont(imui::Context* ctx);

    // Layout. The panel body (and each modal) is a vertical layout; Begin/EndHorizontal make rows and
    // Begin/EndVertical columns, so items are placed without SameLine. A size of 0 on TextField, SearchField,
    // TextArea, Canvas and List fills: inside a row (width) or column (height) the item shares the space left after
    // the fixed items with FlexibleSpace; elsewhere it takes the available size. Shares use the previous frame's
    // measurements, so a new layout settles one frame after it appears.
    // Toolbars are rows with header styling. spacing < 0 uses the style's item spacing.
    XG_MODULE_EXPORT void XG_ImUIBeginToolbar(imui::Context* ctx);
    XG_MODULE_EXPORT void XG_ImUIEndToolbar(imui::Context* ctx);
    XG_MODULE_EXPORT void XG_ImUIBeginHorizontal(imui::Context* ctx, float spacing);
    XG_MODULE_EXPORT void XG_ImUIEndHorizontal(imui::Context* ctx);
    XG_MODULE_EXPORT void XG_ImUIBeginVertical(imui::Context* ctx);
    XG_MODULE_EXPORT void XG_ImUIEndVertical(imui::Context* ctx);
    // Takes the leftover space of the current row or column (several share it equally).
    XG_MODULE_EXPORT void XG_ImUIFlexibleSpace(imui::Context* ctx);
    // Extra gap of pixels before the next item in a row, or a vertical gap in a column.
    XG_MODULE_EXPORT void XG_ImUISpace(imui::Context* ctx, float pixels);
    // Places the next item on the same line. spacing < 0 uses the default spacing.
    XG_MODULE_EXPORT void XG_ImUISameLine(imui::Context* ctx, float spacing);
    // Moves the cursor so the next width pixels of items end at the right edge.
    XG_MODULE_EXPORT void XG_ImUIAlignRight(imui::Context* ctx, float width);
    XG_MODULE_EXPORT interop::Vec2Abi XG_ImUIAvailableSize(imui::Context* ctx);
    // Horizontal bar; dragging it changes *size (the height of the area below it), clamped to [minSize, maxSize].
    XG_MODULE_EXPORT imui::Result XG_ImUISplitter(imui::Context* ctx, const char* id, float* size, float minSize, float maxSize);
    // Vertical bar for side-by-side areas (place it in a row); dragging changes *size (the width of the area on its
    // left), clamped to [minSize, maxSize]. It takes the available height.
    XG_MODULE_EXPORT imui::Result XG_ImUISplitterVertical(imui::Context* ctx, const char* id, float* size, float minSize, float maxSize);

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

    // Scrolling region without a background; scrollbars show only when the content overflows. Its content is a column
    // (a row with ScrollAreaFlags::Horizontal). size <= 0 fills like Canvas. Call EndScrollArea only when Begin returned true.
    XG_MODULE_EXPORT bool XG_ImUIBeginScrollArea(imui::Context* ctx, const char* id, const Vec2* size, imui::ScrollAreaFlags flags);
    XG_MODULE_EXPORT void XG_ImUIEndScrollArea(imui::Context* ctx);

    // Region (ApiVersion 13): a transparent child placed at rect in the current draw space (row, canvas or panel space),
    // without scrollbars, whose content is an implicit column with its own draw space. It leaves the layout cursor where
    // it was, so it can host immediate GUI over a canvas. contentSize (optional) receives the content size measured on
    // the previous frame. Call EndRegion only when Begin returned true.
    XG_MODULE_EXPORT bool XG_ImUIBeginRegion(imui::Context* ctx, const char* id, const Rect* rect, Vec2* contentSize);
    XG_MODULE_EXPORT void XG_ImUIEndRegion(imui::Context* ctx);

    // Group card (the Ribbon's group look) around ordinary layout content: a full-width bordered box whose header shows
    // an expand arrow (when open isn't null; a click toggles *open), the texture or icon in iconColor and the bold title.
    // Returns whether the body is open; its content is a column. Always call EndCard. Heights follow the content one
    // frame late.
    XG_MODULE_EXPORT bool XG_ImUIBeginCard(imui::Context* ctx, const char* id, const char* title, imui::Icon icon, imui::TextureHandle texture, Color32 iconColor, bool* open);
    XG_MODULE_EXPORT void XG_ImUIEndCard(imui::Context* ctx);

    // Placement of the panel being drawn (call from its OnGUI). Returns false outside a panel.
    XG_MODULE_EXPORT bool XG_ImUIGetPanelPlacement(imui::Context* ctx, imui::PanelPlacement* out);

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
    // Text turned by quarterTurns * 90 degrees clockwise (1 reads top to bottom, 3 bottom to top). position is the
    // top-left of the turned text's bounds.
    XG_MODULE_EXPORT void XG_ImUIDrawTextRotated(imui::Context* ctx, const Vec2* position, Color32 color, const char* text, int quarterTurns);
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
