// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
//
// Header-only C++ layer over the xgImUIKit exports (xgImUI.h). Only the XG_ImUI* calls cross the DLL boundary,
// so STL types are allowed here. Mirrors the managed xg.imui API (Gui, Painter, Panel, Menu, Texture).
#pragma once
#include "xgImUI.h"
#include "xgVec3.h"

#include <functional>
#include <memory>
#include <string>
#include <utility>

namespace xg::imui
{
    XG_FORCEINLINE Color32 Style(StyleColor token)
    {
        Color32 color;
        XG_ImUIStyleToColor(token, &color);
        return color;
    }

    XG_FORCEINLINE bool Clicked(Result r) { return HasFlag(r, Result::Clicked); }
    XG_FORCEINLINE bool Changed(Result r) { return HasFlag(r, Result::Changed); }
    XG_FORCEINLINE bool Hovered(Result r) { return HasFlag(r, Result::Hovered); }

    class Texture
    {
    public:
        Texture() = default;
        Texture(const uint8_t* rgba, int width, int height) : _handle(XG_ImUICreateTexture(rgba, width, height)) {}
        ~Texture() { Reset(); }
        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;
        Texture(Texture&& o) noexcept : _handle(std::exchange(o._handle, nullptr)) {}
        Texture& operator=(Texture&& o) noexcept { if (this != &o) { Reset(); _handle = std::exchange(o._handle, nullptr); } return *this; }

        void Reset() { if (_handle) XG_ImUIDestroyTexture(_handle); _handle = nullptr; }
        TextureHandle Native() const { return _handle; }
        explicit operator bool() const { return _handle != nullptr; }

    private:
        TextureHandle _handle = nullptr;
    };

    // Immediate drawing in the current canvas (or panel content) space.
    class Painter
    {
    public:
        explicit Painter(Context* ctx) : _ctx(ctx) {}

        void Line(const Vec2& from, const Vec2& to, Color32 color, float thickness = 1.0f) const { XG_ImUIDrawLine(_ctx, &from, &to, color, thickness); }
        void DrawRect(const xg::Rect& r, Color32 color, float rounding = 0.0f, float thickness = 1.0f) const { XG_ImUIDrawRect(_ctx, &r, color, rounding, thickness); }
        void DrawRectFilled(const xg::Rect& r, Color32 color, float rounding = 0.0f) const { XG_ImUIDrawRectFilled(_ctx, &r, color, rounding); }
        void ColorBox(const xg::Rect& r, Color32 color) const { XG_ImUIDrawColorBox(_ctx, &r, color); }
        void Circle(const Vec2& center, float radius, Color32 color, float thickness = 1.0f) const { XG_ImUIDrawCircle(_ctx, &center, radius, color, thickness); }
        void CircleFilled(const Vec2& center, float radius, Color32 color) const { XG_ImUIDrawCircleFilled(_ctx, &center, radius, color); }
        void TriangleFilled(const Vec2& a, const Vec2& b, const Vec2& c, Color32 color) const { XG_ImUIDrawTriangleFilled(_ctx, &a, &b, &c, color); }
        void Polyline(const Vec2* points, int count, Color32 color, bool closed = false, float thickness = 1.0f) const { XG_ImUIDrawPolyline(_ctx, points, count, color, closed, thickness); }
        void Text(const Vec2& position, Color32 color, const char* text) const { XG_ImUIDrawText(_ctx, &position, color, text); }
        void Text(const Vec2& position, Color32 color, const std::string& text) const { XG_ImUIDrawText(_ctx, &position, color, text.c_str()); }
        // Turned by quarterTurns * 90 degrees clockwise; position is the top-left of the turned text's bounds.
        void TextRotated(const Vec2& position, Color32 color, const char* text, int quarterTurns) const { XG_ImUIDrawTextRotated(_ctx, &position, color, text, quarterTurns); }
        void DrawIcon(imui::Icon icon, const Vec2& center, float size, Color32 color) const { XG_ImUIDrawIcon(_ctx, icon, &center, size, color); }
        void Image(const Texture& texture, const xg::Rect& r, const Vec2& uv0 = Vec2(0.0f, 0.0f), const Vec2& uv1 = Vec2(1.0f, 1.0f), Color32 tint = Color32(255, 255, 255, 255)) const
        {
            XG_ImUIDrawImage(_ctx, texture.Native(), &r, &uv0, &uv1, tint);
        }
        void ExternalImage(uint64_t textureId, const xg::Rect& r, const Vec2& uv0 = Vec2(0.0f, 0.0f), const Vec2& uv1 = Vec2(1.0f, 1.0f), Color32 tint = Color32(255, 255, 255, 255)) const
        {
            XG_ImUIDrawExternalTexture(_ctx, textureId, &r, &uv0, &uv1, tint);
        }
        void PushClip(const xg::Rect& r, bool intersect = true) const { XG_ImUIPushClipRect(_ctx, &r, intersect); }
        void PopClip() const { XG_ImUIPopClipRect(_ctx); }
        void Layer(DrawLayer layer) const { XG_ImUISetDrawLayer(_ctx, layer); }

