// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "xgImUI.h"
#include "ImUIInternal.h"
#include "ImUIHost.h"
#include "xgUIDraw.h"

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

namespace xg::imui
{
    struct PanelEntry
    {
        std::string Title;
        std::string WindowName;
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
        bool DockRequest = false;
        // Frames left to keep this panel the selected tab of its dock node: windows appearing in a rebuilt layout
        // take the selection for the first frames.
        int SelectTabFrames = 0;
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
        int ToolbarCount = 0;

        // Layout groups: rows and columns. Stretch items (FlexibleSpace, fill-size widgets) share the space left
        // after the fixed items, measured on the previous frame and kept in the window's state storage.
        static constexpr int MaxLayoutDepth = 16;
        struct LayoutFrame
        {
            ImGuiWindow* Window = nullptr;
            bool Horizontal = false;
            bool Group = false;
            float Spacing = 0.0f;
            float PendingSpace = 0.0f;
            int Items = 0;
            ImGuiID FixedId = 0;
            ImGuiID CountId = 0;
            ImVec2 Start;
            float Share = 0.0f;
            float StretchUsed = 0.0f;
            int Stretches = 0;
        };
        LayoutFrame Layouts[MaxLayoutDepth];
        int LayoutDepth = 0;
        int LayoutOverflow = 0;
        int LayoutCounter = 0;

        static constexpr int MaxListDepth = 4;
        struct ListFrame
        {
            int Count = 0;
            int Last = 0;
            float RowHeight = 0.0f;
            bool ScrollToBottom = false;
            bool NoStripes = false;
        };
        ListFrame Lists[MaxListDepth];
        int ListDepth = 0;

        static constexpr int MaxScrollDepth = 4;
        ImVec2 ScrollOrigins[MaxScrollDepth];
        int ScrollDepth = 0;

        static constexpr int MaxRegionDepth = 4;
        struct RegionFrame
        {
            ImVec2 ContentOrigin;
            int CanvasDepth = 0;
        };
        RegionFrame Regions[MaxRegionDepth];
        int RegionDepth = 0;

        static constexpr int MaxCardDepth = 4;
        struct CardFrame
        {
            ImVec2 Origin;
            bool BodyOpen = false;
        };
        CardFrame Cards[MaxCardDepth];
        int CardDepth = 0;
        int CardOverflow = 0;

        ImGuiWindow* PanelWindow = nullptr;

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

