// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "xgImUI.h"
#include "ImUIInternal.h"
#include "ImUIHost.h"
#include "xgUIDraw.h"

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace xg::imui
{
    struct PanelEntry
    {
        std::string Title;
        std::string MenuPath;
        DockSlot Slot = DockSlot::Floating;
        Icon PanelIcon = Icon::None;
        PanelFlags Flags = PanelFlags::None;
        PanelGUIFn OnGUI = nullptr;
        PanelEventFn OnOpen = nullptr;
        PanelEventFn OnClose = nullptr;
        void* User = nullptr;
        bool Open = true;
        bool FocusRequest = false;
        Rect InitialRect;
        bool Removed = false;
    };

    struct MenuItemEntry
    {
        std::string TopLevel;
        std::string SubPath;
        std::string Shortcut;
        MenuFn Callback = nullptr;
        void* User = nullptr;
        bool Removed = false;
    };

    struct TextureEntry
    {
        ImTextureData* Data = nullptr;
    };

    struct Context
    {
        static constexpr int MaxCanvasDepth = 8;

        PanelEntry* Panel = nullptr;
        ImVec2 ContentOrigin;
        ImVec2 CanvasOrigins[MaxCanvasDepth];
        int CanvasDepth = 0;
        DrawLayer Layer = DrawLayer::Panel;

        ImVec2 Origin() const { return CanvasDepth > 0 ? CanvasOrigins[CanvasDepth - 1] : ContentOrigin; }
        ImVec2 ToScreen(const Vec2& v) const { const ImVec2 o = Origin(); return ImVec2(o.x + v.x, o.y + v.y); }
        ImDrawList* DrawList() const
        {
            return Layer == DrawLayer::Foreground ? ImGui::GetForegroundDrawList(ImGui::GetWindowViewport()) : ImGui::GetWindowDrawList();
        }
    };
}

namespace
{
    using namespace xg;
    using namespace xg::imui;

    std::vector<PanelEntry*> s_panels;
    std::vector<MenuItemEntry*> s_menuItems;
    std::vector<TextureEntry*> s_textures;
    bool s_drawing = false;
    ImGuiID s_dockIds[5] = {};

    ImU32 ToImU32(Color32 c) { return IM_COL32(c.r, c.g, c.b, c.a); }
    Color32 ToColor32(ImU32 c)
    {
        return Color32(static_cast<uint8_t>(c >> IM_COL32_R_SHIFT), static_cast<uint8_t>(c >> IM_COL32_G_SHIFT),
            static_cast<uint8_t>(c >> IM_COL32_B_SHIFT), static_cast<uint8_t>(c >> IM_COL32_A_SHIFT));
    }
    ImVec2 ToImVec2(const Vec2& v) { return ImVec2(v.x, v.y); }
    Vec2 LoadVec2(const Vec2* v) { return v ? interop::Load(v) : Vec2(); }
    Rect LoadRect(const Rect* r) { return r ? interop::Load(r) : Rect(); }

    void FlushRemoved()
    {
        for (auto it = s_panels.begin(); it != s_panels.end();)
        {
            if ((*it)->Removed)
            {
                delete *it;
                it = s_panels.erase(it);
            }
            else
                ++it;
        }
        for (auto it = s_menuItems.begin(); it != s_menuItems.end();)
        {
            if ((*it)->Removed)
            {
                delete *it;
                it = s_menuItems.erase(it);
            }
            else
                ++it;
        }
    }

    bool InFrame()
    {
        return ImGui::GetCurrentContext() != nullptr && ImGui::GetCurrentContext()->WithinFrameScope;
    }

    bool IsRegistered(PanelHandle panel)
    {
        return std::find(s_panels.begin(), s_panels.end(), panel) != s_panels.end();
    }

    Result ItemResult(bool changed)
    {
        Result result = changed ? Result::Changed : Result::None;
        if (ImGui::IsItemHovered())
            result = result | Result::Hovered;
        if (ImGui::IsItemActive())
            result = result | Result::Active;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
            result = result | Result::Clicked;
        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            result = result | Result::RightClicked;
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            result = result | Result::DoubleClicked;
        return result;
    }