        static Vec2 MeasureText(const char* text) { const interop::Vec2Abi s = XG_ImUIMeasureText(text); return Vec2(s.x, s.y); }

    private:
        Context* _ctx;
    };

    class Gui
    {
    public:
        explicit Gui(Context* ctx) : _ctx(ctx) {}
        Context* Native() const { return _ctx; }

        class IdScope
        {
        public:
            explicit IdScope(Context* ctx) : _ctx(ctx) {}
            ~IdScope() { if (_ctx) XG_ImUIPopID(_ctx); }
            IdScope(const IdScope&) = delete;
            IdScope& operator=(const IdScope&) = delete;
            IdScope(IdScope&& o) noexcept : _ctx(std::exchange(o._ctx, nullptr)) {}
        private:
            Context* _ctx;
        };

        class CanvasScope
        {
        public:
            CanvasScope(Context* ctx, const char* id, const Vec2& size)
                : _ctx(ctx)
            {
                _result = XG_ImUIBeginCanvas(_ctx, id, &size, &_bounds);
            }
            ~CanvasScope() { if (_ctx) XG_ImUIEndCanvas(_ctx); }
            CanvasScope(const CanvasScope&) = delete;
            CanvasScope& operator=(const CanvasScope&) = delete;
            CanvasScope(CanvasScope&& o) noexcept : _ctx(std::exchange(o._ctx, nullptr)), _bounds(o._bounds), _result(o._result) {}

            // Canvas bounds in panel content space; draw with Painter in canvas space (0,0 = top-left).
            const xg::Rect& Bounds() const { return _bounds; }
            Vec2 Size() const { return Vec2(_bounds.Width(), _bounds.Height()); }
            Result Interaction() const { return _result; }
            Painter Draw() const { return Painter(_ctx); }

        private:
            Context* _ctx;
            xg::Rect _bounds;
            Result _result = Result::None;
        };

        Result Label(const char* text) const { return XG_ImUILabel(_ctx, text); }
        Result Label(const std::string& text) const { return Label(text.c_str()); }
        bool Button(const char* label) const { return Clicked(XG_ImUIButton(_ctx, label)); }
        Result ButtonResult(const char* label) const { return XG_ImUIButton(_ctx, label); }
        bool Field(const char* label, float& value, float speed = 0.1f) const { return Changed(XG_ImUIFieldFloat(_ctx, label, &value, speed)); }
        bool Field(const char* label, int& value, float speed = 0.2f) const { return Changed(XG_ImUIFieldInt(_ctx, label, &value, speed)); }
        bool Field(const char* label, Vec2& value, float speed = 0.1f) const { return Changed(XG_ImUIFieldVector(_ctx, label, &value.x, 2, speed)); }
        bool Field(const char* label, Vec3& value, float speed = 0.1f) const { return Changed(XG_ImUIFieldVector(_ctx, label, &value.x, 3, speed)); }
        bool Field(const char* label, float* values, int count, const NumberFieldStyle& style, float speed = 0.0f) const
        {
            return Changed(XG_ImUIFieldNumbers(_ctx, label, values, count, speed, &style));
        }
        bool Field(const char* label, char* buffer, int capacity) const { return Changed(XG_ImUIFieldText(_ctx, label, buffer, capacity)); }
        bool Slider(const char* label, float& value, float min, float max) const { return Changed(XG_ImUISliderFloat(_ctx, label, &value, min, max)); }
        bool Slider(const char* label, int& value, int min, int max) const { return Changed(XG_ImUISliderInt(_ctx, label, &value, min, max)); }
        bool Checkbox(const char* label, bool& value) const { return Changed(XG_ImUICheckbox(_ctx, label, &value)); }
        bool Combo(const char* label, int& index, const char* const* items, int count) const { return Changed(XG_ImUICombo(_ctx, label, &index, items, count)); }
        Result PropertyLabel(const char* label, const char* value) const { return XG_ImUIPropertyLabel(_ctx, label, value); }
        Result PickerField(const char* label, const char* text, Icon icon, Color32 iconColor, TextureHandle thumbnail = nullptr, const char* chip = nullptr,
            bool open = false) const
        {
            return XG_ImUIFieldPicker(_ctx, label, text, icon, iconColor, thumbnail, chip, open);
        }
        bool Foldout(const char* label, bool& open) const { XG_ImUIFoldout(_ctx, label, &open); return open; }