    std::vector<PanelEntry*> _panels;
    std::vector<MenuItemEntry*> _menuItems;
    std::vector<TextureEntry*> _textures;
    bool _drawing = false;
    ImGuiID _dockIds[5] = {};

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
        for (auto it = _panels.begin(); it != _panels.end();)
        {
            if ((*it)->Removed)
            {
                delete *it;
                it = _panels.erase(it);
            }
            else
                ++it;
        }
        for (auto it = _menuItems.begin(); it != _menuItems.end();)
        {
            if ((*it)->Removed)
            {
                delete *it;
                it = _menuItems.erase(it);
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
        return std::find(_panels.begin(), _panels.end(), panel) != _panels.end();
    }

    // Rows report Hovered while the mouse is anywhere over them, including over items placed on top of the row.
    Result RowHoverResult(const ImVec2& rowMin, const ImVec2& rowMax)
    {
        const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
            && ImGui::IsMouseHoveringRect(rowMin, rowMax, true);
        return hovered ? Result::Hovered : Result::None;
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
        if (ImGui::IsItemHovered() && ImGui::IsMouseReleased(ImGuiMouseButton_Right))
            result = result | Result::RightClicked;
        if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
            result = result | Result::DoubleClicked;
        return result;
    }

    // Layout cursor saved around an item placed at an explicit position (hit areas, placed icon buttons), so the
    // placed item changes neither where the next item goes (also after SameLine) nor the window's boundaries.
    struct CursorState
    {
        ImVec2 Pos;
        ImVec2 PrevLinePos;
        ImVec2 PrevLineSize;
        float PrevLineTextBaseOffset = 0.0f;
        bool IsSameLine = false;
        bool IsSetPos = false;

        static CursorState Save()
        {
            const ImGuiWindowTempData& dc = ImGui::GetCurrentWindow()->DC;
            return CursorState{ dc.CursorPos, dc.CursorPosPrevLine, dc.PrevLineSize, dc.PrevLineTextBaseOffset, dc.IsSameLine, dc.IsSetPos };
        }

        void Restore() const
        {
            ImGuiWindowTempData& dc = ImGui::GetCurrentWindow()->DC;
            dc.CursorPos = Pos;
            dc.CursorPosPrevLine = PrevLinePos;
            dc.PrevLineSize = PrevLineSize;
            dc.PrevLineTextBaseOffset = PrevLineTextBaseOffset;
            dc.IsSameLine = IsSameLine;
            dc.IsSetPos = IsSetPos;
        }
    };

    CursorState _regionCursors[Context::MaxRegionDepth];

    Context::LayoutFrame* CurrentLayout(Context* ctx)
    {
        if (!ctx || ctx->LayoutDepth <= 0)
            return nullptr;
        Context::LayoutFrame& frame = ctx->Layouts[ctx->LayoutDepth - 1];
        return frame.Window == ImGui::GetCurrentWindow() ? &frame : nullptr;
    }

    // Called before every layout item: places it after the previous item in a row.
    void LayoutItem(Context* ctx, bool textItem = false)
    {
        Context::LayoutFrame* frame = CurrentLayout(ctx);
        if (!frame)
            return;
        if (frame->Horizontal)
        {
            if (frame->Items > 0 && !ImGui::GetCurrentWindow()->DC.IsSameLine)
                ImGui::SameLine(0.0f, frame->Spacing + frame->PendingSpace);
            if (textItem)
                ImGui::AlignTextToFramePadding();
        }
        frame->PendingSpace = 0.0f;
        ++frame->Items;
    }

    // Size of a fill (size <= 0) item along one axis: a stretch share in a layout on that axis, else the available size.
    float LayoutFill(Context* ctx, bool horizontalAxis)
    {
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        Context::LayoutFrame* frame = CurrentLayout(ctx);
        if (!frame || frame->Horizontal != horizontalAxis)
            return (std::max)(horizontalAxis ? avail.x : avail.y, 1.0f);
        const float share = (std::max)(frame->Share, 1.0f);
        frame->StretchUsed += share;
        ++frame->Stretches;
        return share;
    }

    void BeginLayout(Context* ctx, bool horizontal, float spacing, bool group)
    {
        if (ctx->LayoutDepth >= Context::MaxLayoutDepth)
        {
            ++ctx->LayoutOverflow;
            return;
        }
        if (group)
            LayoutItem(ctx);
        char key[40];
        std::snprintf(key, sizeof(key), "##xg_layout_fixed%d", ctx->LayoutCounter);
        const ImGuiID fixedId = ImGui::GetID(key);
        std::snprintf(key, sizeof(key), "##xg_layout_count%d", ctx->LayoutCounter);
        const ImGuiID countId = ImGui::GetID(key);
        ++ctx->LayoutCounter;

        ImGuiStorage* storage = ImGui::GetStateStorage();
        const float fixed = storage->GetFloat(fixedId, -1.0f);
        const int count = storage->GetInt(countId, 0);
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        const float extent = horizontal ? avail.x : avail.y;

        Context::LayoutFrame& frame = ctx->Layouts[ctx->LayoutDepth++];
        frame = Context::LayoutFrame();
        frame.Window = ImGui::GetCurrentWindow();
        frame.Horizontal = horizontal;
        frame.Group = group;
        frame.Spacing = spacing >= 0.0f ? spacing : ImGui::GetStyle().ItemSpacing.x;
        frame.FixedId = fixedId;
        frame.CountId = countId;
        frame.Start = ImGui::GetCursorScreenPos();
        frame.Share = (fixed >= 0.0f && count > 0) ? (std::max)((extent - fixed) / static_cast<float>(count), 0.0f) : 0.0f;
        if (group)
            ImGui::BeginGroup();
    }

    void EndLayout(Context* ctx)
    {
        if (ctx->LayoutOverflow > 0)
        {
            --ctx->LayoutOverflow;
            return;
        }
        if (ctx->LayoutDepth <= 0)
            return;
        const Context::LayoutFrame frame = ctx->Layouts[--ctx->LayoutDepth];
        float size = 0.0f;
        if (frame.Group)
        {
            ImGui::EndGroup();
            const ImVec2 rect = ImGui::GetItemRectSize();
            size = frame.Horizontal ? rect.x : rect.y;
        }
        else
        {
            const ImVec2 max = frame.Window->DC.CursorMaxPos;
            size = frame.Horizontal ? max.x - frame.Start.x : max.y - frame.Start.y;
        }
        ImGuiStorage* storage = frame.Window->DC.StateStorage;
        storage->SetFloat(frame.FixedId, (std::max)(size - frame.StretchUsed, 0.0f));
        storage->SetInt(frame.CountId, frame.Stretches);
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

    void RunGUI(PanelGUIFn callback, void* user, PanelEntry* panel, ImGuiWindow* ownerWindow, bool row)
    {
        Context ctx;
        ctx.Panel = panel;
        ctx.PanelWindow = ownerWindow;
        ctx.ContentOrigin = ImGui::GetCursorScreenPos();

        ImGuiErrorRecoveryState state;
        ImGui::ErrorRecoveryStoreState(&state);
        ImDrawList* bodyList = ImGui::GetWindowDrawList();
        const int clipDepth = bodyList->_ClipRectStack.Size;
        BeginLayout(&ctx, row, -1.0f, false);
        callback(&ctx, user);
        ImGui::ErrorRecoveryTryToRecoverState(&state);
        ctx.LayoutOverflow = 0;
        ctx.LayoutDepth = (std::min)(ctx.LayoutDepth, 1);
        if (ImGui::GetCurrentWindow() == ctx.Layouts[0].Window)
            EndLayout(&ctx);
        while (bodyList->_ClipRectStack.Size > clipDepth)
            bodyList->PopClipRect();
        ImDrawList* foreground = ImGui::GetForegroundDrawList(ImGui::GetWindowViewport());
        while (foreground->_ClipRectStack.Size > 1)
            foreground->PopClipRect();
    }

    struct ShellBarEntry
    {
        PanelGUIFn Callback = nullptr;
        void* User = nullptr;
    };
    ShellBarEntry _shellBars[2];

    void DrawShellBar(ShellBar bar)
    {
        const ShellBarEntry& entry = _shellBars[static_cast<int>(bar)];
        if (entry.Callback)
            RunGUI(entry.Callback, entry.User, nullptr, ImGui::GetCurrentWindow(), true);
    }

    void DrawPanelBody(PanelEntry* entry)
    {
        ImGuiWindowFlags bodyFlags = ImGuiWindowFlags_None;
        if (HasFlag(entry->Flags, PanelFlags::NoScrollbar))
            bodyFlags |= ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        const ImVec2 padding = HasFlag(entry->Flags, PanelFlags::NoPadding) ? ImVec2(0.0f, 0.0f) : ImVec2(8.0f, 6.0f);
        ImGuiWindow* panelWindow = ImGui::GetCurrentWindow();
        imui::detail::BeginBody("##imui_body", padding, bodyFlags);

        if (entry->OnGUI)
            RunGUI(entry->OnGUI, entry->User, entry, panelWindow, false);
        ImGui::EndChild();
    }

    void DrawFillViewportPanel(PanelEntry* entry)
    {
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->WorkPos);
        ImGui::SetNextWindowSize(viewport->WorkSize);
        ImGui::SetNextWindowViewport(viewport->ID);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
        const ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove
            | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;
        const bool visible = ImGui::Begin(entry->WindowName.c_str(), nullptr, flags);
        ImGui::PopStyleVar(3);
        if (visible)
            DrawPanelBody(entry);
        ImGui::End();
    }

    void DrawPanel(PanelEntry* entry)
    {
        if (HasFlag(entry->Flags, PanelFlags::FillViewport))
        {
            entry->FocusRequest = false;
            DrawFillViewportPanel(entry);
            return;
        }

        const int slot = static_cast<int>(entry->Slot);
        if (entry->Slot != DockSlot::Floating && _dockIds[slot] != 0)
            ImGui::SetNextWindowDockID(_dockIds[slot], entry->DockRequest ? ImGuiCond_Always : ImGuiCond_FirstUseEver);
        entry->DockRequest = false;
        if (entry->FocusRequest && !imui::detail::_layoutJustReset)
        {
            ImGui::SetNextWindowFocus();
            entry->FocusRequest = false;
            for (PanelEntry* other : _panels)
                if (other->Slot == entry->Slot)
                    other->SelectTabFrames = 0;
        }

        const bool shellStyle = imui::detail::HostEditorShell();
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
        const bool visible = ImGui::Begin(entry->WindowName.c_str(), &open,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoCollapse);
        ImGui::PopStyleVar();
        if (shellStyle)
            ImGui::PopStyleColor();
        if (entry->PanelIcon > imui::Icon::None && entry->PanelIcon < imui::Icon::Count)
            imui::detail::DrawPanelTabIcon(static_cast<imui::detail::Icon>(entry->PanelIcon));
        if (entry->SelectTabFrames > 0)
        {
            const ImGuiWindow* window = ImGui::GetCurrentWindow();
            if (window->DockNode && window->DockNode->TabBar)
            {
                window->DockNode->TabBar->NextSelectedTabId = window->TabId;
                --entry->SelectTabFrames;
            }
        }

        if (visible)
        {
            if (shellStyle)
            {
                const ImRect r = ImGui::GetCurrentWindow()->InnerRect;
                ImGui::GetWindowDrawList()->AddRectFilled(r.Min, r.Max, imui::detail::HeaderBg, imui::detail::AreaRounding, ImDrawFlags_RoundCornersBottom);
            }
            DrawPanelBody(entry);
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
        _drawing = true;
        for (size_t i = 0; i < _panels.size(); ++i)
        {
            PanelEntry* entry = _panels[i];
            if (!entry->Removed && entry->Open)
                DrawPanel(entry);
        }
        _drawing = false;
        FlushRemoved();
    }

    void DockRegisteredPanels(ImGuiID center, ImGuiID left, ImGuiID right, ImGuiID bottom)
    {
        _dockIds[static_cast<int>(DockSlot::Center)] = center;
        _dockIds[static_cast<int>(DockSlot::Left)] = left;
        _dockIds[static_cast<int>(DockSlot::Right)] = right;
        _dockIds[static_cast<int>(DockSlot::Bottom)] = bottom;
        bool slotHasTab[static_cast<int>(DockSlot::Bottom) + 1] = {};
        for (PanelEntry* entry : _panels)
        {
            if (entry->Removed || entry->Slot == DockSlot::Floating)
                continue;
            const int slot = static_cast<int>(entry->Slot);
            ImGui::DockBuilderDockWindow(entry->WindowName.c_str(), _dockIds[slot]);
            if (entry->Open && !slotHasTab[slot])
            {
                slotHasTab[slot] = true;
                entry->SelectTabFrames = 3;
                entry->FocusRequest = entry->FocusRequest || entry->Slot == DockSlot::Center;
            }
        }
    }

    void SetDefaultDockSpace(ImGuiID id)
    {
        for (ImGuiID& dock : _dockIds)
            dock = id;
        _dockIds[static_cast<int>(DockSlot::Floating)] = 0;
    }

    void DrawRegisteredPanelsMenu()
    {
        bool separated = false;
        for (PanelEntry* entry : _panels)
        {
            if (entry->Removed)
                continue;
            if (!separated)
            {
                ImGui::Separator();
                separated = true;
            }
            const std::string path = entry->MenuPath.empty() ? entry->Title.substr(0, entry->Title.find("###")) : entry->MenuPath;
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

    bool HasRegisteredMenuItems(const char* topLevel)
    {
        return std::any_of(_menuItems.begin(), _menuItems.end(), [&](const MenuItemEntry* item) { return !item->Removed && item->TopLevel == topLevel; });
    }

    bool HasShellToolbar()
    {
        return _shellBars[static_cast<int>(ShellBar::Toolbar)].Callback != nullptr;
    }

    void DrawShellToolbar()
    {
        DrawShellBar(ShellBar::Toolbar);
    }

    void DrawShellStatusBar()
    {
        DrawShellBar(ShellBar::StatusBar);
    }

    void DrawRegisteredMenuItems(const char* topLevel, bool separate)
    {
        bool separated = !separate;
        for (size_t i = 0; i < _menuItems.size(); ++i)
        {
            MenuItemEntry* item = _menuItems[i];
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
        for (MenuItemEntry* item : _menuItems)
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
                DrawRegisteredMenuItems(top.c_str(), false);
                ImGui::EndMenu();
            }
        }
    }

    void ReleaseUserTextures()
    {
        UIRenderTarget* target = HostRenderTarget();
        for (TextureEntry* texture : _textures)
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
        entry->WindowName = imui::detail::PanelWindowName(desc->Title, desc->PanelIcon != imui::Icon::None
            && !HasFlag(desc->Flags, imui::PanelFlags::FillViewport));
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
        _panels.push_back(entry);

        if (entry->Open && entry->OnOpen)
            entry->OnOpen(entry->User);
        return entry;
    }

    XG_MODULE_EXPORT void XG_ImUIUnregisterPanel(imui::PanelHandle panel)
    {
        if (!IsRegistered(panel) || panel->Removed)
            return;
        panel->Removed = true;
        if (!_drawing)
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

    XG_MODULE_EXPORT void XG_ImUIDockPanel(imui::PanelHandle panel)
    {
        if (!IsRegistered(panel))
            return;
        SetOpen(panel, true);
        panel->DockRequest = true;
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
        _menuItems.push_back(item);
        return item;
    }

    XG_MODULE_EXPORT void XG_ImUIUnregisterMenuItem(imui::MenuItemHandle item)
    {
        auto it = std::find(_menuItems.begin(), _menuItems.end(), item);
        if (it == _menuItems.end() || item->Removed)
            return;
        item->Removed = true;
        if (!InFrame())
            FlushRemoved();
    }

    XG_MODULE_EXPORT void XG_ImUISetShellBar(imui::ShellBar bar, imui::PanelGUIFn callback, void* user)
    {
        if (bar != imui::ShellBar::Toolbar && bar != imui::ShellBar::StatusBar)
            return;
        _shellBars[static_cast<int>(bar)] = { callback, callback ? user : nullptr };
    }

    XG_MODULE_EXPORT void XG_ImUISetDockOutline(Color32 color)
    {
        imui::detail::_dockOutline = ToImU32(color);
    }

    XG_MODULE_EXPORT void XG_ImUIResetLayout()
    {
        imui::detail::_resetLayout = true;
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
        LayoutItem(ctx, true);
        ImGui::TextUnformatted(text ? text : "");
        return ItemResult(false);
    }

    XG_MODULE_EXPORT imui::Result XG_ImUIButton(imui::Context* ctx, const char* label)
    {
        if (!ctx)
            return imui::Result::None;
        LayoutItem(ctx);
        ImGui::Button(label ? label : "");
        return ItemResult(false);
    }

    namespace
    {
        void InspectorLabel(const char* text)
        {
            const ImRect& content = ImGui::GetCurrentWindow()->ContentRegionRect;
            const float column = content.Min.x + (std::max)(imui::detail::PropertyLabelWidth, content.GetWidth() * 0.4f);
            const float startX = ImGui::GetCursorScreenPos().x;
            ImGui::AlignTextToFramePadding();
            ImGui::PushStyleColor(ImGuiCol_Text, imui::detail::ToVec4(imui::detail::TextDim));
            ImGui::TextUnformatted(text, ImGui::FindRenderedTextEnd(text));
            ImGui::PopStyleColor();
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::SetCursorScreenPos(ImVec2((std::max)(column, startX + 24.0f), ImGui::GetCursorScreenPos().y));
            ImGui::SetNextItemWidth(-FLT_MIN);
        }

        template <typename Body>
        imui::Result PropertyRow(imui::Context* ctx, const char* label, Body body)
        {
            const char* text = label ? label : "";
            LayoutItem(ctx);
            ImGui::PushID(text);
            InspectorLabel(text);
            const bool changed = body();
            const imui::Result result = ItemResult(changed);
            ImGui::PopID();
            return result;
        }
    }

    XG_MODULE_EXPORT imui::Result XG_ImUIFieldFloat(imui::Context* ctx, const char* label, float* value, float speed)
    {
        if (!ctx || !value)
            return imui::Result::None;
        return PropertyRow(ctx, label, [&] { return ImGui::DragFloat("##value", value, speed > 0.0f ? speed : 0.1f, 0.0f, 0.0f, "%.2f"); });
    }

    XG_MODULE_EXPORT imui::Result XG_ImUIToggleButton(imui::Context* ctx, const char* label, bool* on)
    {
        if (!ctx || !on)
            return imui::Result::None;
        LayoutItem(ctx);
        const bool pressed = imui::detail::FlatButton(label ? label : "", *on);
        if (pressed)
            *on = !*on;
        return ItemResult(pressed);
    }

    XG_MODULE_EXPORT imui::Result XG_ImUIFieldInt(imui::Context* ctx, const char* label, int* value, float speed)
    {
        if (!ctx || !value)
            return imui::Result::None;
        return PropertyRow(ctx, label, [&] { return ImGui::DragInt("##value", value, speed > 0.0f ? speed : 0.2f); });
    }

    XG_MODULE_EXPORT imui::Result XG_ImUIFieldVector(imui::Context* ctx, const char* label, float* values, int count, float speed)
    {
        if (!ctx || !values || count < 1 || count > 4)
            return imui::Result::None;
        return PropertyRow(ctx, label, [&] { return ImGui::DragScalarN("##value", ImGuiDataType_Float, values, count, speed > 0.0f ? speed : 0.1f, nullptr, nullptr, "%.2f"); });
    }

    XG_MODULE_EXPORT imui::Result XG_ImUIFieldText(imui::Context* ctx, const char* label, char* buffer, int capacity)
    {
        if (!ctx || !buffer || capacity <= 0)
            return imui::Result::None;
        return PropertyRow(ctx, label, [&] { return ImGui::InputText("##value", buffer, static_cast<size_t>(capacity)); });
    }

    XG_MODULE_EXPORT imui::Result XG_ImUISliderFloat(imui::Context* ctx, const char* label, float* value, float min, float max)
    {
        if (!ctx || !value)
            return imui::Result::None;
        return PropertyRow(ctx, label, [&] { return ImGui::SliderFloat("##value", value, min, max, "%.2f"); });
    }

    XG_MODULE_EXPORT imui::Result XG_ImUISliderInt(imui::Context* ctx, const char* label, int* value, int min, int max)
    {
        if (!ctx || !value)
            return imui::Result::None;
        return PropertyRow(ctx, label, [&] { return ImGui::SliderInt("##value", value, min, max); });
    }

    XG_MODULE_EXPORT imui::Result XG_ImUICheckbox(imui::Context* ctx, const char* label, bool* value)
    {
        if (!ctx || !value)
            return imui::Result::None;
        return PropertyRow(ctx, label, [&] { return ImGui::Checkbox("##value", value); });
    }

    XG_MODULE_EXPORT imui::Result XG_ImUICombo(imui::Context* ctx, const char* label, int* index, const char* const* items, int count)
    {
        if (!ctx || !index || (!items && count > 0))
            return imui::Result::None;
        return PropertyRow(ctx, label, [&]
        {
            const char* preview = *index >= 0 && *index < count && items[*index] ? items[*index] : "";
            bool changed = false;
            if (ImGui::BeginCombo("##value", preview))
            {
                for (int i = 0; i < count; ++i)
                {
                    const bool selected = i == *index;
                    ImGui::PushID(i);
                    if (ImGui::Selectable(items[i] ? items[i] : "", selected))
                    {
                        changed = *index != i;
                        *index = i;
                    }
                    if (selected)
                        ImGui::SetItemDefaultFocus();
                    ImGui::PopID();
                }
                ImGui::EndCombo();
            }
            return changed;
        });
    }

    XG_MODULE_EXPORT imui::Result XG_ImUIFoldout(imui::Context* ctx, const char* label, bool* open)
    {
        if (!ctx || !open)
            return imui::Result::None;
        LayoutItem(ctx);
        ImGui::SetNextItemOpen(*open, ImGuiCond_Always);
        const bool nowOpen = ImGui::CollapsingHeader(label ? label : "", ImGuiTreeNodeFlags_SpanAvailWidth);
        const bool changed = nowOpen != *open;
        *open = nowOpen;
        return ItemResult(changed);
    }

    XG_MODULE_EXPORT imui::Result XG_ImUIPropertyLabel(imui::Context* ctx, const char* label, const char* value)
    {
        if (!ctx)
            return imui::Result::None;
        return PropertyRow(ctx, label, [&] { ImGui::TextUnformatted(value ? value : ""); return false; });
    }

    XG_MODULE_EXPORT imui::Result XG_ImUIIconToggle(imui::Context* ctx, const char* id, imui::Icon icon, bool* on, Color32 color, const char* text)
    {
        if (!ctx || !on)
            return imui::Result::None;
        const ImU32 col = ToImU32(color);
        const imui::detail::Icon drawIcon = (icon > imui::Icon::None && icon < imui::Icon::Count) ? static_cast<imui::detail::Icon>(icon) : imui::detail::Icon::None;
        LayoutItem(ctx);
        ImGui::BeginGroup();
        ImGui::PushID(id ? id : "##icontoggle");
        const bool pressed = imui::detail::IconButton("##toggle", drawIcon, false, imui::detail::SegButtonSize, *on ? col : imui::detail::WithAlpha(col, 0.3f));
        if (pressed)
            *on = !*on;
        const imui::Result result = ItemResult(pressed);
        if (text && *text)
        {
            ImGui::SameLine(0.0f, 2.0f);
            ImGui::AlignTextToFramePadding();
            ImGui::TextColored(imui::detail::ToVec4(*on ? imui::detail::TextColor : imui::detail::TextDim), "%s", text);
        }
        ImGui::PopID();
        ImGui::EndGroup();
        return result;
    }

    XG_MODULE_EXPORT imui::Result XG_ImUISearchField(imui::Context* ctx, const char* id, char* buffer, int capacity, float width)
    {
        if (!ctx || !buffer || capacity <= 0)
            return imui::Result::None;
        LayoutItem(ctx);
        const bool changed = imui::detail::SearchField(id ? id : "##search", buffer, static_cast<size_t>(capacity),
            width > 0.0f ? width : LayoutFill(ctx, true));
        return ItemResult(changed);
    }

    XG_MODULE_EXPORT imui::Result XG_ImUITextField(imui::Context* ctx, const char* id, const char* hint, char* buffer, int capacity, float width)
    {
        if (!ctx || !buffer || capacity <= 0)
            return imui::Result::None;
        LayoutItem(ctx);
        const float fieldWidth = width > 0.0f ? width : LayoutFill(ctx, true);
        ImGui::PushStyleColor(ImGuiCol_FrameBg, imui::detail::DropdownBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, imui::detail::DropdownHover);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, imui::detail::DropdownBg);
        ImGui::PushStyleColor(ImGuiCol_Border, imui::detail::DropdownBorder);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
        ImGui::SetNextItemWidth(fieldWidth);
        const bool changed = ImGui::InputTextWithHint(id ? id : "##textfield", hint ? hint : "", buffer, static_cast<size_t>(capacity));
        const imui::Result result = ItemResult(changed);
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
        return result;
    }

    namespace
    {
        struct TextFieldCallbackData
        {
            const char* Source;
            imui::TextFieldFlags Flags;
        };

        int TextFieldCallback(ImGuiInputTextCallbackData* data)
        {
            const TextFieldCallbackData* user = static_cast<const TextFieldCallbackData*>(data->UserData);
            if (data->EventFlag == ImGuiInputTextFlags_CallbackAlways && HasFlag(user->Flags, imui::TextFieldFlags::SetText))
            {
                if (std::strcmp(data->Buf, user->Source) != 0)
                {
                    data->DeleteChars(0, data->BufTextLen);
                    data->InsertChars(0, user->Source);
                }
                data->CursorPos = data->SelectionStart = data->SelectionEnd = data->BufTextLen;
            }
            return 0;
        }

        ImGuiKey ToImGuiKey(imui::Key key)
        {
            switch (key)
            {
            case imui::Key::Tab: return ImGuiKey_Tab;
            case imui::Key::Left: return ImGuiKey_LeftArrow;
            case imui::Key::Right: return ImGuiKey_RightArrow;
            case imui::Key::Up: return ImGuiKey_UpArrow;
            case imui::Key::Down: return ImGuiKey_DownArrow;
            case imui::Key::PageUp: return ImGuiKey_PageUp;
            case imui::Key::PageDown: return ImGuiKey_PageDown;
            case imui::Key::Home: return ImGuiKey_Home;
            case imui::Key::End: return ImGuiKey_End;
            case imui::Key::Delete: return ImGuiKey_Delete;
            case imui::Key::Backspace: return ImGuiKey_Backspace;
            case imui::Key::Enter: return ImGuiKey_Enter;
            case imui::Key::Escape: return ImGuiKey_Escape;
            default: return ImGuiKey_None;
            }
        }
    }

    XG_MODULE_EXPORT imui::Result XG_ImUITextFieldEx(imui::Context* ctx, const char* id, const char* hint, char* buffer, int capacity, float width, imui::TextFieldFlags flags)
    {
        if (!ctx || !buffer || capacity <= 0)
            return imui::Result::None;
        LayoutItem(ctx);
        const float fieldWidth = width > 0.0f ? width : LayoutFill(ctx, true);
        const std::string source(buffer);
        TextFieldCallbackData user{ source.c_str(), flags };
        ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_CallbackAlways;
        if (HasFlag(flags, imui::TextFieldFlags::CaptureNavKeys))
            inputFlags |= ImGuiInputTextFlags_CallbackCompletion | ImGuiInputTextFlags_CallbackHistory;
        ImGui::PushStyleColor(ImGuiCol_FrameBg, imui::detail::DropdownBg);
        ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, imui::detail::DropdownHover);
        ImGui::PushStyleColor(ImGuiCol_FrameBgActive, imui::detail::DropdownBg);
        ImGui::PushStyleColor(ImGuiCol_Border, imui::detail::DropdownBorder);
        ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
        ImGui::SetNextItemWidth(fieldWidth);
        const bool changed = ImGui::InputTextWithHint(id ? id : "##textfield", hint ? hint : "", buffer, static_cast<size_t>(capacity),
            inputFlags, TextFieldCallback, &user);
        const imui::Result result = ItemResult(changed || (HasFlag(flags, imui::TextFieldFlags::SetText) && source != buffer));
        ImGui::PopStyleVar(2);
        ImGui::PopStyleColor(4);
        return result;
    }

    XG_MODULE_EXPORT bool XG_ImUIIsKeyPressed(imui::Context* ctx, imui::Key key, bool repeat)
    {
        const ImGuiKey imKey = ToImGuiKey(key);
        return ctx && imKey != ImGuiKey_None && ImGui::IsKeyPressed(imKey, repeat);
    }

    XG_MODULE_EXPORT bool XG_ImUIIsItemActive(imui::Context* ctx)
    {
        return ctx && ImGui::IsItemActive();
    }

    XG_MODULE_EXPORT void XG_ImUISetKeyboardFocusHere(imui::Context* ctx)
    {
        if (ctx)
            ImGui::SetKeyboardFocusHere(0);
    }

    XG_MODULE_EXPORT void XG_ImUISetClipboardText(imui::Context* ctx, const char* text)
    {
        if (ctx && text)
            ImGui::SetClipboardText(text);
    }

    XG_MODULE_EXPORT bool XG_ImUIGetItemRect(imui::Context* ctx, Rect* out)
    {
        if (!ctx || !out)
            return false;
        const ImVec2 origin = ctx->Origin();
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        interop::Store(out, Rect(Vec2(min.x - origin.x, min.y - origin.y), Vec2(max.x - origin.x, max.y - origin.y)));
        return true;
    }

    XG_MODULE_EXPORT bool XG_ImUIGetMousePosition(imui::Context* ctx, Vec2* out)
    {
        if (!ctx || !out)
            return false;
        const ImVec2 origin = ctx->Origin();
        const ImVec2 mouse = ImGui::GetMousePos();
        interop::Store(out, Vec2(mouse.x - origin.x, mouse.y - origin.y));
        return true;
    }

    XG_MODULE_EXPORT imui::Result XG_ImUIIconButton(imui::Context* ctx, const char* id, imui::Icon icon, const Vec2* position, float size, Color32 color)
    {
        if (!ctx)
            return imui::Result::None;
        const float side = size > 0.0f ? size : ImGui::GetFrameHeight();
        if (!position)
            LayoutItem(ctx);
        const CursorState restore = CursorState::Save();
        if (position)
            ImGui::SetCursorScreenPos(ctx->ToScreen(interop::Load(position)));
        ImGui::InvisibleButton(id ? id : "##iconbutton", ImVec2(side, side));
        const imui::Result result = ItemResult(false);
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (ImGui::IsItemActive())
            dl->AddRectFilled(min, max, imui::detail::WithAlpha(IM_COL32_WHITE, 0.16f), 4.0f);
        else if (ImGui::IsItemHovered())
            dl->AddRectFilled(min, max, imui::detail::WithAlpha(IM_COL32_WHITE, 0.09f), 4.0f);
        if (icon > imui::Icon::None && icon < imui::Icon::Count)
            imui::detail::DrawIcon(dl, static_cast<imui::detail::Icon>(icon), ImVec2((min.x + max.x) * 0.5f, (min.y + max.y) * 0.5f),
                side * 0.62f, ToImU32(color));
        if (position)
            restore.Restore();
        return HasFlag(result, imui::Result::Clicked) ? result | imui::Result::Changed : result;
    }

    XG_MODULE_EXPORT imui::Result XG_ImUIHitArea(imui::Context* ctx, const char* id, const Rect* rect)
    {
        if (!ctx || !rect)
            return imui::Result::None;
        const Rect r = interop::Load(rect);
        if (r.Width() <= 0.0f || r.Height() <= 0.0f)
            return imui::Result::None;
        const CursorState restore = CursorState::Save();
        ImGui::SetCursorScreenPos(ctx->ToScreen(r.min));
        ImGui::SetNextItemAllowOverlap();
        ImGui::InvisibleButton(id ? id : "##hitarea", ImVec2(r.Width(), r.Height()),
            ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
        const imui::Result result = ItemResult(false);
        restore.Restore();
        return result;
    }

    XG_MODULE_EXPORT void XG_ImUIItemTooltip(imui::Context* ctx, const char* text)
    {
        if (ctx && text && *text)
            ImGui::SetItemTooltip("%s", text);
    }

    // ------------------------------------------------------------
    // Modals
    // ------------------------------------------------------------
    XG_MODULE_EXPORT void XG_ImUIOpenPopup(imui::Context* ctx, const char* id)
    {
        if (ctx && id)
            ImGui::OpenPopup((std::string("###") + id).c_str());
    }

    XG_MODULE_EXPORT bool XG_ImUIBeginModal(imui::Context* ctx, const char* id, const char* title, const Vec2* size)
    {
        if (!ctx || !id)
            return false;
        std::string name = title ? title : "";
        name += "###";
        name += id;
        const Vec2 requested = LoadVec2(size);
        const ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (requested.x > 0.0f && requested.y <= 0.0f)
            ImGui::SetNextWindowSizeConstraints(ImVec2(requested.x, 0.0f), ImVec2(requested.x, FLT_MAX));
        else
            ImGui::SetNextWindowSize(ImVec2((std::max)(requested.x, 0.0f), (std::max)(requested.y, 0.0f)), ImGuiCond_Appearing);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(14.0f, 12.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
        ImGui::PushStyleColor(ImGuiCol_PopupBg, imui::detail::DropdownBg);
        ImGui::PushStyleColor(ImGuiCol_ModalWindowDimBg, IM_COL32(0, 0, 0, 120));
        const bool open = ImGui::BeginPopupModal(name.c_str(), nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse
            | (requested.y <= 0.0f ? ImGuiWindowFlags_AlwaysAutoResize : ImGuiWindowFlags_None));
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
        if (!open)
            return false;
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
            ImGui::CloseCurrentPopup();
        BeginLayout(ctx, false, -1.0f, false);
        return true;
    }

    XG_MODULE_EXPORT void XG_ImUIEndModal(imui::Context* ctx)
    {
        if (!ctx)
            return;
        const imui::Context::LayoutFrame* frame = CurrentLayout(ctx);
        if (frame && !frame->Group)
            EndLayout(ctx);
        ImGui::EndPopup();
    }

    XG_MODULE_EXPORT void XG_ImUICloseModal(imui::Context* ctx)
    {
        if (ctx)
            ImGui::CloseCurrentPopup();
    }

    XG_MODULE_EXPORT bool XG_ImUIBeginPopup(imui::Context* ctx, const char* id, const Vec2* position, const Vec2* size)
    {
        if (!ctx || !id)
            return false;
        const std::string name = std::string("###") + id;
        if (!ImGui::IsPopupOpen(name.c_str()))
            return false;
        const Vec2 requested = LoadVec2(size);
        ImGui::SetNextWindowPos(ctx->ToScreen(LoadVec2(position)), ImGuiCond_Always);
        if (requested.x > 0.0f)
            ImGui::SetNextWindowSizeConstraints(ImVec2(requested.x, requested.y > 0.0f ? requested.y : 0.0f),
                ImVec2(requested.x, requested.y > 0.0f ? requested.y : FLT_MAX));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_PopupBg, imui::detail::HeaderBg);
        ImGui::PushStyleColor(ImGuiCol_Border, imui::detail::DropdownBorder);
        const bool open = ImGui::BeginPopup(name.c_str(),
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);
        if (!open)
            return false;
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) && ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))
            ImGui::CloseCurrentPopup();
        BeginLayout(ctx, false, -1.0f, false);
        return true;
    }

