// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <cstdint>
#include "xgHandles.h"
#include "xgColor.h"
#include "xgRect.h"
#include "xgVec2.h"

namespace xg {

    class Window;

    // UI geometry shared by every UI layer (immediate or retained) and every render target.
    struct UIVertex
    {
        Vec2 Position;
        Vec2 UV;
        Color32 Color;
    };
    static_assert(sizeof(UIVertex) == 20, "UIVertex must stay 20 bytes (position, uv, packed color)");

    struct UIDrawCommand
    {
        Rect Clip;
        uint64_t TextureId;
        uint32_t IndexOffset;
        uint32_t VertexOffset;
        uint32_t IndexCount;
    };

    struct UIDrawList
    {
        const UIVertex* Vertices;
        uint32_t VertexCount;
        const uint16_t* Indices;
        uint32_t IndexCount;
        const UIDrawCommand* Commands;
        uint32_t CommandCount;
        Vec2 DisplayPos;
        Vec2 DisplaySize;
        Vec2 FramebufferScale;
    };

    // Anything that can draw UI geometry: a full renderer, or a UI-only presenter.
    class UIRenderTarget {
    public:
        virtual ~UIRenderTarget() = default;

        virtual uint64_t CreateUITexture(int32_t width, int32_t height, const void* rgba) = 0;
        virtual void UpdateUITexture(uint64_t id, int32_t x, int32_t y, int32_t width, int32_t height, const void* rgba, int32_t pitch) = 0;
        virtual void DestroyUITexture(uint64_t id) = 0;
        virtual void SubmitUI(const UIDrawList* list) = 0;
        virtual void SetClearColor(const Color& color) = 0;

        // Extra presentation surfaces for secondary windows (UI multi-viewport). Surface 0 is the main window.
        virtual bool SupportsSurfaces() const = 0;
        virtual uint64_t CreateUISurface(Window& window) = 0;
        virtual void DestroyUISurface(uint64_t surface) = 0;
        virtual void SubmitUISurface(uint64_t surface, const UIDrawList* list) = 0;
    };

    // Factory implemented in xgPlatform: a UI-only presenter on the window, used when no renderer module is loaded.
    XG_DECLARE_UNIQUE(UIRenderTarget)
    XG_API UIRenderTargetUnique NewUIRenderTarget(Window& window);

} // namespace xg