        [[nodiscard]] IdScope Id(const char* id) const { XG_ImUIPushID(_ctx, id); return IdScope(_ctx); }
        [[nodiscard]] IdScope Id(const void* id) const { XG_ImUIPushIDPtr(_ctx, id); return IdScope(_ctx); }
        [[nodiscard]] IdScope Id(int id) const { XG_ImUIPushIDInt(_ctx, id); return IdScope(_ctx); }
        [[nodiscard]] CanvasScope Canvas(const char* id, const Vec2& size = Vec2(0.0f, 0.0f)) const { return CanvasScope(_ctx, id, size); }

        Result SceneView(const char* id, SceneViewState& state, const Vec2& size = Vec2(0.0f, 0.0f)) const
        {
            return XG_ImUISceneView(_ctx, id, &size, &state);
        }

        Painter Draw() const { return Painter(_ctx); }

        bool Toggle(const char* label, bool& on) const { return Changed(XG_ImUIToggleButton(_ctx, label, &on)); }
        bool IconToggle(const char* id, imui::Icon icon, bool& on, Color32 color, const char* text = nullptr) const
        {
            return Changed(XG_ImUIIconToggle(_ctx, id, icon, &on, color, text));
        }
        bool SearchField(const char* id, char* buffer, int capacity, float width = 200.0f) const
        {
            return Changed(XG_ImUISearchField(_ctx, id, buffer, capacity, width));
        }

        bool TextField(const char* id, const char* hint, char* buffer, int capacity, float width = 0.0f) const
        {
            return Changed(XG_ImUITextField(_ctx, id, hint, buffer, capacity, width));
        }
        Result TextField(const char* id, const char* hint, char* buffer, int capacity, float width, TextFieldFlags flags) const
        {
            return XG_ImUITextFieldEx(_ctx, id, hint, buffer, capacity, width, flags);
        }
        Result TextArea(const char* id, const char* text, const Vec2& size = Vec2(0.0f, 0.0f)) const { return XG_ImUITextArea(_ctx, id, text, &size); }
        bool KeyPressed(imui::Key key, bool repeat = true) const { return XG_ImUIIsKeyPressed(_ctx, key, repeat); }
        bool ItemActive() const { return XG_ImUIIsItemActive(_ctx); }
        void FocusNext() const { XG_ImUISetKeyboardFocusHere(_ctx); }
        void SetClipboard(const char* text) const { XG_ImUISetClipboardText(_ctx, text); }
        xg::Rect ItemRect() const { xg::Rect rect; XG_ImUIGetItemRect(_ctx, &rect); return rect; }
        xg::Vec2 MousePosition() const { xg::Vec2 position; XG_ImUIGetMousePosition(_ctx, &position); return position; }

        bool IconButton(const char* id, imui::Icon icon, float size, Color32 color) const
        {
            return Clicked(XG_ImUIIconButton(_ctx, id, icon, nullptr, size, color));
        }
        // Placed at position in the current draw space (row or canvas space), over the row.
        bool IconButton(const char* id, imui::Icon icon, const Vec2& position, float size, Color32 color) const
        {
            return Clicked(XG_ImUIIconButton(_ctx, id, icon, &position, size, color));
        }
        Result HitArea(const char* id, const xg::Rect& rect) const { return XG_ImUIHitArea(_ctx, id, &rect); }
        void Tooltip(const char* text) const { XG_ImUIItemTooltip(_ctx, text); }