    void SetOpen(PanelEntry* entry, bool open)
    {
        if (!entry || entry->Removed || entry->Open == open)
            return;
        entry->Open = open;
        if (open && entry->OnOpen)
            entry->OnOpen(entry->User);
        if (!open && entry->OnClose)
            entry->OnClose(entry->User);
    }

    void DrawPanel(PanelEntry* entry)
    {
        const int slot = static_cast<int>(entry->Slot);
        if (entry->Slot != DockSlot::Floating && s_dockIds[slot] != 0)
            ImGui::SetNextWindowDockID(s_dockIds[slot], ImGuiCond_FirstUseEver);
        if (entry->FocusRequest && imui::detail::s_selectDefaultTabs == 0)
        {
            ImGui::SetNextWindowFocus();
            entry->FocusRequest = false;
        }

        const bool shellStyle = imui::detail::HostPrototypeShell();
        if (shellStyle)
        {
            ImGuiWindowClass windowClass;
            windowClass.DockNodeFlagsOverrideSet = ImGuiDockNodeFlags_NoWindowMenuButton | ImGuiDockNodeFlags_NoCloseButton;
            ImGui::SetNextWindowClass(&windowClass);
            ImGui::PushStyleColor(ImGuiCol_WindowBg, imui::detail::GapBg);
        }
        const Rect& initial = entry->InitialRect;
        if (initial.Width() > 0.0f && initial.Height() > 0.0f)
        {
            const ImVec2 origin = ImGui::GetMainViewport()->Pos;
            ImGui::SetNextWindowPos(ImVec2(origin.x + initial.min.x, origin.y + initial.min.y), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(initial.Width(), initial.Height()), ImGuiCond_FirstUseEver);
        }
        else
        {
            ImGui::SetNextWindowSize(ImVec2(420.0f, 320.0f), ImGuiCond_FirstUseEver);
        }
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

        bool open = true;
        const bool visible = ImGui::Begin(entry->Title.c_str(), &open,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse);
        ImGui::PopStyleVar();
        if (shellStyle)
            ImGui::PopStyleColor();

        if (visible)
        {
            if (shellStyle)
            {
                const ImRect r = ImGui::GetCurrentWindow()->InnerRect;
                ImGui::GetWindowDrawList()->AddRectFilled(r.Min, r.Max, imui::detail::HeaderBg, imui::detail::AreaRounding, ImDrawFlags_RoundCornersBottom);
            }

            ImGuiWindowFlags bodyFlags = ImGuiWindowFlags_None;
            if (HasFlag(entry->Flags, PanelFlags::NoScrollbar))
                bodyFlags |= ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
            const ImVec2 padding = HasFlag(entry->Flags, PanelFlags::NoPadding) ? ImVec2(0.0f, 0.0f) : ImVec2(8.0f, 6.0f);
            imui::detail::BeginBody("##imui_body", padding, bodyFlags);

            if (entry->OnGUI)
            {
                Context ctx;
                ctx.Panel = entry;
                ctx.ContentOrigin = ImGui::GetCursorScreenPos();

                ImGuiErrorRecoveryState state;
                ImGui::ErrorRecoveryStoreState(&state);
                ImDrawList* bodyList = ImGui::GetWindowDrawList();
                const int clipDepth = bodyList->_ClipRectStack.Size;
                entry->OnGUI(&ctx, entry->User);
                ImGui::ErrorRecoveryTryToRecoverState(&state);
                while (bodyList->_ClipRectStack.Size > clipDepth)
                    bodyList->PopClipRect();
                ImDrawList* foreground = ImGui::GetForegroundDrawList(ImGui::GetWindowViewport());
                while (foreground->_ClipRectStack.Size > 1)
                    foreground->PopClipRect();
            }
            ImGui::EndChild();
        }
        ImGui::End();

        if (!open)
            SetOpen(entry, false);
    }

