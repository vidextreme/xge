// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "xgImUI.h"
#include "ImUIInternal.h"
#include "ImUIHost.h"
#include "xgUIDraw.h"
#include "xgEnv.h"

#include <algorithm>
#include <cctype>
#include <cfloat>
#include <cmath>
#include <cstdlib>
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
        const imui::Result result = ItemResult(false);
        const ImVec2 min = ImGui::GetItemRectMin();
        const ImVec2 max = ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddLine(ImVec2(min.x + 2.0f, max.y - 0.5f), ImVec2(max.x - 2.0f, max.y - 0.5f), imui::detail::SegmentBg, 1.0f);
        return result;
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

        struct NotchTag
        {
            std::string Text;
            ImU32 Color = 0;
        };

        std::string Trim(const std::string& text)
        {
            const size_t first = text.find_first_not_of(" \t");
            if (first == std::string::npos)
                return std::string();
            const size_t last = text.find_last_not_of(" \t");
            return text.substr(first, last - first + 1);
        }

        ImU32 ParseHexColor(const std::string& text)
        {
            std::string hex = Trim(text);
            if (!hex.empty() && hex[0] == '#')
                hex = hex.substr(1);
            if (hex.size() != 6)
                return 0;
            char* end = nullptr;
            const unsigned long value = std::strtoul(hex.c_str(), &end, 16);
            if (!end || *end != '\0')
                return 0;
            return IM_COL32((value >> 16) & 0xFF, (value >> 8) & 0xFF, value & 0xFF, 255);
        }

        std::vector<NotchTag> ParseNotchTags(const char* text, int count)
        {
            std::vector<NotchTag> tags;
            if (!text || !*text)
                return tags;
            const std::string all(text);
            size_t start = 0;
            while (start <= all.size() && static_cast<int>(tags.size()) < count)
            {
                size_t comma = all.find(',', start);
                if (comma == std::string::npos)
                    comma = all.size();
                const std::string item = Trim(all.substr(start, comma - start));
                NotchTag tag;
                const size_t eq = item.find('=');
                tag.Text = Trim(eq == std::string::npos ? item : item.substr(0, eq));
                if (eq != std::string::npos)
                    tag.Color = ParseHexColor(item.substr(eq + 1));
                tags.push_back(tag);
                start = comma + 1;
            }
            return tags;
        }

        ImU32 NotchColor(const NotchTag& tag)
        {
            if (tag.Color != 0)
                return tag.Color;
            std::string lower;
            for (char c : tag.Text)
                lower += static_cast<char>(c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
            if (lower == "x" || lower == "r")
                return imui::detail::AxisX;
            if (lower == "y" || lower == "g")
                return imui::detail::AxisY;
            if (lower == "z" || lower == "b")
                return imui::detail::AxisZ;
            if (lower == "w" || lower == "a")
                return imui::detail::AxisW;
            uint32_t hash = 2166136261u;
            for (char c : lower)
                hash = (hash ^ static_cast<uint8_t>(c)) * 16777619u;
            float r = 0.0f, g = 0.0f, b = 0.0f;
            ImGui::ColorConvertHSVtoRGB(static_cast<float>(hash % 360u) / 360.0f, 0.42f, 0.86f, r, g, b);
            return ImGui::GetColorU32(ImVec4(r, g, b, 1.0f));
        }

        // The value as text that fits width: two decimals, then one, then none.
        std::string FormatNumber(float value, bool integer, float width)
        {
            char text[64];
            if (integer)
            {
                std::snprintf(text, sizeof(text), "%d", static_cast<int>(std::lround(value)));
                return text;
            }
            for (int decimals = 2; decimals >= 0; --decimals)
            {
                std::snprintf(text, sizeof(text), "%.*f", decimals, value);
                if (decimals == 0 || ImGui::CalcTextSize(text).x <= width)
                    break;
            }
            return text;
        }

        std::string FormatLimit(float value, bool integer)
        {
            char text[64];
            if (integer)
                std::snprintf(text, sizeof(text), "%d", static_cast<int>(std::lround(value)));
            else
                std::snprintf(text, sizeof(text), "%g", value);
            return text;
        }

        // The shaded notch cell at the left of a field frame (a..b) with its centred tag text.
        void DrawNotchCell(ImDrawList* dl, ImVec2 a, ImVec2 b, float tagWidth, const char* text, ImU32 color, bool hovered, bool active)
        {
            using namespace imui::detail;
            const ImU32 bg = active ? NotchTagActive : (hovered ? NotchTagHover : NotchTagBg);
            dl->AddRectFilled(ImVec2(a.x + 1.0f, a.y + 1.0f), ImVec2(a.x + tagWidth, b.y - 1.0f), bg, 2.0f, ImDrawFlags_RoundCornersLeft);
            dl->AddLine(ImVec2(a.x + tagWidth + 0.5f, a.y + 1.0f), ImVec2(a.x + tagWidth + 0.5f, b.y - 1.0f), NotchDivider, 1.0f);
            ImFont* font = FontSlot(2);
            const float size = font->LegacySize;
            const ImVec2 textSize = font->CalcTextSizeA(size, FLT_MAX, 0.0f, text);
            dl->AddText(font, size, ImVec2(ImFloor(a.x + (tagWidth - textSize.x) * 0.5f + 0.5f), ImFloor(a.y + (b.y - a.y - textSize.y) * 0.5f)), color, text);
        }

        float NotchWidth(const char* text)
        {
            ImFont* font = imui::detail::FontSlot(2);
            return (std::max)(16.0f, ImFloor(font->CalcTextSizeA(font->LegacySize, FLT_MAX, 0.0f, text).x) + 8.0f);
        }

        // The tag, value and range decorations of one number element whose frame is rect.
        void DrawNumberDecor(const ImRect& rect, float value, const imui::NumberFieldStyle& style, const NotchTag* tag, float tagWidth,
            bool hovered, bool active, bool editing)
        {
            using namespace imui::detail;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const bool ranged = HasFlag(style.Flags, imui::NumberFieldFlags::Range) && style.Max > style.Min;
            const bool integer = HasFlag(style.Flags, imui::NumberFieldFlags::Integer);
            const bool hot = hovered || active;
            DrawFieldFrame(rect, hovered, active || editing);
            const ImVec2 a(ImFloor(rect.Min.x), ImFloor(rect.Min.y));
            const ImVec2 b(ImFloor(rect.Max.x), ImFloor(rect.Max.y));
            if (tag && tagWidth > 0.0f)
                DrawNotchCell(dl, a, b, tagWidth, tag->Text.c_str(), NotchColor(*tag), hovered, active || editing);
            if (!editing)
            {
                const std::string text = FormatNumber(value, integer, b.x - 6.0f - (a.x + tagWidth + 4.0f));
                const ImVec2 textSize = ImGui::CalcTextSize(text.c_str());
                const float x = ImFloor(b.x - 6.0f - textSize.x);
                const float y = ImFloor(a.y + (b.y - a.y - textSize.y) * 0.5f) - (ranged && hot ? 1.0f : 0.0f);
                dl->PushClipRect(ImVec2(a.x + tagWidth + 2.0f, a.y), b, true);
                dl->AddText(ImVec2(x, y), active ? IM_COL32_WHITE : TextColor, text.c_str());
                dl->PopClipRect();
            }
            if (!ranged)
                return;
            const float t = ImClamp((value - style.Min) / (style.Max - style.Min), 0.0f, 1.0f);
            const float x0 = a.x + (tagWidth > 0.0f ? tagWidth + 1.0f : 1.0f);
            const float x1 = b.x - 1.0f;
            const float thickness = hot ? 3.0f : 2.0f;
            const float y1 = b.y - 1.0f;
            const float y0 = y1 - thickness;
            const float fx = ImFloor(x0 + (x1 - x0) * t);
            dl->AddRectFilled(ImVec2(x0, y0), ImVec2(x1, y1), hot ? RangeTrackHover : RangeTrack, hot ? 1.5f : 0.0f);
            if (fx > x0)
                dl->AddRectFilled(ImVec2(x0, y0), ImVec2(fx, y1), active ? RangeFillActive : (hovered ? RangeFillHover : RangeFill), hot ? 1.5f : 0.0f);
            if (!hot)
                return;
            const float radius = active ? 4.5f : 4.0f;
            const ImVec2 center(ImClamp(fx, x0 + radius, x1 - radius), (y0 + y1) * 0.5f);
            dl->AddCircleFilled(center, radius, active ? RangeHandleActive : RangeHandleHover, 16);
            dl->AddCircle(center, radius, NotchDivider, 16, 1.0f);
            ImDrawList* fg = ImGui::GetForegroundDrawList(ImGui::GetWindowViewport());
            ImFont* labelFont = FontSlot(1);
            const float size = labelFont->LegacySize;
            const std::string minText = FormatLimit(style.Min, integer);
            const std::string maxText = FormatLimit(style.Max, integer);
            const ImVec2 minSize = labelFont->CalcTextSizeA(size, FLT_MAX, 0.0f, minText.c_str());
            const ImVec2 maxSize = labelFont->CalcTextSizeA(size, FLT_MAX, 0.0f, maxText.c_str());
            const float ty = b.y + 1.0f;
            const ImVec2 minPos(ImFloor(x0), ImFloor(ty));
            const ImVec2 maxPos(ImFloor(x1 - maxSize.x), ImFloor(ty));
            fg->AddRectFilled(ImVec2(minPos.x - 2.0f, minPos.y), ImVec2(minPos.x + minSize.x + 2.0f, minPos.y + minSize.y), PanelBg, 2.0f);
            fg->AddRectFilled(ImVec2(maxPos.x - 2.0f, maxPos.y), ImVec2(maxPos.x + maxSize.x + 2.0f, maxPos.y + maxSize.y), PanelBg, 2.0f);
            fg->AddText(labelFont, size, minPos, TextDim, minText.c_str());
            fg->AddText(labelFont, size, maxPos, TextDim, maxText.c_str());
        }

        bool NumberFields(float* values, int count, float speed, const imui::NumberFieldStyle& style, float width = 0.0f);

        std::vector<ImVec4> _recentColors;
        int _colorPickerTest = -1;
        bool _colorPickerTestDone = false;

        // XG_UI_TEST_COLOR_PICKER: 0 = off, 1 = open the first picker in RGB, 2 = in HSV.
        int ColorPickerTestMode()
        {
            if (_colorPickerTest < 0)
            {
                char value[16] = {};
                _colorPickerTest = xg::GetEnv("XG_UI_TEST_COLOR_PICKER", value, sizeof(value)) ? (std::strcmp(value, "hsv") == 0 ? 2 : 1) : 0;
            }
            return _colorPickerTest;
        }

        bool SameColor(const ImVec4& a, const ImVec4& b)
        {
            return std::fabs(a.x - b.x) < 1e-4f && std::fabs(a.y - b.y) < 1e-4f && std::fabs(a.z - b.z) < 1e-4f && std::fabs(a.w - b.w) < 1e-4f;
        }

        void PushRecentColor(const ImVec4& color)
        {
            _recentColors.erase(std::remove_if(_recentColors.begin(), _recentColors.end(), [&](const ImVec4& c) { return SameColor(c, color); }),
                _recentColors.end());
            _recentColors.insert(_recentColors.begin(), color);
            if (_recentColors.size() > 10)
                _recentColors.resize(10);
        }

        // RRGGBB or RRGGBBAA, with or without a leading '#'.
        bool ParseHexInput(const char* text, ImVec4& color)
        {
            std::string hex = Trim(text ? text : "");
            if (!hex.empty() && hex[0] == '#')
                hex = hex.substr(1);
            if (hex.size() != 6 && hex.size() != 8)
                return false;
            for (char c : hex)
            {
                if (!std::isxdigit(static_cast<unsigned char>(c)))
                    return false;
            }
            const unsigned long value = std::strtoul(hex.c_str(), nullptr, 16);
            const int shift = hex.size() == 8 ? 8 : 0;
            color.x = static_cast<float>((value >> (16 + shift)) & 0xFF) / 255.0f;
            color.y = static_cast<float>((value >> (8 + shift)) & 0xFF) / 255.0f;
            color.z = static_cast<float>((value >> shift) & 0xFF) / 255.0f;
            if (shift)
                color.w = static_cast<float>(value & 0xFF) / 255.0f;
            return true;
        }

        // A 10 px strip in a 14 px hit area across width; while dragged, t follows the mouse (0..1).
        bool PickerStrip(const char* id, float width, float& t, ImRect& strip, bool& active)
        {
            const ImVec2 p = ImGui::GetCursorScreenPos();
            ImGui::InvisibleButton(id, ImVec2(width, 14.0f));
            strip = ImRect(ImVec2(p.x, p.y + 2.0f), ImVec2(p.x + width, p.y + 12.0f));
            active = ImGui::IsItemActive();
            if (!active)
                return false;
            t = ImClamp((ImGui::GetIO().MousePos.x - strip.Min.x) / width, 0.0f, 1.0f);
            return true;
        }

        void DrawStripHandle(ImDrawList* dl, const ImRect& strip, float t, bool active)
        {
            const float radius = 6.0f;
            const ImVec2 center(ImClamp(strip.Min.x + strip.GetWidth() * t, strip.Min.x + 2.0f, strip.Max.x - 2.0f), strip.GetCenter().y);
            dl->AddCircleFilled(center, radius, active ? imui::detail::RangeHandleActive : imui::detail::RangeHandleHover, 20);
            dl->AddCircle(center, radius, imui::detail::Outline, 20, 1.0f);
        }

        // A segment of cells like the Navigator's: SegmentBg backing, the selected cell in Accent.
        bool PickerSegment(const char* const* labels, int count, int& selected, float width)
        {
            using namespace imui::detail;
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 p = ImGui::GetCursorScreenPos();
            const float height = ImGui::GetFrameHeight();
            dl->AddRectFilled(ImVec2(p.x, p.y - 1.0f), ImVec2(p.x + width, p.y + height + 1.0f), SegmentBg, 3.0f);
            const float cell = ImFloor((width - 2.0f - 2.0f * static_cast<float>(count - 1)) / static_cast<float>(count));
            bool changed = false;
            ImGui::SetCursorScreenPos(ImVec2(p.x + 1.0f, p.y));
            for (int i = 0; i < count; ++i)
            {
                if (i > 0)
                    ImGui::SameLine(0.0f, 2.0f);
                ImGui::PushID(i);
                if (ImGui::InvisibleButton("##seg", ImVec2(cell, height)) && selected != i)
                {
                    selected = i;
                    changed = true;
                }
                const ImRect r(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
                const bool on = selected == i;
                if (on || ImGui::IsItemHovered())
                    dl->AddRectFilled(r.Min, r.Max, on ? Accent : DropdownHover, 2.0f);
                const ImVec2 textSize = ImGui::CalcTextSize(labels[i]);
                dl->AddText(ImVec2(ImFloor(r.GetCenter().x - textSize.x * 0.5f), ImFloor(r.GetCenter().y - textSize.y * 0.5f)),
                    on ? TextOnAccent : TextColor, labels[i]);
                ImGui::PopID();
            }
            return changed;
        }

        // The colour picker popup of a swatch at anchor: SV square, hue and alpha strips, old | new boxes, hex field,
        // RGB/HSV switch, number row, recent colours and Revert. State (old colour, HSV, mode) lives in the window storage.
        bool ColorPickerPopup(float* values, int count, const ImRect& anchor)
        {
            using namespace imui::detail;
            const ImGuiID key = ImGui::GetID("##picker");
            ImGuiStorage* storage = ImGui::GetStateStorage();
            const bool alpha = count > 3;
            const bool wasOpen = storage->GetBool(key + 1, false);
            ImVec4 color(values[0], values[1], values[2], alpha ? values[3] : 1.0f);
            const float width = 290.0f;
            const ImGuiViewport* viewport = ImGui::GetWindowViewport();
            const float popupWidth = width + 20.0f;
            const float expectedHeight = alpha ? 330.0f : 306.0f;
            ImVec2 position(anchor.Min.x, anchor.Max.y + 2.0f);
            position.x = ImMax(viewport->WorkPos.x + 4.0f, ImMin(position.x, viewport->WorkPos.x + viewport->WorkSize.x - popupWidth - 4.0f));
            if (position.y + expectedHeight > viewport->WorkPos.y + viewport->WorkSize.y && anchor.Min.y - expectedHeight - 2.0f >= viewport->WorkPos.y)
                position.y = anchor.Min.y - expectedHeight - 2.0f;
            ImGui::SetNextWindowPos(position, ImGuiCond_Appearing);
            ImGui::SetNextWindowSizeConstraints(ImVec2(width + 20.0f, 0.0f), ImVec2(width + 20.0f, FLT_MAX));
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(10.0f, 10.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6.0f, 8.0f));
            ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 2.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_PopupBorderSize, 1.0f);
            ImGui::PushStyleColor(ImGuiCol_PopupBg, HeaderBg);
            ImGui::PushStyleColor(ImGuiCol_Border, DropdownBorder);
            const bool open = ImGui::BeginPopup("##picker", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);
            ImGui::PopStyleColor(2);
            ImGui::PopStyleVar(2);
            if (!open)
            {
                ImGui::PopStyleVar(2);
                if (wasOpen)
                {
                    storage->SetBool(key + 1, false);
                    const ImVec4 old(storage->GetFloat(key + 2), storage->GetFloat(key + 3), storage->GetFloat(key + 4), storage->GetFloat(key + 5));
                    if (!SameColor(old, color))
                        PushRecentColor(color);
                }
                return false;
            }
            float h = 0.0f, s = 0.0f, v = 0.0f;
            if (!wasOpen)
            {
                storage->SetBool(key + 1, true);
                storage->SetFloat(key + 2, color.x);
                storage->SetFloat(key + 3, color.y);
                storage->SetFloat(key + 4, color.z);
                storage->SetFloat(key + 5, color.w);
                ImGui::ColorConvertRGBtoHSV(color.x, color.y, color.z, h, s, v);
            }
            else
            {
                h = storage->GetFloat(key + 6);
                s = storage->GetFloat(key + 7);
                v = storage->GetFloat(key + 8);
            }
            const ImVec4 old(storage->GetFloat(key + 2), storage->GetFloat(key + 3), storage->GetFloat(key + 4), storage->GetFloat(key + 5));
            auto syncHSV = [&]()
            {
                float r = 0.0f, g = 0.0f, b = 0.0f;
                ImGui::ColorConvertHSVtoRGB(h, s, v, r, g, b);
                if (std::fabs(r - color.x) < 1e-4f && std::fabs(g - color.y) < 1e-4f && std::fabs(b - color.z) < 1e-4f)
                    return;
                float nh = 0.0f, ns = 0.0f, nv = 0.0f;
                ImGui::ColorConvertRGBtoHSV(color.x, color.y, color.z, nh, ns, nv);
                if (nv > 0.0f && ns > 0.0f)
                    h = nh;
                if (nv > 0.0f)
                    s = ns;
                v = nv;
            };
            auto applyHSV = [&]() { ImGui::ColorConvertHSVtoRGB(h, s, v, color.x, color.y, color.z); };
            syncHSV();
            int mode = storage->GetInt(key + 9, 0);
            bool changed = false;
            ImDrawList* dl = ImGui::GetWindowDrawList();

            const ImVec2 sv = ImGui::GetCursorScreenPos();
            const ImVec2 svSize(width, 130.0f);
            ImGui::InvisibleButton("##sv", svSize);
            if (ImGui::IsItemActive())
            {
                const ImVec2 mouse = ImGui::GetIO().MousePos;
                s = ImClamp((mouse.x - sv.x) / svSize.x, 0.0f, 1.0f);
                v = 1.0f - ImClamp((mouse.y - sv.y) / svSize.y, 0.0f, 1.0f);
                applyHSV();
                changed = true;
            }
            float hr = 0.0f, hg = 0.0f, hb = 0.0f;
            ImGui::ColorConvertHSVtoRGB(h, 1.0f, 1.0f, hr, hg, hb);
            const ImU32 hue = ImGui::ColorConvertFloat4ToU32(ImVec4(hr, hg, hb, 1.0f));
            const ImVec2 svMax(sv.x + svSize.x, sv.y + svSize.y);
            dl->AddRectFilledMultiColor(sv, svMax, IM_COL32_WHITE, hue, hue, IM_COL32_WHITE);
            dl->AddRectFilledMultiColor(sv, svMax, IM_COL32(0, 0, 0, 0), IM_COL32(0, 0, 0, 0), IM_COL32_BLACK, IM_COL32_BLACK);
            dl->AddRect(ImVec2(sv.x + 0.5f, sv.y + 0.5f), ImVec2(svMax.x - 0.5f, svMax.y - 0.5f), FieldEdgeSide, 0.0f, 0, 1.0f);
            const ImVec2 handle(ImFloor(sv.x + s * svSize.x) + 0.5f, ImFloor(sv.y + (1.0f - v) * svSize.y) + 0.5f);
            dl->PushClipRect(sv, svMax, true);
            dl->AddCircle(handle, 7.5f, IM_COL32(0, 0, 0, 128), 24, 1.0f);
            dl->AddCircle(handle, 6.0f, IM_COL32_WHITE, 24, 2.0f);
            dl->PopClipRect();

            ImRect strip;
            bool stripActive = false;
            float t = h;
            if (PickerStrip("##hue", width, t, strip, stripActive))
            {
                h = (std::min)(t, 0.9999f);
                applyHSV();
                changed = true;
            }
            for (int i = 0; i < 6; ++i)
            {
                float r0 = 0.0f, g0 = 0.0f, b0 = 0.0f, r1 = 0.0f, g1 = 0.0f, b1 = 0.0f;
                ImGui::ColorConvertHSVtoRGB(static_cast<float>(i) / 6.0f, 1.0f, 1.0f, r0, g0, b0);
                ImGui::ColorConvertHSVtoRGB(static_cast<float>(i + 1) / 6.0f, 1.0f, 1.0f, r1, g1, b1);
                const ImU32 c0 = ImGui::ColorConvertFloat4ToU32(ImVec4(r0, g0, b0, 1.0f));
                const ImU32 c1 = ImGui::ColorConvertFloat4ToU32(ImVec4(r1, g1, b1, 1.0f));
                const float x0 = ImFloor(strip.Min.x + strip.GetWidth() * static_cast<float>(i) / 6.0f);
                const float x1 = i == 5 ? strip.Max.x : ImFloor(strip.Min.x + strip.GetWidth() * static_cast<float>(i + 1) / 6.0f);
                dl->AddRectFilledMultiColor(ImVec2(x0, strip.Min.y), ImVec2(x1, strip.Max.y), c0, c1, c1, c0);
            }
            dl->AddRect(strip.Min, strip.Max, FieldEdgeSide, 0.0f, 0, 1.0f);
            DrawStripHandle(dl, strip, h, stripActive);

            if (alpha)
            {
                t = color.w;
                if (PickerStrip("##alpha", width, t, strip, stripActive))
                {
                    color.w = t;
                    changed = true;
                }
                DrawChecker(dl, strip, 5.0f);
                dl->AddRectFilledMultiColor(strip.Min, strip.Max,
                    ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, 0.0f)), ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, 1.0f)),
                    ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, 1.0f)), ImGui::ColorConvertFloat4ToU32(ImVec4(color.x, color.y, color.z, 0.0f)));
                dl->AddRect(strip.Min, strip.Max, FieldEdgeSide, 0.0f, 0, 1.0f);
                DrawStripHandle(dl, strip, color.w, stripActive);
            }

            const float frame = ImGui::GetFrameHeight();
            const ImVec2 boxes = ImGui::GetCursorScreenPos();
            if (ImGui::InvisibleButton("##old", ImVec2(frame, frame)))
            {
                color = old;
                syncHSV();
                changed = true;
            }
            if (ImGui::IsItemHovered())
                ImGui::SetTooltip("Original colour (click to revert)");
            ImGui::SameLine(0.0f, 0.0f);
            ImGui::InvisibleButton("##new", ImVec2(frame, frame));
            DrawColorBox(dl, ImRect(boxes, ImVec2(boxes.x + frame, boxes.y + frame)), old, 2.0f);
            DrawColorBox(dl, ImRect(ImVec2(boxes.x + frame, boxes.y), ImVec2(boxes.x + frame * 2.0f, boxes.y + frame)), color, 2.0f);

            const float segmentWidth = 104.0f;
            const float hexWidth = width - frame * 2.0f - segmentWidth - 16.0f;
            ImGui::SameLine(0.0f, 8.0f);
            char hex[16];
            const auto byte = [](float c) { return static_cast<int>(std::lround(ImClamp(c, 0.0f, 1.0f) * 255.0f)); };
            if (alpha)
                std::snprintf(hex, sizeof(hex), "%02X%02X%02X%02X", byte(color.x), byte(color.y), byte(color.z), byte(color.w));
            else
                std::snprintf(hex, sizeof(hex), "%02X%02X%02X", byte(color.x), byte(color.y), byte(color.z));
            const float tagWidth = NotchWidth("hex");
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(tagWidth + 6.0f, ImGui::GetStyle().FramePadding.y));
            ImGui::PushFont(FontSlot(3), FontSlot(3)->LegacySize);
            ImGui::SetNextItemWidth(hexWidth);
            if (ImGui::InputText("##hex", hex, sizeof(hex), ImGuiInputTextFlags_CharsUppercase | ImGuiInputTextFlags_AutoSelectAll))
            {
                ImVec4 parsed = color;
                if (ParseHexInput(hex, parsed))
                {
                    if (!alpha)
                        parsed.w = 1.0f;
                    color = parsed;
                    syncHSV();
                    changed = true;
                }
            }
            ImGui::PopFont();
            ImGui::PopStyleVar();
            {
                const ImRect r(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
                const bool hovered = ImGui::IsItemHovered();
                const bool active = ImGui::IsItemActive();
                DrawFieldFrame(r, hovered, active);
                DrawNotchCell(dl, ImVec2(ImFloor(r.Min.x), ImFloor(r.Min.y)), ImVec2(ImFloor(r.Max.x), ImFloor(r.Max.y)), tagWidth, "hex", TextDim, hovered, active);
            }
            ImGui::SameLine(0.0f, 8.0f);
            static const char* const modes[] = { "RGB", "HSV" };
            PickerSegment(modes, 2, mode, segmentWidth);

            imui::NumberFieldStyle unit{};
            unit.Size = sizeof(unit);
            unit.Flags = imui::NumberFieldFlags::Range;
            unit.Min = 0.0f;
            unit.Max = 1.0f;
            if (mode == 0)
            {
                float rgba[4] = { color.x, color.y, color.z, color.w };
                unit.NotchTags = alpha ? "r,g,b,a" : "r,g,b";
                ImGui::PushID("rgb");
                if (NumberFields(rgba, count, 0.0f, unit, width))
                {
                    color = ImVec4(rgba[0], rgba[1], rgba[2], alpha ? rgba[3] : 1.0f);
                    syncHSV();
                    changed = true;
                }
                ImGui::PopID();
            }
            else
            {
                const float element = ImFloor((width - 4.0f * static_cast<float>(count - 1)) / static_cast<float>(count));
                imui::NumberFieldStyle degrees = unit;
                degrees.Max = 360.0f;
                degrees.NotchTags = "h";
                float hue360 = h * 360.0f;
                ImGui::PushID("h");
                if (NumberFields(&hue360, 1, 0.0f, degrees, element))
                {
                    h = ImClamp(hue360 / 360.0f, 0.0f, 0.9999f);
                    applyHSV();
                    changed = true;
                }
                ImGui::PopID();
                ImGui::SameLine(0.0f, 4.0f);
                float rest[3] = { s, v, color.w };
                unit.NotchTags = alpha ? "s,v,a" : "s,v";
                ImGui::PushID("sva");
                if (NumberFields(rest, count - 1, 0.0f, unit, width - element - 4.0f))
                {
                    s = rest[0];
                    v = rest[1];
                    if (alpha)
                        color.w = rest[2];
                    applyHSV();
                    changed = true;
                }
                ImGui::PopID();
            }

            const ImVec2 line = ImGui::GetCursorScreenPos();
            dl->AddLine(ImVec2(line.x - 4.0f, line.y - 4.5f), ImVec2(line.x + width + 4.0f, line.y - 4.5f), DropdownBorder, 1.0f);
            for (size_t i = 0; i < _recentColors.size(); ++i)
            {
                ImGui::SetCursorScreenPos(ImVec2(line.x + static_cast<float>(i) * 22.0f, line.y + 2.0f));
                ImGui::PushID(static_cast<int>(i));
                if (ImGui::InvisibleButton("##recent", ImVec2(18.0f, 18.0f)))
                {
                    color = ImVec4(_recentColors[i].x, _recentColors[i].y, _recentColors[i].z, alpha ? _recentColors[i].w : 1.0f);
                    syncHSV();
                    changed = true;
                }
                const ImRect r(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
                DrawColorBox(dl, r, _recentColors[i], 2.0f);
                if (ImGui::IsItemHovered())
                    dl->AddRect(r.Min, r.Max, IM_COL32(255, 255, 255, 140), 2.0f, 0, 1.0f);
                ImGui::PopID();
            }
            if (_recentColors.empty())
            {
                ImGui::SetCursorScreenPos(ImVec2(line.x, line.y + (frame - ImGui::GetTextLineHeight()) * 0.5f));
                ImGui::TextColored(ToVec4(TextDim), "No recent colours");
            }
            ImGui::SetCursorScreenPos(ImVec2(line.x + width - segmentWidth, line.y));
            if (ImGui::Button("Revert", ImVec2(segmentWidth, frame)))
            {
                color = old;
                syncHSV();
                changed = true;
            }

            if (ImGui::IsKeyPressed(ImGuiKey_Escape))
                ImGui::CloseCurrentPopup();
            storage->SetFloat(key + 6, h);
            storage->SetFloat(key + 7, s);
            storage->SetFloat(key + 8, v);
            storage->SetInt(key + 9, mode);
            ImGui::EndPopup();
            ImGui::PopStyleVar(2);
            if (changed)
            {
                values[0] = color.x;
                values[1] = color.y;
                values[2] = color.z;
                if (alpha)
                    values[3] = color.w;
            }
            return changed;
        }

        // A colour swatch the size of a frame (the standard diagonal colour box); a click opens the colour picker.
        bool ColorSwatch(float* values, int count)
        {
            const float height = ImGui::GetFrameHeight();
            const ImVec4 color(values[0], values[1], values[2], count > 3 ? values[3] : 1.0f);
            const bool pressed = ImGui::InvisibleButton("##swatch", ImVec2(height, height));
            const ImRect rect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
            const bool hovered = ImGui::IsItemHovered();
            const bool open = ImGui::IsPopupOpen("##picker");
            imui::detail::DrawColorBox(ImGui::GetWindowDrawList(), rect, color, 2.0f);
            imui::detail::DrawFieldFrame(rect, hovered, open);
            const int test = ColorPickerTestMode();
            if (pressed || (test != 0 && !_colorPickerTestDone && ImGui::GetFrameCount() > 90))
            {
                if (!pressed)
                {
                    _colorPickerTestDone = true;
                    ImGui::GetStateStorage()->SetInt(ImGui::GetID("##picker") + 9, test == 2 ? 1 : 0);
                }
                ImGui::OpenPopup("##picker");
            }
            return ColorPickerPopup(values, count, rect);
        }

        // count number elements side by side filling the row: each a drag field (scrub, double-click or Ctrl+click to
        // type) whose value, tag and range the kit draws itself (right-aligned value, never recentred while dragging).
        bool NumberFields(float* values, int count, float speed, const imui::NumberFieldStyle& style, float width)
        {
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            const bool ranged = HasFlag(style.Flags, imui::NumberFieldFlags::Range) && style.Max > style.Min;
            const bool integer = HasFlag(style.Flags, imui::NumberFieldFlags::Integer);
            const bool color = HasFlag(style.Flags, imui::NumberFieldFlags::Color) && count >= 3;
            const std::vector<NotchTag> tags = ParseNotchTags(style.NotchTags, count);
            const float spacing = 4.0f;
            float avail = width > 0.0f ? width : ImGui::GetContentRegionAvail().x;
            bool changed = false;
            if (color)
            {
                changed |= ColorSwatch(values, count);
                ImGui::SameLine(0.0f, spacing);
                avail -= ImGui::GetFrameHeight() + spacing;
            }
            const float element = (std::max)(1.0f, ImFloor((avail - spacing * static_cast<float>(count - 1)) / static_cast<float>(count)));
            const float dragSpeed = speed > 0.0f ? speed : (ranged ? (style.Max - style.Min) / 200.0f : (integer ? 0.2f : 0.1f));
            ImFont* tagFont = imui::detail::FontSlot(2);
            const ImVec2 padding = ImGui::GetStyle().FramePadding;
            for (int i = 0; i < count; ++i)
            {
                ImGui::PushID(i);
                if (i > 0)
                    ImGui::SameLine(0.0f, spacing);
                const float w = i == count - 1 ? (std::max)(1.0f, avail - (element + spacing) * static_cast<float>(count - 1)) : element;
                const NotchTag* tag = i < static_cast<int>(tags.size()) && !tags[i].Text.empty() ? &tags[i] : nullptr;
                float tagWidth = tag ? (std::max)(16.0f, ImFloor(tagFont->CalcTextSizeA(tagFont->LegacySize, FLT_MAX, 0.0f, tag->Text.c_str()).x) + 8.0f) : 0.0f;
                if (tag && element < tagWidth + ImGui::CalcTextSize("0.0").x + 12.0f)
                {
                    tag = nullptr;
                    tagWidth = 0.0f;
                }
                const ImGuiID id = window->GetID("##n");
                const bool editing = ImGui::TempInputIsActive(id);
                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(tagWidth + 5.0f, padding.y));
                if (!editing)
                    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(0, 0, 0, 0));
                const ImGuiSliderFlags flags = ranged ? ImGuiSliderFlags_AlwaysClamp : ImGuiSliderFlags_None;
                float value = values[i];
                bool edited = false;
                ImGui::SetNextItemWidth(w);
                if (integer)
                {
                    int whole = static_cast<int>(std::lround(value));
                    edited = ImGui::DragInt("##n", &whole, dragSpeed, ranged ? static_cast<int>(style.Min) : 0, ranged ? static_cast<int>(style.Max) : 0, "%d", flags);
                    if (edited)
                        value = static_cast<float>(whole);
                }
                else
                {
                    edited = ImGui::DragFloat("##n", &value, dragSpeed, ranged ? style.Min : 0.0f, ranged ? style.Max : 0.0f, "%.3f", flags);
                }
                if (!editing)
                    ImGui::PopStyleColor();
                ImGui::PopStyleVar();
                if (edited)
                {
                    values[i] = value;
                    changed = true;
                }
                const bool hovered = ImGui::IsItemHovered();
                const bool active = ImGui::IsItemActive();
                if (hovered && !editing)
                    ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
                DrawNumberDecor(ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()), values[i], style, tag, tagWidth, hovered, active && !editing, editing);
                ImGui::PopID();
            }
            return changed;
        }

        // A label column plus the body's field(s), over a subtle row bar: white on hover, accent with a left edge while
        // one of the row's fields is focused. The bar is drawn on a channel behind the row's items.
        template <typename Body>
        imui::Result PropertyRow(imui::Context* ctx, const char* label, Body body)
        {
            const char* text = label ? label : "";
            LayoutItem(ctx);
            ImGui::PushID(text);
            ImGuiWindow* window = ImGui::GetCurrentWindow();
            ImDrawList* dl = window->DrawList;
            const ImVec2 start = ImGui::GetCursorScreenPos();
            const bool split = dl->_Splitter._Count <= 1;
            if (split)
            {
                dl->ChannelsSplit(2);
                dl->ChannelsSetCurrent(1);
            }
            const bool wasActive = imui::detail::_fieldActive;
            imui::detail::_fieldActive = false;
            InspectorLabel(text);
            const bool changed = body();
            const imui::Result result = ItemResult(changed);
            const bool focused = imui::detail::_fieldActive;
            imui::detail::_fieldActive = wasActive;
            if (split)
            {
                const ImRect row(ImVec2(ImFloor(start.x) - 4.0f, ImFloor(start.y) - 2.0f),
                    ImVec2(ImFloor(window->WorkRect.Max.x) + 4.0f, ImFloor(start.y + ImGui::GetFrameHeight()) + 2.0f));
                const bool hovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) && ImGui::IsMouseHoveringRect(row.Min, row.Max, false);
                dl->ChannelsSetCurrent(0);
                if (focused)
                {
                    dl->AddRectFilled(row.Min, row.Max, imui::detail::RowFocus, 2.0f);
                    dl->AddRectFilled(row.Min, ImVec2(row.Min.x + 2.0f, row.Max.y), imui::detail::Accent);
                }
                else if (hovered)
                {
                    dl->AddRectFilled(row.Min, row.Max, imui::detail::RowHover, 2.0f);
                }
                dl->ChannelsMerge();
            }
            ImGui::PopID();
            return result;
        }
    }

    XG_MODULE_EXPORT imui::Result XG_ImUIFieldFloat(imui::Context* ctx, const char* label, float* value, float speed)
    {
        if (!ctx || !value)
            return imui::Result::None;
        return PropertyRow(ctx, label, [&] { return NumberFields(value, 1, speed > 0.0f ? speed : 0.1f, imui::NumberFieldStyle{}); });
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
        return PropertyRow(ctx, label, [&]
        {
            imui::NumberFieldStyle style;
            style.Flags = imui::NumberFieldFlags::Integer;
            float number = static_cast<float>(*value);
            const bool changed = NumberFields(&number, 1, speed > 0.0f ? speed : 0.2f, style);
            if (changed)
                *value = static_cast<int>(std::lround(number));
            return changed;
        });
    }

    XG_MODULE_EXPORT imui::Result XG_ImUIFieldVector(imui::Context* ctx, const char* label, float* values, int count, float speed)
    {
        if (!ctx || !values || count < 1 || count > 4)
            return imui::Result::None;
        return PropertyRow(ctx, label, [&] { return NumberFields(values, count, speed > 0.0f ? speed : 0.1f, imui::NumberFieldStyle{}); });
    }

    XG_MODULE_EXPORT imui::Result XG_ImUIFieldNumbers(imui::Context* ctx, const char* label, float* values, int count, float speed,
        const imui::NumberFieldStyle* style)
    {
        if (!ctx || !values || count < 1 || count > 4)
            return imui::Result::None;
        const imui::NumberFieldStyle loaded = style ? interop::Load(style) : imui::NumberFieldStyle{};
        return PropertyRow(ctx, label, [&] { return NumberFields(values, count, speed, loaded); });
    }

    XG_MODULE_EXPORT imui::Result XG_ImUIFieldText(imui::Context* ctx, const char* label, char* buffer, int capacity)
    {
        if (!ctx || !buffer || capacity <= 0)
            return imui::Result::None;
        return PropertyRow(ctx, label, [&]
        {
            const bool changed = ImGui::InputText("##value", buffer, static_cast<size_t>(capacity));
            imui::detail::DrawFieldFrame(ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()), ImGui::IsItemHovered(), ImGui::IsItemActive());
            return changed;
        });
    }

    XG_MODULE_EXPORT imui::Result XG_ImUISliderFloat(imui::Context* ctx, const char* label, float* value, float min, float max)
    {
        if (!ctx || !value)
            return imui::Result::None;
        return PropertyRow(ctx, label, [&]
        {
            imui::NumberFieldStyle style;
            style.Flags = imui::NumberFieldFlags::Range;
            style.Min = min;
            style.Max = max;
            return NumberFields(value, 1, 0.0f, style);
        });
    }

    XG_MODULE_EXPORT imui::Result XG_ImUISliderInt(imui::Context* ctx, const char* label, int* value, int min, int max)
    {
        if (!ctx || !value)
            return imui::Result::None;
        return PropertyRow(ctx, label, [&]
        {
            imui::NumberFieldStyle style;
            style.Flags = imui::NumberFieldFlags::Range | imui::NumberFieldFlags::Integer;
            style.Min = static_cast<float>(min);
            style.Max = static_cast<float>(max);
            float number = static_cast<float>(*value);
            const bool changed = NumberFields(&number, 1, 0.0f, style);
            if (changed)
                *value = static_cast<int>(std::lround(number));
            return changed;
        });
    }

    XG_MODULE_EXPORT imui::Result XG_ImUICheckbox(imui::Context* ctx, const char* label, bool* value)
    {
        if (!ctx || !value)
            return imui::Result::None;
        return PropertyRow(ctx, label, [&]
        {
            const ImVec2 padding = ImGui::GetStyle().FramePadding;
            ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 2.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(padding.x, padding.y - 2.0f));
            const bool changed = ImGui::Checkbox("##value", value);
            ImGui::PopStyleVar();
            imui::detail::DrawFieldFrame(ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()), ImGui::IsItemHovered(), false);
            return changed;
        });
    }

    XG_MODULE_EXPORT imui::Result XG_ImUICombo(imui::Context* ctx, const char* label, int* index, const char* const* items, int count)
    {
        if (!ctx || !index || (!items && count > 0))
            return imui::Result::None;
        return PropertyRow(ctx, label, [&]
        {
            const char* preview = *index >= 0 && *index < count && items[*index] ? items[*index] : "";
            bool changed = false;
            ImGui::PushStyleColor(ImGuiCol_Button, imui::detail::NotchTagBg);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, imui::detail::NotchTagHover);
            const bool open = ImGui::BeginCombo("##value", preview);
            ImGui::PopStyleColor(2);
            imui::detail::DrawFieldFrame(ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()), ImGui::IsItemHovered(), open);
            if (open)
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

    XG_MODULE_EXPORT imui::Result XG_ImUIFieldPicker(imui::Context* ctx, const char* label, const char* text, imui::Icon icon, Color32 iconColor,
        imui::TextureHandle thumbnail, const char* chip, bool open)
    {
        if (!ctx)
            return imui::Result::None;
        return PropertyRow(ctx, label, [&]
        {
            using namespace imui::detail;
            const float height = ImGui::GetFrameHeight();
            const float width = ImMax(ImGui::GetContentRegionAvail().x, height * 3.0f);
            ImGui::InvisibleButton("##picker_field", ImVec2(width, height));
            const ImRect r(ImFloor(ImGui::GetItemRectMin()), ImFloor(ImGui::GetItemRectMax()));
            const bool hovered = ImGui::IsItemHovered();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(r.Min, r.Max, open ? FieldBgActive : (hovered ? FieldBgHover : FieldBg), 2.0f);
            const float cell = height - 2.0f;
            const ImVec2 arrowMin(r.Max.x - cell - 1.0f, r.Min.y + 1.0f);
            dl->AddRectFilled(arrowMin, ImVec2(r.Max.x - 1.0f, r.Max.y - 1.0f), (hovered || open) ? NotchTagHover : NotchTagBg, 2.0f, ImDrawFlags_RoundCornersRight);
            dl->AddLine(ImVec2(arrowMin.x - 0.5f, r.Min.y + 1.0f), ImVec2(arrowMin.x - 0.5f, r.Max.y - 1.0f), NotchDivider, 1.0f);
            const float arrowScale = 0.6f;
            const float arrowSize = ImGui::GetFontSize() * arrowScale;
            ImGui::RenderArrow(dl, ImVec2(ImFloor(arrowMin.x + (cell - arrowSize) * 0.5f), ImFloor(arrowMin.y + (cell - arrowSize) * 0.5f + 1.0f)),
                hovered || open ? TextColor : TextDim, ImGuiDir_Down, arrowScale);
            const float cy = (r.Min.y + r.Max.y) * 0.5f;
            float x = r.Min.x + 5.0f;
            if (thumbnail && thumbnail->Data)
            {
                dl->AddImageRounded(thumbnail->Data->GetTexRef(), ImVec2(x, cy - 7.0f), ImVec2(x + 14.0f, cy + 7.0f), ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f),
                    IM_COL32_WHITE, 2.0f);
                x += 19.0f;
            }
            else if (icon > imui::Icon::None && icon < imui::Icon::Count)
            {
                DrawIcon(dl, static_cast<Icon>(icon), ImVec2(x + 7.0f, cy), 12.0f, ToImU32(iconColor));
                x += 19.0f;
            }
            float textRight = arrowMin.x - 6.0f;
            if (chip && *chip)
            {
                ImFont* font = FontSlot(1);
                const float size = font->LegacySize;
                const ImVec2 chipText = font->CalcTextSizeA(size, FLT_MAX, 0.0f, chip);
                const ImVec2 chipMax(ImFloor(arrowMin.x - 5.0f), ImFloor(cy + 7.0f));
                const ImVec2 chipMin(ImFloor(chipMax.x - chipText.x - 12.0f), ImFloor(cy - 7.0f));
                if (chipMin.x > x + 40.0f)
                {
                    dl->AddRectFilled(chipMin, chipMax, NotchTagHover, 7.0f);
                    dl->AddText(font, size, ImVec2(chipMin.x + 6.0f, ImFloor(cy - chipText.y * 0.5f)), TextDim, chip);
                    textRight = chipMin.x - 6.0f;
                }
            }
            const char* shown = text ? text : "";
            const ImVec2 textSize = ImGui::CalcTextSize(shown);
            dl->PushClipRect(ImVec2(x, r.Min.y), ImVec2(ImMax(x, textRight), r.Max.y), true);
            dl->AddText(ImVec2(x, ImFloor(cy - textSize.y * 0.5f)), TextColor, shown);
            dl->PopClipRect();
            DrawFieldFrame(r, hovered, open);
            if (hovered)
                ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
            return false;
        });
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
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
        ImGui::SetNextItemWidth(fieldWidth);
        const bool changed = ImGui::InputTextWithHint(id ? id : "##textfield", hint ? hint : "", buffer, static_cast<size_t>(capacity));
        const imui::Result result = ItemResult(changed);
        ImGui::PopStyleVar();
        imui::detail::DrawFieldFrame(ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()), ImGui::IsItemHovered(), ImGui::IsItemActive());
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
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6.0f, 4.0f));
        ImGui::SetNextItemWidth(fieldWidth);
        const bool changed = ImGui::InputTextWithHint(id ? id : "##textfield", hint ? hint : "", buffer, static_cast<size_t>(capacity),
            inputFlags, TextFieldCallback, &user);
        const imui::Result result = ItemResult(changed || (HasFlag(flags, imui::TextFieldFlags::SetText) && source != buffer));
        ImGui::PopStyleVar();
        imui::detail::DrawFieldFrame(ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()), ImGui::IsItemHovered(), ImGui::IsItemActive());
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
        imui::detail::DrawFieldFrame(ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax()), false, false);
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

    XG_MODULE_EXPORT void XG_ImUIDrawColorBox(imui::Context* ctx, const Rect* rect, Color32 color)
    {
        if (!ctx)
            return;
        const Rect r = LoadRect(rect);
        imui::detail::DrawColorBox(ctx->DrawList(), ImRect(ctx->ToScreen(r.min), ctx->ToScreen(r.max)), ImGui::ColorConvertU32ToFloat4(ToImU32(color)), 2.0f);
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