    XG_MODULE_EXPORT void XG_ImUIEndPopup(imui::Context* ctx)
    {
        XG_ImUIEndModal(ctx);
    }

    XG_MODULE_EXPORT bool XG_ImUIBeginOverlay(imui::Context* ctx, const char* id, const Vec2* position, const Vec2* size)
    {
        if (!ctx || !id)
            return false;
        const std::string name = std::string("###overlay_") + id;
        const Vec2 requested = LoadVec2(size);
        ImGui::SetNextWindowPos(ctx->ToScreen(LoadVec2(position)), ImGuiCond_Always);
        if (requested.x > 0.0f)
            ImGui::SetNextWindowSizeConstraints(ImVec2(requested.x, requested.y > 0.0f ? requested.y : 0.0f),
                ImVec2(requested.x, requested.y > 0.0f ? requested.y : FLT_MAX));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowMinSize, ImVec2(1.0f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_WindowBg, imui::detail::HeaderBg);
        ImGui::PushStyleColor(ImGuiCol_Border, imui::detail::DropdownBorder);
        ImGui::Begin(name.c_str(), nullptr,
            ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings
            | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoCollapse
            | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_AlwaysAutoResize);
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(4);
        ImGui::BringWindowToDisplayFront(ImGui::GetCurrentWindow());
        BeginLayout(ctx, false, -1.0f, false);
        return true;
    }