        class ModalScope
        {
        public:
            ModalScope(Context* ctx, const char* id, const char* title, const Vec2& size)
                : _ctx(ctx), _open(XG_ImUIBeginModal(ctx, id, title, &size)) {}
            ~ModalScope() { if (_ctx && _open) XG_ImUIEndModal(_ctx); }
            ModalScope(const ModalScope&) = delete;
            ModalScope& operator=(const ModalScope&) = delete;
            ModalScope(ModalScope&& o) noexcept : _ctx(std::exchange(o._ctx, nullptr)), _open(o._open) {}
            explicit operator bool() const { return _open; }
        private:
            Context* _ctx;
            bool _open;
        };

        void OpenPopup(const char* id) const { XG_ImUIOpenPopup(_ctx, id); }
        [[nodiscard]] ModalScope Modal(const char* id, const char* title, const Vec2& size = Vec2(0.0f, 0.0f)) const { return ModalScope(_ctx, id, title, size); }
        void CloseModal() const { XG_ImUICloseModal(_ctx); }

        // Flyout: gui.OpenPopup(id) once, then each frame if (auto popup = gui.Popup(id, position, size)) { ... }.
        class PopupScope
        {
        public:
            PopupScope(Context* ctx, const char* id, const Vec2& position, const Vec2& size)
                : _ctx(ctx), _open(XG_ImUIBeginPopup(ctx, id, &position, &size)) {}
            ~PopupScope() { if (_ctx && _open) XG_ImUIEndPopup(_ctx); }
            PopupScope(const PopupScope&) = delete;
            PopupScope& operator=(const PopupScope&) = delete;
            PopupScope(PopupScope&& o) noexcept : _ctx(std::exchange(o._ctx, nullptr)), _open(o._open) {}
            explicit operator bool() const { return _open; }
        private:
            Context* _ctx;
            bool _open;
        };

        [[nodiscard]] PopupScope Popup(const char* id, const Vec2& position, const Vec2& size = Vec2(0.0f, 0.0f)) const { return PopupScope(_ctx, id, position, size); }

        // Floating box that never takes focus (autocomplete lists): if (auto overlay = gui.Overlay(id, position, size)) { ... }.
        class OverlayScope
        {
        public:
            OverlayScope(Context* ctx, const char* id, const Vec2& position, const Vec2& size)
                : _ctx(ctx), _open(XG_ImUIBeginOverlay(ctx, id, &position, &size)) {}
            ~OverlayScope() { if (_ctx && _open) XG_ImUIEndOverlay(_ctx); }
            OverlayScope(const OverlayScope&) = delete;
            OverlayScope& operator=(const OverlayScope&) = delete;
            OverlayScope(OverlayScope&& o) noexcept : _ctx(std::exchange(o._ctx, nullptr)), _open(o._open) {}
            explicit operator bool() const { return _open; }
        private:
            Context* _ctx;
            bool _open;
        };

        [[nodiscard]] OverlayScope Overlay(const char* id, const Vec2& position, const Vec2& size = Vec2(0.0f, 0.0f)) const { return OverlayScope(_ctx, id, position, size); }

        void SameLine(float spacing = -1.0f) const { XG_ImUISameLine(_ctx, spacing); }
        void AlignRight(float width) const { XG_ImUIAlignRight(_ctx, width); }
        Vec2 AvailableSize() const { const interop::Vec2Abi v = XG_ImUIAvailableSize(_ctx); return Vec2(v.x, v.y); }

        // auto card = gui.Card("##health", "Health", Icon::Physics, nullptr, color, &open); if (card) { ...fields... }
        class CardScope
        {
        public:
            CardScope(Context* ctx, const char* id, const char* title, imui::Icon icon, TextureHandle texture, Color32 color, bool* open)
                : _ctx(ctx), _open(XG_ImUIBeginCard(ctx, id, title, icon, texture, color, open)) {}
            ~CardScope() { if (_ctx) XG_ImUIEndCard(_ctx); }
            CardScope(const CardScope&) = delete;
            CardScope& operator=(const CardScope&) = delete;
            CardScope(CardScope&& o) noexcept : _ctx(std::exchange(o._ctx, nullptr)), _open(o._open) {}
            explicit operator bool() const { return _open; }
        private:
            Context* _ctx;
            bool _open;
        };