    void SplitMenuPath(const char* path, std::string& top, std::string& rest)
    {
        const std::string full = path ? path : "";
        const size_t slash = full.find('/');
        if (slash == std::string::npos)
        {
            top = "Window";
            rest = full;
        }
        else
        {
            top = full.substr(0, slash);
            rest = full.substr(slash + 1);
        }
    }

    void DrawMenuPath(const std::string& path, const char* shortcut, bool* selected, bool& activated)
    {
        const size_t slash = path.find('/');
        if (slash == std::string::npos)
        {
            if (ImGui::MenuItem(path.c_str(), shortcut, selected))
                activated = true;
            return;
        }
        if (ImGui::BeginMenu(path.substr(0, slash).c_str()))
        {
            DrawMenuPath(path.substr(slash + 1), shortcut, selected, activated);
            ImGui::EndMenu();
        }
    }
}

namespace xg::imui::detail
{
    void DrawRegisteredPanels()
    {
        s_drawing = true;
        for (size_t i = 0; i < s_panels.size(); ++i)
        {
            PanelEntry* entry = s_panels[i];
            if (!entry->Removed && entry->Open)
                DrawPanel(entry);
        }
        s_drawing = false;
        FlushRemoved();
    }

    void DockRegisteredPanels(ImGuiID center, ImGuiID left, ImGuiID right, ImGuiID bottom)
    {
        s_dockIds[static_cast<int>(DockSlot::Center)] = center;
        s_dockIds[static_cast<int>(DockSlot::Left)] = left;
        s_dockIds[static_cast<int>(DockSlot::Right)] = right;
        s_dockIds[static_cast<int>(DockSlot::Bottom)] = bottom;
        for (PanelEntry* entry : s_panels)
            if (!entry->Removed && entry->Slot != DockSlot::Floating)
                ImGui::DockBuilderDockWindow(entry->Title.c_str(), s_dockIds[static_cast<int>(entry->Slot)]);
    }

    void SetDefaultDockSpace(ImGuiID id)
    {
        for (ImGuiID& dock : s_dockIds)
            dock = id;
        s_dockIds[static_cast<int>(DockSlot::Floating)] = 0;
    }

    void DrawRegisteredPanelsMenu()
    {
        bool separated = false;
        for (PanelEntry* entry : s_panels)
        {
            if (entry->Removed)
                continue;
            if (!separated)
            {
                ImGui::Separator();
                separated = true;
            }
            const std::string path = entry->MenuPath.empty() ? entry->Title : entry->MenuPath;
            bool open = entry->Open;
            bool activated = false;
            DrawMenuPath(path, nullptr, &open, activated);
            if (activated)
            {
                SetOpen(entry, true);
                entry->FocusRequest = true;
            }
        }
    }

    void DrawRegisteredMenuItems(const char* topLevel)
    {
        bool separated = false;
        for (size_t i = 0; i < s_menuItems.size(); ++i)
        {
            MenuItemEntry* item = s_menuItems[i];
            if (item->Removed || item->TopLevel != topLevel)
                continue;
            if (!separated)
            {
                ImGui::Separator();
                separated = true;
            }
            bool activated = false;
            DrawMenuPath(item->SubPath, item->Shortcut.empty() ? nullptr : item->Shortcut.c_str(), nullptr, activated);
            if (activated && item->Callback)
                item->Callback(item->User);
        }
    }

    void DrawExtraTopLevelMenus(const char* const* builtIn, int builtInCount)
    {
        std::vector<std::string> extra;
        for (MenuItemEntry* item : s_menuItems)
        {
            if (item->Removed)
                continue;
            const bool isBuiltIn = std::any_of(builtIn, builtIn + builtInCount, [&](const char* name) { return item->TopLevel == name; });
            if (!isBuiltIn && std::find(extra.begin(), extra.end(), item->TopLevel) == extra.end())
                extra.push_back(item->TopLevel);
        }
        for (const std::string& top : extra)
        {
            if (ImGui::BeginMenu(top.c_str()))
            {
                DrawRegisteredMenuItems(top.c_str());
                ImGui::EndMenu();
            }
        }
    }