    XG_MODULE_EXPORT void XG_ImUIEndOverlay(imui::Context* ctx)
    {
        if (!ctx)
            return;
        const imui::Context::LayoutFrame* frame = CurrentLayout(ctx);
        if (frame && !frame->Group)
            EndLayout(ctx);
        ImGui::End();
    }

    XG_MODULE_EXPORT imui::Result XG_ImUITextArea(imui::Context* ctx, const char* id, const char* text, const Vec2* size)
    {
        if (!ctx)
            return imui::Result::None;
        const char* value = text ? text : "";
        const Vec2 requested = LoadVec2(size);
        LayoutItem(ctx);
        const ImVec2 areaSize(requested.x > 0.0f ? requested.x : LayoutFill(ctx, true), requested.y > 0.0f ? requested.y : LayoutFill(ctx, false));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, imui::detail::AreaBg);
        ImGui::InputTextMultiline(id ? id : "##textarea", const_cast<char*>(value), std::strlen(value) + 1, areaSize, ImGuiInputTextFlags_ReadOnly);
        ImGui::PopStyleColor();
        return ItemResult(false);
    }

    // ------------------------------------------------------------
    // Fonts
    // ------------------------------------------------------------
    XG_MODULE_EXPORT void XG_ImUIPushFont(imui::Context* ctx, imui::FontStyle style)
    {
        if (!ctx)
            return;
        static constexpr int FontIndex[] = { 0, 2, 3 };
        const int slot = (style >= imui::FontStyle::Default && style <= imui::FontStyle::Mono) ? FontIndex[static_cast<int>(style)] : 0;
        ImFontAtlas* atlas = ImGui::GetIO().Fonts;
        ImFont* font = slot < atlas->Fonts.Size ? atlas->Fonts[slot] : ImGui::GetFont();
        ImGui::PushFont(font, font ? font->LegacySize : 0.0f);
    }