        [[nodiscard]] CardScope Card(const char* id, const char* title, imui::Icon icon, TextureHandle texture, Color32 color, bool* open = nullptr) const
        {
            return CardScope(_ctx, id, title, icon, texture, color, open);
        }

        // Placement of the panel being drawn; false outside a panel.
        bool PanelPlacement(imui::PanelPlacement& placement) const
        {
            placement.Size = sizeof(imui::PanelPlacement);
            return XG_ImUIGetPanelPlacement(_ctx, &placement);
        }

        // auto area = gui.ScrollArea("##tools"); scrollbars appear only when the content overflows.
        class ScrollAreaScope
        {
        public:
            ScrollAreaScope(Context* ctx, const char* id, const Vec2& size, ScrollAreaFlags flags)
                : _ctx(ctx), _open(XG_ImUIBeginScrollArea(ctx, id, &size, flags)) {}
            ~ScrollAreaScope() { if (_ctx && _open) XG_ImUIEndScrollArea(_ctx); }
            ScrollAreaScope(const ScrollAreaScope&) = delete;
            ScrollAreaScope& operator=(const ScrollAreaScope&) = delete;
            ScrollAreaScope(ScrollAreaScope&& o) noexcept : _ctx(std::exchange(o._ctx, nullptr)), _open(o._open) {}
            explicit operator bool() const { return _open; }
        private:
            Context* _ctx;
            bool _open = false;
        };

        [[nodiscard]] ScrollAreaScope ScrollArea(const char* id, const Vec2& size = Vec2(0.0f, 0.0f), ScrollAreaFlags flags = ScrollAreaFlags::None) const
        {
            return ScrollAreaScope(_ctx, id, size, flags);
        }
        // auto region = gui.Region("##options", rect, &contentSize); immediate GUI placed at rect in the current draw space.
        class RegionScope
        {
        public:
            RegionScope(Context* ctx, const char* id, const Rect& rect, Vec2* contentSize)
                : _ctx(ctx), _open(XG_ImUIBeginRegion(ctx, id, &rect, contentSize)) {}
            ~RegionScope() { if (_ctx && _open) XG_ImUIEndRegion(_ctx); }
            RegionScope(const RegionScope&) = delete;
            RegionScope& operator=(const RegionScope&) = delete;
            RegionScope(RegionScope&& o) noexcept : _ctx(std::exchange(o._ctx, nullptr)), _open(o._open) {}
            explicit operator bool() const { return _open; }
        private:
            Context* _ctx;
            bool _open = false;
        };

        [[nodiscard]] RegionScope Region(const char* id, const Rect& rect, Vec2* contentSize = nullptr) const
        {
            return RegionScope(_ctx, id, rect, contentSize);
        }
        bool Splitter(const char* id, float& size, float minSize, float maxSize) const { return Changed(XG_ImUISplitter(_ctx, id, &size, minSize, maxSize)); }
        bool SplitterVertical(const char* id, float& size, float minSize, float maxSize) const { return Changed(XG_ImUISplitterVertical(_ctx, id, &size, minSize, maxSize)); }

        class FontScope
        {
        public:
            FontScope(Context* ctx, FontStyle style) : _ctx(ctx) { XG_ImUIPushFont(_ctx, style); }
            ~FontScope() { if (_ctx) XG_ImUIPopFont(_ctx); }
            FontScope(const FontScope&) = delete;
            FontScope& operator=(const FontScope&) = delete;
            FontScope(FontScope&& o) noexcept : _ctx(std::exchange(o._ctx, nullptr)) {}
        private:
            Context* _ctx;
        };

        class ToolbarScope
        {
        public:
            explicit ToolbarScope(Context* ctx) : _ctx(ctx) { XG_ImUIBeginToolbar(_ctx); }
            ~ToolbarScope() { if (_ctx) XG_ImUIEndToolbar(_ctx); }
            ToolbarScope(const ToolbarScope&) = delete;
            ToolbarScope& operator=(const ToolbarScope&) = delete;
            ToolbarScope(ToolbarScope&& o) noexcept : _ctx(std::exchange(o._ctx, nullptr)) {}
        private:
            Context* _ctx;
        };