    void ReleaseUserTextures()
    {
        UIRenderTarget* target = HostRenderTarget();
        for (TextureEntry* texture : s_textures)
        {
            if (texture->Data)
            {
                const uint64_t id = static_cast<uint64_t>(texture->Data->GetTexID());
                if (id != 0 && target)
                    target->DestroyUITexture(id);
                ImGui::UnregisterUserTexture(texture->Data);
                IM_DELETE(texture->Data);
                texture->Data = nullptr;
            }
        }
    }
}

namespace xg
{
    // ------------------------------------------------------------
    // Panels and menus
    // ------------------------------------------------------------
    XG_MODULE_EXPORT imui::PanelHandle XG_ImUIRegisterPanel(const imui::PanelDesc* desc)
    {
        if (!desc || desc->Size < offsetof(imui::PanelDesc, InitialRect) || !desc->Title || !desc->Title[0])
            return nullptr;

        auto* entry = new imui::PanelEntry();
        entry->Title = desc->Title;
        entry->MenuPath = desc->MenuPath ? desc->MenuPath : "";
        entry->Slot = (desc->Slot >= imui::DockSlot::Floating && desc->Slot <= imui::DockSlot::Bottom) ? desc->Slot : imui::DockSlot::Floating;
        entry->PanelIcon = desc->PanelIcon;
        entry->Flags = desc->Flags;
        entry->OnGUI = desc->OnGUI;
        entry->OnOpen = desc->OnOpen;
        entry->OnClose = desc->OnClose;
        entry->User = desc->User;
        if (desc->Size >= offsetof(imui::PanelDesc, InitialRect) + sizeof(Rect))
            entry->InitialRect = interop::Load(&desc->InitialRect);
        entry->Open = !HasFlag(desc->Flags, imui::PanelFlags::StartClosed);
        s_panels.push_back(entry);

        if (entry->Open && entry->OnOpen)
            entry->OnOpen(entry->User);
        return entry;
    }

    XG_MODULE_EXPORT void XG_ImUIUnregisterPanel(imui::PanelHandle panel)
    {
        if (!IsRegistered(panel) || panel->Removed)
            return;
        panel->Removed = true;
        if (!s_drawing)
            FlushRemoved();
    }

    XG_MODULE_EXPORT void XG_ImUIOpenPanel(imui::PanelHandle panel)
    {
        if (IsRegistered(panel))
            SetOpen(panel, true);
    }

    XG_MODULE_EXPORT void XG_ImUIClosePanel(imui::PanelHandle panel)
    {
        if (IsRegistered(panel))
            SetOpen(panel, false);
    }

    XG_MODULE_EXPORT bool XG_ImUIIsPanelOpen(imui::PanelHandle panel)
    {
        return IsRegistered(panel) && !panel->Removed && panel->Open;
    }

    XG_MODULE_EXPORT void XG_ImUIFocusPanel(imui::PanelHandle panel)
    {
        if (!IsRegistered(panel))
            return;
        SetOpen(panel, true);
        panel->FocusRequest = true;
    }

    XG_MODULE_EXPORT void XG_ImUIRequestRepaint(imui::PanelHandle)
    {
    }

    XG_MODULE_EXPORT imui::MenuItemHandle XG_ImUIRegisterMenuItem(const char* path, const char* shortcut, imui::MenuFn callback, void* user)
    {
        if (!path || !path[0] || !callback)
            return nullptr;
        auto* item = new imui::MenuItemEntry();
        SplitMenuPath(path, item->TopLevel, item->SubPath);
        item->Shortcut = shortcut ? shortcut : "";
        item->Callback = callback;
        item->User = user;
        s_menuItems.push_back(item);
        return item;
    }