    XG_MODULE_EXPORT void XG_ImUIPopFont(imui::Context* ctx)
    {
        if (ctx)
            ImGui::PopFont();
    }

    // ------------------------------------------------------------
    // Layout
    // ------------------------------------------------------------
    XG_MODULE_EXPORT void XG_ImUIBeginToolbar(imui::Context* ctx)
    {
        if (!ctx)
            return;
        LayoutItem(ctx);
        ImGui::PushID(ctx->ToolbarCount++);
        imui::detail::BeginHeader();
        BeginLayout(ctx, true, -1.0f, true);
    }

    XG_MODULE_EXPORT void XG_ImUIEndToolbar(imui::Context* ctx)
    {
        if (!ctx)
            return;
        EndLayout(ctx);
        imui::detail::EndHeader();
        ImGui::PopID();
    }

    XG_MODULE_EXPORT void XG_ImUIBeginHorizontal(imui::Context* ctx, float spacing)
    {
        if (ctx)
            BeginLayout(ctx, true, spacing, true);
    }

    XG_MODULE_EXPORT void XG_ImUIEndHorizontal(imui::Context* ctx)
    {
        if (ctx)
            EndLayout(ctx);
    }

    XG_MODULE_EXPORT void XG_ImUIBeginVertical(imui::Context* ctx)
    {
        if (ctx)
            BeginLayout(ctx, false, -1.0f, true);
    }