        // auto row = gui.Horizontal(); ... items left to right until the scope ends.
        class LayoutScope
        {
        public:
            LayoutScope(Context* ctx, bool horizontal, float spacing) : _ctx(ctx), _horizontal(horizontal)
            {
                if (horizontal)
                    XG_ImUIBeginHorizontal(_ctx, spacing);
                else
                    XG_ImUIBeginVertical(_ctx);
            }
            ~LayoutScope()
            {
                if (!_ctx)
                    return;
                if (_horizontal)
                    XG_ImUIEndHorizontal(_ctx);
                else
                    XG_ImUIEndVertical(_ctx);
            }
            LayoutScope(const LayoutScope&) = delete;
            LayoutScope& operator=(const LayoutScope&) = delete;
            LayoutScope(LayoutScope&& o) noexcept : _ctx(std::exchange(o._ctx, nullptr)), _horizontal(o._horizontal) {}
        private:
            Context* _ctx;
            bool _horizontal;
        };

        class ListRowScope
        {
        public:
            ListRowScope(Context* ctx, int index, bool selected) : _ctx(ctx), _result(XG_ImUIBeginListRow(ctx, index, selected)) {}
            ~ListRowScope() { if (_ctx) XG_ImUIEndListRow(_ctx); }
            ListRowScope(const ListRowScope&) = delete;
            ListRowScope& operator=(const ListRowScope&) = delete;
            ListRowScope(ListRowScope&& o) noexcept : _ctx(std::exchange(o._ctx, nullptr)), _result(o._result) {}

            Result Interaction() const { return _result; }
            Painter Draw() const { return Painter(_ctx); }
        private:
            Context* _ctx;
            Result _result = Result::None;
        };

        class TreeRowScope
        {
        public:
            TreeRowScope(Context* ctx, int index, int depth, TreeRowFlags flags) : _ctx(ctx), _result(XG_ImUIBeginTreeRow(ctx, index, depth, flags)) {}
            ~TreeRowScope() { if (_ctx) XG_ImUIEndTreeRow(_ctx); }
            TreeRowScope(const TreeRowScope&) = delete;
            TreeRowScope& operator=(const TreeRowScope&) = delete;
            TreeRowScope(TreeRowScope&& o) noexcept : _ctx(std::exchange(o._ctx, nullptr)), _result(o._result) {}

            Result Interaction() const { return _result; }
            bool Clicked() const { return HasFlag(_result, Result::Clicked); }
            bool Toggled() const { return HasFlag(_result, Result::Changed); }
            Painter Draw() const { return Painter(_ctx); }
        private:
            Context* _ctx;
            Result _result = Result::None;
        };

        class ListScope
        {
        public:
            ListScope(Context* ctx, const char* id, const Vec2& size, ListState& state)
                : _ctx(ctx), _open(XG_ImUIBeginList(ctx, id, &size, &state)) {}
            ~ListScope() { if (_ctx && _open) XG_ImUIEndList(_ctx); }
            ListScope(const ListScope&) = delete;
            ListScope& operator=(const ListScope&) = delete;
            ListScope(ListScope&& o) noexcept : _ctx(std::exchange(o._ctx, nullptr)), _open(o._open) {}

            bool IsOpen() const { return _open; }
            [[nodiscard]] ListRowScope Row(int index, bool selected) const { return ListRowScope(_ctx, index, selected); }
            [[nodiscard]] TreeRowScope TreeRow(int index, int depth, TreeRowFlags flags) const { return TreeRowScope(_ctx, index, depth, flags); }
        private:
            Context* _ctx;
            bool _open = false;
        };

        [[nodiscard]] ToolbarScope Toolbar() const { return ToolbarScope(_ctx); }
        [[nodiscard]] LayoutScope Horizontal(float spacing = -1.0f) const { return LayoutScope(_ctx, true, spacing); }
        [[nodiscard]] LayoutScope Vertical() const { return LayoutScope(_ctx, false, -1.0f); }
        void FlexibleSpace() const { XG_ImUIFlexibleSpace(_ctx); }
        void Space(float pixels) const { XG_ImUISpace(_ctx, pixels); }
        [[nodiscard]] FontScope Font(FontStyle style) const { return FontScope(_ctx, style); }
        [[nodiscard]] ListScope List(const char* id, ListState& state, const Vec2& size = Vec2(0.0f, 0.0f)) const { return ListScope(_ctx, id, size, state); }