    XG_MODULE_EXPORT void XG_ImUIUnregisterMenuItem(imui::MenuItemHandle item)
    {
        auto it = std::find(s_menuItems.begin(), s_menuItems.end(), item);
        if (it == s_menuItems.end() || item->Removed)
            return;
        item->Removed = true;
        if (!InFrame())
            FlushRemoved();
    }

    // ------------------------------------------------------------
    // Identity
    // ------------------------------------------------------------
    XG_MODULE_EXPORT void XG_ImUIPushID(imui::Context* ctx, const char* id)
    {
        if (ctx)
            ImGui::PushID(id ? id : "");
    }

    XG_MODULE_EXPORT void XG_ImUIPushIDPtr(imui::Context* ctx, const void* id)
    {
        if (ctx)
            ImGui::PushID(id);
    }

    XG_MODULE_EXPORT void XG_ImUIPushIDInt(imui::Context* ctx, int id)
    {
        if (ctx)
            ImGui::PushID(id);
    }

    XG_MODULE_EXPORT void XG_ImUIPopID(imui::Context* ctx)
    {
        if (ctx)
            ImGui::PopID();
    }

    // ------------------------------------------------------------
    // Widgets
    // ------------------------------------------------------------
    XG_MODULE_EXPORT imui::Result XG_ImUILabel(imui::Context* ctx, const char* text)
    {
        if (!ctx)
            return imui::Result::None;
        ImGui::TextUnformatted(text ? text : "");
        return ItemResult(false);
    }

    XG_MODULE_EXPORT imui::Result XG_ImUIButton(imui::Context* ctx, const char* label)
    {
        if (!ctx)
            return imui::Result::None;
        ImGui::Button(label ? label : "");
        return ItemResult(false);
    }

    XG_MODULE_EXPORT imui::Result XG_ImUIFieldFloat(imui::Context* ctx, const char* label, float* value, float speed)
    {
        if (!ctx || !value)
            return imui::Result::None;
        const char* text = label ? label : "";
        ImGui::PushID(text);
        imui::detail::PropertyLabel(text);
        const bool changed = ImGui::DragFloat("##value", value, speed > 0.0f ? speed : 0.1f, 0.0f, 0.0f, "%.2f");
        const imui::Result result = ItemResult(changed);
        ImGui::PopID();
        return result;
    }

    // ------------------------------------------------------------
    // Immediate drawing
    // ------------------------------------------------------------
    XG_MODULE_EXPORT imui::Result XG_ImUIBeginCanvas(imui::Context* ctx, const char* id, const Vec2* size, Rect* outRect)
    {
        if (!ctx || ctx->CanvasDepth >= imui::Context::MaxCanvasDepth)
            return imui::Result::None;

        const Vec2 requested = LoadVec2(size);
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const ImVec2 canvasSize((std::max)(requested.x > 0.0f ? requested.x : avail.x, 1.0f), (std::max)(requested.y > 0.0f ? requested.y : avail.y, 1.0f));
        ImGui::InvisibleButton(id ? id : "##canvas", canvasSize,
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight | ImGuiButtonFlags_MouseButtonMiddle);
        const imui::Result result = ItemResult(false);

        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        if (outRect)
        {
            const Rect bounds(Vec2(min.x - ctx->ContentOrigin.x, min.y - ctx->ContentOrigin.y), Vec2(max.x - ctx->ContentOrigin.x, max.y - ctx->ContentOrigin.y));
            interop::Store(outRect, bounds);
        }

        ctx->CanvasOrigins[ctx->CanvasDepth++] = min;
        ImGui::GetWindowDrawList()->PushClipRect(min, max, true);
        return result;
    }

    XG_MODULE_EXPORT void XG_ImUIEndCanvas(imui::Context* ctx)
    {
        if (!ctx || ctx->CanvasDepth <= 0)
            return;
        ImGui::GetWindowDrawList()->PopClipRect();
        --ctx->CanvasDepth;
    }

