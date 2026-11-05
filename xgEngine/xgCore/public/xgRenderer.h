// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <cstdint>
#include "xgHandles.h"
#include "xgModules.h"

namespace xg {

    class Window;

    struct UIVertex
    {
        float X, Y;
        float U, V;
        uint32_t Color;
    };

    struct UIDrawCommand
    {
        float ClipMinX, ClipMinY, ClipMaxX, ClipMaxY;
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
        float DisplayX, DisplayY, DisplayWidth, DisplayHeight;
        float ScaleX, ScaleY;
    };

    class Renderer {
    public:
        virtual ~Renderer() = default;
        virtual void Render() = 0;
        virtual void* GetNativeHandle() const = 0;

        virtual uint64_t CreateUITexture(int32_t width, int32_t height, const void* rgba) = 0;
        virtual void UpdateUITexture(uint64_t id, int32_t x, int32_t y, int32_t width, int32_t height, const void* rgba, int32_t pitch) = 0;
        virtual void DestroyUITexture(uint64_t id) = 0;
        virtual void SubmitUI(const UIDrawList* list) = 0;
        virtual void SetClearColor(float r, float g, float b, float a) = 0;
    };

    XG_DECLARE_MODULE_FUNCTION(CreateRenderer, Renderer*, Window&);
    XG_DECLARE_MODULE_FUNCTION(DestroyRenderer, void, Renderer*);

	XG_DECLARE_UNIQUE(Renderer)

    XG_API RendererUnique CreateRenderer(Window& window);
    XG_API void      DestroyRenderer(Renderer* renderer);

}

XG_DECLARE_MODULE(xg, Renderer)