    private:
        Context* _ctx;
    };

    // Base for native panels. Register() adds it to the dock layout and the Window menu.
    class Panel
    {
    public:
        struct Settings
        {
            const char* Title = nullptr;
            const char* MenuPath = nullptr;
            DockSlot Slot = DockSlot::Floating;
            imui::Icon PanelIcon = imui::Icon::None;
            PanelFlags Flags = PanelFlags::None;
            xg::Rect InitialRect;
        };

        Panel() = default;
        virtual ~Panel() { Unregister(); }
        Panel(const Panel&) = delete;
        Panel& operator=(const Panel&) = delete;

        bool Register(const Settings& settings)
        {
            Unregister();
            PanelDesc desc = {};
            desc.Size = sizeof(desc);
            desc.Title = settings.Title;
            desc.MenuPath = settings.MenuPath;
            desc.Slot = settings.Slot;
            desc.PanelIcon = settings.PanelIcon;
            desc.Flags = settings.Flags;
            desc.OnGUI = &Panel::GUITrampoline;
            desc.OnOpen = &Panel::OpenTrampoline;
            desc.OnClose = &Panel::CloseTrampoline;
            desc.User = this;
            desc.InitialRect = settings.InitialRect;
            _handle = XG_ImUIRegisterPanel(&desc);
            return _handle != nullptr;
        }

        void Unregister() { if (_handle) XG_ImUIUnregisterPanel(_handle); _handle = nullptr; }
        void Open() const { XG_ImUIOpenPanel(_handle); }
        void Close() const { XG_ImUIClosePanel(_handle); }
        void Focus() const { XG_ImUIFocusPanel(_handle); }
        void Dock() const { XG_ImUIDockPanel(_handle); }
        void Repaint() const { XG_ImUIRequestRepaint(_handle); }
        bool IsOpen() const { return XG_ImUIIsPanelOpen(_handle); }
        bool IsRegistered() const { return _handle != nullptr; }

    protected:
        virtual void OnGUI(Gui& gui) = 0;
        virtual void OnOpen() {}
        virtual void OnClose() {}

    private:
        static void GUITrampoline(Context* ctx, void* user)
        {
            Gui gui(ctx);
            static_cast<Panel*>(user)->OnGUI(gui);
        }
        static void OpenTrampoline(void* user) { static_cast<Panel*>(user)->OnOpen(); }
        static void CloseTrampoline(void* user) { static_cast<Panel*>(user)->OnClose(); }

        PanelHandle _handle = nullptr;
    };

    inline void SetShellBar(ShellBar bar, PanelGUIFn callback, void* user) { XG_ImUISetShellBar(bar, callback, user); }
    inline void ResetLayout() { XG_ImUIResetLayout(); }
    inline void SetDockOutline(Color32 color) { XG_ImUISetDockOutline(color); }

    // Main menu command, unregistered on destruction. Path "Tools/Rebuild" creates a Tools menu if needed.
    class MenuCommand
    {
    public:
        MenuCommand() = default;
        MenuCommand(const char* path, const char* shortcut, std::function<void()> action)
            : _action(std::make_unique<std::function<void()>>(std::move(action)))
        {
            _handle = XG_ImUIRegisterMenuItem(path, shortcut, &MenuCommand::Invoke, _action.get());
        }
        ~MenuCommand() { Reset(); }
        MenuCommand(const MenuCommand&) = delete;
        MenuCommand& operator=(const MenuCommand&) = delete;
        MenuCommand(MenuCommand&& o) noexcept : _handle(std::exchange(o._handle, nullptr)), _action(std::move(o._action)) {}
        MenuCommand& operator=(MenuCommand&& o) noexcept
        {
            if (this != &o) { Reset(); _handle = std::exchange(o._handle, nullptr); _action = std::move(o._action); }
            return *this;
        }

        void Reset() { if (_handle) XG_ImUIUnregisterMenuItem(_handle); _handle = nullptr; _action.reset(); }

    private:
        static void Invoke(void* user) { (*static_cast<std::function<void()>*>(user))(); }

        MenuItemHandle _handle = nullptr;
        std::unique_ptr<std::function<void()>> _action;
    };
}