    XG_MODULE_EXPORT void XG_ImUISetDrawLayer(imui::Context* ctx, imui::DrawLayer layer)
    {
        if (ctx)
            ctx->Layer = layer == imui::DrawLayer::Foreground ? imui::DrawLayer::Foreground : imui::DrawLayer::Panel;
    }

    XG_MODULE_EXPORT void XG_ImUIPushClipRect(imui::Context* ctx, const Rect* rect, bool intersectWithCurrent)
    {
        if (!ctx)
            return;
        const Rect r = LoadRect(rect);
        ctx->DrawList()->PushClipRect(ctx->ToScreen(r.min), ctx->ToScreen(r.max), intersectWithCurrent);
    }

    XG_MODULE_EXPORT void XG_ImUIPopClipRect(imui::Context* ctx)
    {
        if (ctx)
            ctx->DrawList()->PopClipRect();
    }

    XG_MODULE_EXPORT void XG_ImUIDrawLine(imui::Context* ctx, const Vec2* from, const Vec2* to, Color32 color, float thickness)
    {
        if (ctx)
            ctx->DrawList()->AddLine(ctx->ToScreen(LoadVec2(from)), ctx->ToScreen(LoadVec2(to)), ToImU32(color), thickness > 0.0f ? thickness : 1.0f);
    }

    XG_MODULE_EXPORT void XG_ImUIDrawRect(imui::Context* ctx, const Rect* rect, Color32 color, float rounding, float thickness)
    {
        if (!ctx)
            return;
        const Rect r = LoadRect(rect);
        ctx->DrawList()->AddRect(ctx->ToScreen(r.min), ctx->ToScreen(r.max), ToImU32(color), rounding, 0, thickness > 0.0f ? thickness : 1.0f);
    }

    XG_MODULE_EXPORT void XG_ImUIDrawRectFilled(imui::Context* ctx, const Rect* rect, Color32 color, float rounding)
    {
        if (!ctx)
            return;
        const Rect r = LoadRect(rect);
        ctx->DrawList()->AddRectFilled(ctx->ToScreen(r.min), ctx->ToScreen(r.max), ToImU32(color), rounding);
    }

    XG_MODULE_EXPORT void XG_ImUIDrawCircle(imui::Context* ctx, const Vec2* center, float radius, Color32 color, float thickness)
    {
        if (ctx)
            ctx->DrawList()->AddCircle(ctx->ToScreen(LoadVec2(center)), radius, ToImU32(color), 0, thickness > 0.0f ? thickness : 1.0f);
    }

    XG_MODULE_EXPORT void XG_ImUIDrawCircleFilled(imui::Context* ctx, const Vec2* center, float radius, Color32 color)
    {
        if (ctx)
            ctx->DrawList()->AddCircleFilled(ctx->ToScreen(LoadVec2(center)), radius, ToImU32(color));
    }

    XG_MODULE_EXPORT void XG_ImUIDrawTriangleFilled(imui::Context* ctx, const Vec2* a, const Vec2* b, const Vec2* c, Color32 color)
    {
        if (ctx)
            ctx->DrawList()->AddTriangleFilled(ctx->ToScreen(LoadVec2(a)), ctx->ToScreen(LoadVec2(b)), ctx->ToScreen(LoadVec2(c)), ToImU32(color));
    }

    XG_MODULE_EXPORT void XG_ImUIDrawPolyline(imui::Context* ctx, const Vec2* points, int count, Color32 color, bool closed, float thickness)
    {
        if (!ctx || !points || count < 2)
            return;
        ImVector<ImVec2> screen;
        screen.resize(count);
        for (int i = 0; i < count; ++i)
            screen[i] = ctx->ToScreen(interop::Load(points + i));
        ctx->DrawList()->AddPolyline(screen.Data, count, ToImU32(color), closed ? ImDrawFlags_Closed : ImDrawFlags_None, thickness > 0.0f ? thickness : 1.0f);
    }

