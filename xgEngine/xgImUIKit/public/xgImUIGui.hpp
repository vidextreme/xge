// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
//
// Header-only C++ layer over the xgImUIKit exports (xgImUI.h). Only the XG_ImUI* calls cross the DLL boundary,
// so STL types are allowed here. Mirrors the managed xg.imui API (Gui, Painter, Panel, Menu, Texture).
#pragma once
#include "xgImUI.h"

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
        void Circle(const Vec2& center, float radius, Color32 color, float thickness = 1.0f) const { XG_ImUIDrawCircle(_ctx, &center, radius, color, thickness); }
        void CircleFilled(const Vec2& center, float radius, Color32 color) const { XG_ImUIDrawCircleFilled(_ctx, &center, radius, color); }
        void TriangleFilled(const Vec2& a, const Vec2& b, const Vec2& c, Color32 color) const { XG_ImUIDrawTriangleFilled(_ctx, &a, &b, &c, color); }
        void Polyline(const Vec2* points, int count, Color32 color, bool closed = false, float thickness = 1.0f) const { XG_ImUIDrawPolyline(_ctx, points, count, color, closed, thickness); }
        void Text(const Vec2& position, Color32 color, const char* text) const { XG_ImUIDrawText(_ctx, &position, color, text); }
        void Text(const Vec2& position, Color32 color, const std::string& text) const { XG_ImUIDrawText(_ctx, &position, color, text.c_str()); }
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
        Result TextArea(const char* id, const char* text, const Vec2& size = Vec2(0.0f, 0.0f)) const { return XG_ImUITextArea(_ctx, id, text, &size); }

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

        void SameLine(float spacing = -1.0f) const { XG_ImUISameLine(_ctx, spacing); }
        void AlignRight(float width) const { XG_ImUIAlignRight(_ctx, width); }
        Vec2 AvailableSize() const { const interop::Vec2Abi v = XG_ImUIAvailableSize(_ctx); return Vec2(v.x, v.y); }
        bool Splitter(const char* id, float& size, float minSize, float maxSize) const { return Changed(XG_ImUISplitter(_ctx, id, &size, minSize, maxSize)); }

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