    XG_MODULE_EXPORT void XG_ImUIEndVertical(imui::Context* ctx)
    {
        if (ctx)
            EndLayout(ctx);
    }

    XG_MODULE_EXPORT void XG_ImUIFlexibleSpace(imui::Context* ctx)
    {
        imui::Context::LayoutFrame* frame = CurrentLayout(ctx);
        if (!frame)
            return;
        const bool horizontal = frame->Horizontal;
        LayoutItem(ctx);
        const float share = LayoutFill(ctx, horizontal);
        ImGui::Dummy(horizontal ? ImVec2(share, 0.0f) : ImVec2(0.0f, share));
    }

    XG_MODULE_EXPORT void XG_ImUISpace(imui::Context* ctx, float pixels)
    {
        if (!ctx || pixels <= 0.0f)
            return;
        imui::Context::LayoutFrame* frame = CurrentLayout(ctx);
        if (frame && frame->Horizontal && frame->Items > 0)
        {
            frame->PendingSpace += pixels;
            return;
        }
        LayoutItem(ctx);
        ImGui::Dummy(frame && frame->Horizontal ? ImVec2(pixels, 0.0f) : ImVec2(0.0f, pixels));
    }

    XG_MODULE_EXPORT void XG_ImUISameLine(imui::Context* ctx, float spacing)
    {
        if (ctx)
            ImGui::SameLine(0.0f, spacing < 0.0f ? -1.0f : spacing);
    }

    XG_MODULE_EXPORT void XG_ImUIAlignRight(imui::Context* ctx, float width)
    {
        if (ctx)
            imui::detail::AlignRight(width);
    }

    XG_MODULE_EXPORT interop::Vec2Abi XG_ImUIAvailableSize(imui::Context* ctx)
    {
        if (!ctx)
            return interop::ToAbi(Vec2());
        const ImVec2 avail = ImGui::GetContentRegionAvail();
        return interop::ToAbi(Vec2(avail.x, avail.y));
    }