    XG_MODULE_EXPORT void XG_ImUIDrawText(imui::Context* ctx, const Vec2* position, Color32 color, const char* text)
    {
        if (ctx && text)
            ctx->DrawList()->AddText(ctx->ToScreen(LoadVec2(position)), ToImU32(color), text);
    }

    XG_MODULE_EXPORT void XG_ImUIDrawIcon(imui::Context* ctx, imui::Icon icon, const Vec2* center, float size, Color32 color)
    {
        if (!ctx || icon <= imui::Icon::None || icon >= imui::Icon::Count)
            return;
        imui::detail::DrawIcon(ctx->DrawList(), static_cast<imui::detail::Icon>(icon), ctx->ToScreen(LoadVec2(center)), size, ToImU32(color));
    }

    XG_MODULE_EXPORT void XG_ImUIDrawImage(imui::Context* ctx, imui::TextureHandle texture, const Rect* rect, const Vec2* uv0, const Vec2* uv1, Color32 tint)
    {
        if (!ctx || !texture || !texture->Data)
            return;
        const Rect r = LoadRect(rect);
        ctx->DrawList()->AddImage(texture->Data->GetTexRef(), ctx->ToScreen(r.min), ctx->ToScreen(r.max),
            ToImVec2(LoadVec2(uv0)), ToImVec2(LoadVec2(uv1)), ToImU32(tint));
    }

    XG_MODULE_EXPORT void XG_ImUIDrawExternalTexture(imui::Context* ctx, uint64_t textureId, const Rect* rect, const Vec2* uv0, const Vec2* uv1, Color32 tint)
    {
        if (!ctx || textureId == 0)
            return;
        const Rect r = LoadRect(rect);
        ctx->DrawList()->AddImage(ImTextureRef(static_cast<ImTextureID>(textureId)), ctx->ToScreen(r.min), ctx->ToScreen(r.max),
            ToImVec2(LoadVec2(uv0)), ToImVec2(LoadVec2(uv1)), ToImU32(tint));
    }

    XG_MODULE_EXPORT void XG_ImUIStyleToColor(imui::StyleColor token, Color32* outColor)
    {
        if (!outColor)
            return;
        using namespace imui::detail;
        static const ImU32 colors[static_cast<int>(imui::StyleColor::Count)] =
        {
            TextColor, TextDim, TextSelected, Accent, AccentHover, Selection, ActiveObject,
            PanelBg, PanelHeaderBg, AreaBg, HeaderBg, ToolbarBg, ViewportBg, Outline,
            IconColor, IconObject, IconData, IconComponent, IconMaterial, IconWarning,
            AxisX, AxisY, AxisZ, Grid,
        };
        const int index = static_cast<int>(token);
        interop::Store(outColor, ToColor32(index >= 0 && index < static_cast<int>(imui::StyleColor::Count) ? colors[index] : TextColor));
    }

    XG_MODULE_EXPORT interop::Vec2Abi XG_ImUIMeasureText(const char* text)
    {
        if (!text || !ImGui::GetCurrentContext())
            return interop::Vec2Abi{ 0.0f, 0.0f };
        const ImVec2 size = ImGui::CalcTextSize(text);
        return interop::Vec2Abi{ size.x, size.y };
    }

    // ------------------------------------------------------------
    // Scene views
    // ------------------------------------------------------------
    XG_MODULE_EXPORT imui::Result XG_ImUISceneView(imui::Context* ctx, const char* id, const Vec2* size, imui::SceneViewState* state)
    {
        if (!ctx || !state || state->Size < sizeof(imui::SceneViewState))
            return imui::Result::None;

        Rect rect;
        const imui::Result result = XG_ImUIBeginCanvas(ctx, id ? id : "##sceneview", size, &rect);
        if (ctx->CanvasDepth <= 0)
            return result;

        ImGuiIO& io = ImGui::GetIO();
        const bool hovered = HasFlag(result, imui::Result::Hovered);
        const bool active = HasFlag(result, imui::Result::Active);
        if (hovered)
            ImGui::SetItemKeyOwner(ImGuiKey_MouseWheelY);

        const ImVec2 origin = ctx->Origin();
        const Vec2 delta(io.MouseDelta.x, io.MouseDelta.y);
        state->PixelSize = Vec2(rect.Width() * io.DisplayFramebufferScale.x, rect.Height() * io.DisplayFramebufferScale.y);
        state->MousePosition = Vec2(io.MousePos.x - origin.x, io.MousePos.y - origin.y);
        state->DragLeft = active && ImGui::IsMouseDown(ImGuiMouseButton_Left) ? delta : Vec2();
        state->DragRight = active && ImGui::IsMouseDown(ImGuiMouseButton_Right) ? delta : Vec2();
        state->DragMiddle = active && ImGui::IsMouseDown(ImGuiMouseButton_Middle) ? delta : Vec2();
        state->Wheel = hovered ? io.MouseWheel : 0.0f;
        state->Flags = (hovered ? imui::SceneViewFlags::Hovered : imui::SceneViewFlags::None)
            | (active ? imui::SceneViewFlags::Active : imui::SceneViewFlags::None);

        const Rect local(Vec2(0.0f, 0.0f), Vec2(rect.Width(), rect.Height()));
        const Vec2 uv0(0.0f, 0.0f);
        const Vec2 uv1(1.0f, 1.0f);
        if (state->TextureId != 0)
        {
            XG_ImUIDrawExternalTexture(ctx, state->TextureId, &local, &uv0, &uv1, Color32(255, 255, 255, 255));
        }
        else
        {
            XG_ImUIDrawRectFilled(ctx, &local, ToColor32(imui::detail::ViewportBg), imui::detail::AreaRounding);
            const char* text = state->Placeholder ? state->Placeholder : "No view";
            const ImVec2 textSize = ImGui::CalcTextSize(text);
            const Vec2 textPos((rect.Width() - textSize.x) * 0.5f, (rect.Height() - textSize.y) * 0.5f);
            XG_ImUIDrawText(ctx, &textPos, ToColor32(imui::detail::TextDim), text);
        }
        if (hovered || active)
            XG_ImUIDrawRect(ctx, &local, ToColor32(active ? imui::detail::Accent : imui::detail::WithAlpha(imui::detail::Accent, 0.5f)), 0.0f, 1.0f);

        XG_ImUIEndCanvas(ctx);
        return result;
    }

    // ------------------------------------------------------------
    // Textures
    // ------------------------------------------------------------
    XG_MODULE_EXPORT imui::TextureHandle XG_ImUICreateTexture(const uint8_t* rgba, int width, int height)
    {
        if (!rgba || width <= 0 || height <= 0 || !ImGui::GetCurrentContext())
            return nullptr;
        auto* entry = new imui::TextureEntry();
        entry->Data = IM_NEW(ImTextureData)();
        entry->Data->Create(ImTextureFormat_RGBA32, width, height);
        for (int y = 0; y < height; ++y)
            std::memcpy(entry->Data->GetPixelsAt(0, y), rgba + static_cast<size_t>(y) * width * 4, static_cast<size_t>(width) * 4);
        ImGui::RegisterUserTexture(entry->Data);
        s_textures.push_back(entry);
        return entry;
    }

    XG_MODULE_EXPORT void XG_ImUIDestroyTexture(imui::TextureHandle texture)
    {
        auto it = std::find(s_textures.begin(), s_textures.end(), texture);
        if (it == s_textures.end())
            return;
        if (texture->Data && ImGui::GetCurrentContext())
        {
            const uint64_t id = static_cast<uint64_t>(texture->Data->GetTexID());
            if (id != 0)
                if (UIRenderTarget* target = imui::detail::HostRenderTarget())
                    target->DestroyUITexture(id);
            ImGui::UnregisterUserTexture(texture->Data);
            IM_DELETE(texture->Data);
        }
        s_textures.erase(it);
        delete texture;
    }
}