    XG_MODULE_EXPORT imui::Result XG_ImUISplitter(imui::Context* ctx, const char* id, float* size, float minSize, float maxSize)
    {
        if (!ctx || !size)
            return imui::Result::None;
        constexpr float Thickness = 4.0f;
        LayoutItem(ctx);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float width = (std::max)(ImGui::GetContentRegionAvail().x, 1.0f);
        ImGui::InvisibleButton(id ? id : "##splitter", ImVec2(width, Thickness));
        bool changed = false;
        if (ImGui::IsItemActive())
        {
            const float next = (std::clamp)(*size - ImGui::GetIO().MouseDelta.y, minSize, (std::max)(minSize, maxSize));
            changed = next != *size;
            *size = next;
        }
        const imui::Result result = ItemResult(changed);
        const bool hot = ImGui::IsItemHovered() || ImGui::IsItemActive();
        if (hot)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
        const float y = p.y + Thickness * 0.5f;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x, y), ImVec2(p.x + width, y), hot ? imui::detail::Accent : imui::detail::Outline, hot ? 2.0f : 1.0f);
        return result;
    }

    XG_MODULE_EXPORT imui::Result XG_ImUISplitterVertical(imui::Context* ctx, const char* id, float* size, float minSize, float maxSize)
    {
        if (!ctx || !size)
            return imui::Result::None;
        constexpr float Thickness = 4.0f;
        LayoutItem(ctx);
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float height = (std::max)(ImGui::GetContentRegionAvail().y, 1.0f);
        ImGui::InvisibleButton(id ? id : "##vsplitter", ImVec2(Thickness, height));
        bool changed = false;
        if (ImGui::IsItemActive())
        {
            const float next = (std::clamp)(*size + ImGui::GetIO().MouseDelta.x, minSize, (std::max)(minSize, maxSize));
            changed = next != *size;
            *size = next;
        }
        const imui::Result result = ItemResult(changed);
        const bool hot = ImGui::IsItemHovered() || ImGui::IsItemActive();
        if (hot)
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        const float x = p.x + Thickness * 0.5f;
        ImGui::GetWindowDrawList()->AddLine(ImVec2(x, p.y), ImVec2(x, p.y + height), hot ? imui::detail::Accent : imui::detail::Outline, hot ? 2.0f : 1.0f);
        return result;
    }

    // ------------------------------------------------------------
    // Virtualized lists
    // ------------------------------------------------------------
    XG_MODULE_EXPORT bool XG_ImUIBeginList(imui::Context* ctx, const char* id, const Vec2* size, imui::ListState* state)
    {
        if (!ctx || !state || state->Size < sizeof(imui::ListState) || ctx->ListDepth >= imui::Context::MaxListDepth)
            return false;

        const Vec2 requested = LoadVec2(size);
        LayoutItem(ctx);
        const ImVec2 listSize(requested.x > 0.0f ? requested.x : LayoutFill(ctx, true), requested.y > 0.0f ? requested.y : LayoutFill(ctx, false));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, imui::detail::AreaBg);
        ImGui::BeginChild(id ? id : "##list", listSize, ImGuiChildFlags_None, ImGuiWindowFlags_None);
        ImGui::PopStyleColor();
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0.0f, 0.0f));

        const int count = (std::max)(state->Count, 0);
        const float rowHeight = (std::max)(state->RowHeight, 1.0f);
        const float scrollY = ImGui::GetScrollY();
        const float viewHeight = ImGui::GetWindowHeight();
        const int first = (std::clamp)(static_cast<int>(scrollY / rowHeight), 0, count);
        const int last = (std::clamp)(static_cast<int>((scrollY + viewHeight) / rowHeight) + 1, first, count);
        const bool atBottom = ImGui::GetScrollMaxY() <= 0.0f || scrollY >= ImGui::GetScrollMaxY() - 1.0f;

        state->First = first;
        state->Last = last;
        const bool scrollToBottom = HasFlag(state->Flags, imui::ListFlags::ScrollToBottom);
        const bool noStripes = HasFlag(state->Flags, imui::ListFlags::NoStripes);
        state->Flags = (atBottom ? imui::ListFlags::AtBottom : imui::ListFlags::None) | (noStripes ? imui::ListFlags::NoStripes : imui::ListFlags::None);

        imui::Context::ListFrame& frame = ctx->Lists[ctx->ListDepth++];
        frame.Count = count;
        frame.Last = last;
        frame.RowHeight = rowHeight;
        frame.ScrollToBottom = scrollToBottom;
        frame.NoStripes = noStripes;

        if (first > 0)
            ImGui::Dummy(ImVec2(1.0f, first * rowHeight));
        return true;
    }

    XG_MODULE_EXPORT void XG_ImUIEndList(imui::Context* ctx)
    {
        if (!ctx || ctx->ListDepth <= 0)
            return;
        const imui::Context::ListFrame frame = ctx->Lists[--ctx->ListDepth];
        if (frame.Count > frame.Last)
            ImGui::Dummy(ImVec2(1.0f, (frame.Count - frame.Last) * frame.RowHeight));
        if (frame.ScrollToBottom)
            ImGui::SetScrollHereY(1.0f);
        ImGui::PopStyleVar();
        ImGui::EndChild();
    }

    XG_MODULE_EXPORT imui::Result XG_ImUIBeginListRow(imui::Context* ctx, int index, bool selected)
    {
        if (!ctx || ctx->ListDepth <= 0 || ctx->CanvasDepth >= imui::Context::MaxCanvasDepth)
            return imui::Result::None;
        const float rowHeight = ctx->Lists[ctx->ListDepth - 1].RowHeight;
        const ImVec2 rowMin = ImGui::GetCursorScreenPos();
        const float width = (std::max)(ImGui::GetContentRegionAvail().x, 1.0f);
        const ImVec2 rowMax(rowMin.x + width, rowMin.y + rowHeight);
        if (index % 2 && !selected && !ctx->Lists[ctx->ListDepth - 1].NoStripes)
            ImGui::GetWindowDrawList()->AddRectFilled(rowMin, rowMax, imui::detail::RowAltBg);
        ImGui::PushID(index);
        const bool pressed = ImGui::Selectable("##row", selected, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowOverlap, ImVec2(width, rowHeight));
        const imui::Result result = ItemResult(pressed) | RowHoverResult(rowMin, rowMax);
        ImGui::PopID();

        ctx->CanvasOrigins[ctx->CanvasDepth++] = rowMin;
        ImGui::GetWindowDrawList()->PushClipRect(rowMin, rowMax, true);
        return result;
    }

    XG_MODULE_EXPORT void XG_ImUIEndListRow(imui::Context* ctx)
    {
        XG_ImUIEndCanvas(ctx);
    }

    XG_MODULE_EXPORT imui::Result XG_ImUIBeginTreeRow(imui::Context* ctx, int index, int depth, imui::TreeRowFlags flags)
    {
        if (!ctx || ctx->ListDepth <= 0 || ctx->CanvasDepth >= imui::Context::MaxCanvasDepth)
            return imui::Result::None;
        constexpr float Indent = 14.0f;
        constexpr float ArrowWidth = 16.0f;
        const float rowHeight = ctx->Lists[ctx->ListDepth - 1].RowHeight;
        const ImVec2 rowMin = ImGui::GetCursorScreenPos();
        const float width = (std::max)(ImGui::GetContentRegionAvail().x, 1.0f);
        const ImVec2 rowMax(rowMin.x + width, rowMin.y + rowHeight);
        const bool selected = HasFlag(flags, imui::TreeRowFlags::Selected);
        ImDrawList* dl = ImGui::GetWindowDrawList();
        if (index % 2 && !selected)
            dl->AddRectFilled(rowMin, rowMax, imui::detail::RowAltBg);

        ImGui::PushID(index);
        ImGui::Selectable("##row", selected, ImGuiSelectableFlags_AllowDoubleClick | ImGuiSelectableFlags_AllowOverlap, ImVec2(width, rowHeight));
        imui::Result result = ItemResult(false) | RowHoverResult(rowMin, rowMax);

        const float arrowX = rowMin.x + 4.0f + (std::max)(depth, 0) * Indent;
        const float cy = rowMin.y + rowHeight * 0.5f;
        if (HasFlag(flags, imui::TreeRowFlags::HasChildren))
        {
            ImGui::SetCursorScreenPos(ImVec2(arrowX, rowMin.y));
            if (ImGui::InvisibleButton("##arrow", ImVec2(ArrowWidth, rowHeight)))
                result = result | imui::Result::Changed;
            const ImU32 col = ImGui::IsItemHovered() ? imui::detail::TextColor : imui::detail::TextDim;
            const float ax = arrowX + 4.0f;
            if (HasFlag(flags, imui::TreeRowFlags::Expanded))
                dl->AddTriangleFilled(ImVec2(ax, cy - 3.0f), ImVec2(ax + 8.0f, cy - 3.0f), ImVec2(ax + 4.0f, cy + 3.0f), col);
            else
                dl->AddTriangleFilled(ImVec2(ax + 1.0f, cy - 4.0f), ImVec2(ax + 7.0f, cy), ImVec2(ax + 1.0f, cy + 4.0f), col);
        }
        ImGui::PopID();

        ctx->CanvasOrigins[ctx->CanvasDepth++] = ImVec2(arrowX + ArrowWidth, rowMin.y);
        dl->PushClipRect(rowMin, rowMax, true);
        return result;
    }

    XG_MODULE_EXPORT void XG_ImUIEndTreeRow(imui::Context* ctx)
    {
        XG_ImUIEndCanvas(ctx);
    }

    // ------------------------------------------------------------
    // Scroll areas and panel placement
    // ------------------------------------------------------------
    XG_MODULE_EXPORT bool XG_ImUIBeginScrollArea(imui::Context* ctx, const char* id, const Vec2* size, imui::ScrollAreaFlags flags)
    {
        if (!ctx || ctx->ScrollDepth >= imui::Context::MaxScrollDepth)
            return false;

        const Vec2 requested = LoadVec2(size);
        LayoutItem(ctx);
        const ImVec2 areaSize(requested.x > 0.0f ? requested.x : LayoutFill(ctx, true), requested.y > 0.0f ? requested.y : LayoutFill(ctx, false));
        const bool horizontal = HasFlag(flags, imui::ScrollAreaFlags::Horizontal);
        const ImGuiWindowFlags windowFlags = horizontal ? ImGuiWindowFlags_HorizontalScrollbar : ImGuiWindowFlags_None;
        const bool padded = HasFlag(flags, imui::ScrollAreaFlags::Padded);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, padded ? ImVec2(6.0f, 6.0f) : ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
        ImGui::BeginChild(id ? id : "##scroll", areaSize, padded ? ImGuiChildFlags_AlwaysUseWindowPadding : ImGuiChildFlags_None, windowFlags);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        if (horizontal && ImGui::IsWindowHovered() && ImGui::GetScrollMaxX() > 0.0f && ImGui::GetScrollMaxY() <= 0.0f)
        {
            const float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f)
                ImGui::SetScrollX(ImGui::GetScrollX() - wheel * ImGui::GetFontSize() * 4.0f);
        }

        ctx->ScrollOrigins[ctx->ScrollDepth++] = ctx->ContentOrigin;
        ctx->ContentOrigin = ImGui::GetCursorScreenPos();
        BeginLayout(ctx, horizontal, -1.0f, false);
        return true;
    }

    XG_MODULE_EXPORT void XG_ImUIEndScrollArea(imui::Context* ctx)
    {
        if (!ctx || ctx->ScrollDepth <= 0)
            return;
        EndLayout(ctx);
        ImGui::EndChild();
        ctx->ContentOrigin = ctx->ScrollOrigins[--ctx->ScrollDepth];
    }

    XG_MODULE_EXPORT bool XG_ImUIBeginRegion(imui::Context* ctx, const char* id, const Rect* rect, Vec2* contentSize)
    {
        if (!ctx || !rect || ctx->RegionDepth >= imui::Context::MaxRegionDepth)
            return false;
        const Rect r = interop::Load(rect);
        if (r.Width() <= 0.0f || r.Height() <= 0.0f)
            return false;

        const int depth = ctx->RegionDepth++;
        _regionCursors[depth] = CursorState::Save();
        imui::Context::RegionFrame& frame = ctx->Regions[depth];
        frame.ContentOrigin = ctx->ContentOrigin;
        frame.CanvasDepth = ctx->CanvasDepth;

        ImGui::SetCursorScreenPos(ctx->ToScreen(r.min));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleColor(ImGuiCol_ChildBg, IM_COL32(0, 0, 0, 0));
        ImGui::BeginChild(id ? id : "##region", ImVec2(r.Width(), r.Height()), ImGuiChildFlags_None,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        if (contentSize)
        {
            const ImVec2 content = ImGui::GetCurrentWindow()->ContentSize;
            interop::Store(contentSize, Vec2(content.x, content.y));
        }
        ctx->CanvasDepth = 0;
        ctx->ContentOrigin = ImGui::GetCursorScreenPos();
        BeginLayout(ctx, false, -1.0f, false);
        return true;
    }

    XG_MODULE_EXPORT void XG_ImUIEndRegion(imui::Context* ctx)
    {
        if (!ctx || ctx->RegionDepth <= 0)
            return;
        EndLayout(ctx);
        ImGui::EndChild();
        const int depth = --ctx->RegionDepth;
        const imui::Context::RegionFrame& frame = ctx->Regions[depth];
        ctx->ContentOrigin = frame.ContentOrigin;
        ctx->CanvasDepth = frame.CanvasDepth;
        _regionCursors[depth].Restore();
    }

    XG_MODULE_EXPORT bool XG_ImUIBeginCard(imui::Context* ctx, const char* id, const char* title, imui::Icon icon, imui::TextureHandle texture, Color32 iconColor, bool* open)
    {
        if (!ctx)
            return false;
        if (ctx->CardDepth >= imui::Context::MaxCardDepth)
        {
            ++ctx->CardOverflow;
            return false;
        }

        constexpr float HeaderHeight = 22.0f;
        LayoutItem(ctx);
        const float width = (std::max)(ImGui::GetContentRegionAvail().x, 1.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, imui::detail::HeaderBg);
        ImGui::PushStyleColor(ImGuiCol_Border, imui::detail::DropdownBorder);
        ImGui::BeginChild(id ? id : "##card", ImVec2(width, 0.0f), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_Borders,
            ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(3);

        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float headerWidth = (std::max)(ImGui::GetContentRegionAvail().x, 1.0f);
        ImGui::InvisibleButton("##card_header", ImVec2(headerWidth, HeaderHeight));
        const bool hovered = open && ImGui::IsItemHovered();
        if (open && ImGui::IsItemClicked(ImGuiMouseButton_Left))
            *open = !*open;
        const bool bodyOpen = !open || *open;

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 q(p.x + headerWidth, p.y + HeaderHeight);
        dl->AddRectFilled(p, q, hovered ? imui::detail::PanelHeaderHov : imui::detail::PanelHeaderBg, 4.0f,
            bodyOpen ? ImDrawFlags_RoundCornersTop : ImDrawFlags_RoundCornersAll);
        if (bodyOpen)
            dl->AddLine(ImVec2(p.x, q.y - 0.5f), ImVec2(q.x, q.y - 0.5f), imui::detail::SegmentBg);
        const float cy = p.y + HeaderHeight * 0.5f;
        float x = p.x + 8.0f;
        if (open)
        {
            const ImU32 arrow = imui::detail::TextDim;
            if (*open)
                dl->AddTriangleFilled(ImVec2(x - 1.0f, cy - 2.5f), ImVec2(x + 7.0f, cy - 2.5f), ImVec2(x + 3.0f, cy + 2.5f), arrow);
            else
                dl->AddTriangleFilled(ImVec2(x + 1.0f, cy - 4.0f), ImVec2(x + 6.0f, cy), ImVec2(x + 1.0f, cy + 4.0f), arrow);
            x += 14.0f;
        }
        constexpr float IconSize = 14.0f;
        if (texture && texture->Data)
        {
            dl->AddImage(texture->Data->GetTexRef(), ImVec2(x, cy - IconSize * 0.5f), ImVec2(x + IconSize, cy + IconSize * 0.5f));
            x += IconSize + 6.0f;
        }
        else if (icon > imui::Icon::None && icon < imui::Icon::Count)
        {
            imui::detail::DrawIcon(dl, static_cast<imui::detail::Icon>(icon), ImVec2(x + IconSize * 0.5f, cy), IconSize, ToImU32(iconColor));
            x += IconSize + 6.0f;
        }
        if (title && title[0])
        {
            XG_ImUIPushFont(ctx, imui::FontStyle::Bold);
            dl->AddText(ImVec2(x, IM_ROUND(cy - ImGui::GetFontSize() * 0.5f)), imui::detail::TextColor, title);
            XG_ImUIPopFont(ctx);
        }

        imui::Context::CardFrame& frame = ctx->Cards[ctx->CardDepth++];
        frame.Origin = ctx->ContentOrigin;
        frame.BodyOpen = bodyOpen;
        if (bodyOpen)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(6.0f, 4.0f));
            ImGui::BeginChild("##card_body", ImVec2(headerWidth, 0.0f), ImGuiChildFlags_AutoResizeY | ImGuiChildFlags_AlwaysUseWindowPadding,
                ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
            ImGui::PopStyleVar();
            ctx->ContentOrigin = ImGui::GetCursorScreenPos();
            BeginLayout(ctx, false, -1.0f, false);
        }
        return bodyOpen;
    }

    XG_MODULE_EXPORT void XG_ImUIEndCard(imui::Context* ctx)
    {
        if (!ctx)
            return;
        if (ctx->CardOverflow > 0)
        {
            --ctx->CardOverflow;
            return;
        }
        if (ctx->CardDepth <= 0)
            return;
        const imui::Context::CardFrame frame = ctx->Cards[--ctx->CardDepth];
        if (frame.BodyOpen)
        {
            EndLayout(ctx);
            ImGui::EndChild();
        }
        ImGui::EndChild();
        ctx->ContentOrigin = frame.Origin;
    }

    XG_MODULE_EXPORT bool XG_ImUIGetPanelPlacement(imui::Context* ctx, imui::PanelPlacement* out)
    {
        if (!ctx || !out || !ctx->PanelWindow)
            return false;
        const imui::PanelPlacement current = interop::Load(out);
        if (current.Size < sizeof(imui::PanelPlacement))
            return false;

        const auto toRect = [](const ImVec2& pos, const ImVec2& size) { return Rect::FromXYWH(pos.x, pos.y, size.x, size.y); };
        ImGuiWindow* window = ctx->PanelWindow;
        imui::PanelPlacement placement;
        placement.Window = toRect(window->Pos, window->Size);
        placement.DockNode = placement.Window;
        placement.DockSpace = placement.Window;
        if (window->DockIsActive && window->DockNode)
        {
            ImGuiDockNode* root = ImGui::DockNodeGetRootNode(window->DockNode);
            placement.DockNode = toRect(window->DockNode->Pos, window->DockNode->Size);
            placement.DockSpace = toRect(root->Pos, root->Size);
            placement.Flags = placement.Flags | imui::PanelPlacementFlags::Docked;
        }
        if (window->Viewport == ImGui::GetMainViewport())
            placement.Flags = placement.Flags | imui::PanelPlacementFlags::MainViewport;
        interop::Store(out, placement);
        return true;
    }

    // ------------------------------------------------------------
    // Immediate drawing
    // ------------------------------------------------------------
    XG_MODULE_EXPORT imui::Result XG_ImUIBeginCanvas(imui::Context* ctx, const char* id, const Vec2* size, Rect* outRect)
    {
        if (!ctx || ctx->CanvasDepth >= imui::Context::MaxCanvasDepth)
            return imui::Result::None;

        const Vec2 requested = LoadVec2(size);
        LayoutItem(ctx);
        const ImVec2 canvasSize(requested.x > 0.0f ? requested.x : LayoutFill(ctx, true), requested.y > 0.0f ? requested.y : LayoutFill(ctx, false));
        ImGui::SetNextItemAllowOverlap();
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

    XG_MODULE_EXPORT void XG_ImUIDrawTextRotated(imui::Context* ctx, const Vec2* position, Color32 color, const char* text, int quarterTurns)
    {
        if (!ctx || !text || !text[0])
            return;
        const int turns = ((quarterTurns % 4) + 4) % 4;
        ImDrawList* dl = ctx->DrawList();
        const ImVec2 topLeft = ctx->ToScreen(LoadVec2(position));
        if (turns == 0)
        {
            dl->AddText(topLeft, ToImU32(color), text);
            return;
        }

        const ImVec2 size = ImGui::CalcTextSize(text);
        ImVec2 pivot = topLeft;
        if (turns == 1)
            pivot.x += size.y;
        else if (turns == 2)
            pivot = ImVec2(pivot.x + size.x, pivot.y + size.y);
        else
            pivot.y += size.x;

        pivot = ImVec2(IM_ROUND(pivot.x), IM_ROUND(pivot.y));

        const int first = dl->VtxBuffer.Size;
        const ImVec4 unclipped(-FLT_MAX, -FLT_MAX, FLT_MAX, FLT_MAX);
        ImGui::GetFont()->RenderText(dl, ImGui::GetFontSize(), pivot, ToImU32(color), unclipped, text, nullptr, 0.0f, ImDrawTextFlags_None);
        ImDrawVert* vertices = dl->VtxBuffer.Data;
        const int end = dl->VtxBuffer.Size;
        for (int i = first; i < end; ++i)
        {
            ImVec2& p = vertices[i].pos;
            const float dx = p.x - pivot.x;
            const float dy = p.y - pivot.y;
            if (turns == 1)
                p = ImVec2(pivot.x - dy, pivot.y + dx);
            else if (turns == 2)
                p = ImVec2(pivot.x - dx, pivot.y - dy);
            else
                p = ImVec2(pivot.x + dy, pivot.y - dx);
        }
        for (int quad = first; quad + 3 < end; quad += 4)
        {
            ImVec2 min = vertices[quad].pos;
            for (int k = 1; k < 4; ++k)
                min = ImMin(min, vertices[quad + k].pos);
            const ImVec2 snap(IM_ROUND(min.x) - min.x, IM_ROUND(min.y) - min.y);
            for (int k = 0; k < 4; ++k)
            {
                vertices[quad + k].pos.x += snap.x;
                vertices[quad + k].pos.y += snap.y;
            }
        }
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
            TextOnAccent, PanelHeaderHov,
            TopBarBg, SegmentBg, ButtonBg, ButtonHover, DropdownBorder,
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
            | (active ? imui::SceneViewFlags::Active : imui::SceneViewFlags::None)
            | (active && ImGui::IsMouseDown(ImGuiMouseButton_Left) ? imui::SceneViewFlags::LeftDown : imui::SceneViewFlags::None)
            | (active && ImGui::IsMouseDown(ImGuiMouseButton_Right) ? imui::SceneViewFlags::RightDown : imui::SceneViewFlags::None)
            | (active && ImGui::IsMouseDown(ImGuiMouseButton_Middle) ? imui::SceneViewFlags::MiddleDown : imui::SceneViewFlags::None);

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
        _textures.push_back(entry);
        return entry;
    }

    XG_MODULE_EXPORT void XG_ImUIDestroyTexture(imui::TextureHandle texture)
    {
        auto it = std::find(_textures.begin(), _textures.end(), texture);
        if (it == _textures.end())
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
        _textures.erase(it);
        delete texture;
    }
}
