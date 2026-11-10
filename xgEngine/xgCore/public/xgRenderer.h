// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <cstdint>
#include "xgHandles.h"
#include "xgModules.h"
#include "xgUIDraw.h"
#include "xgVec3.h"

namespace xg {

    class Window;

    // Camera for an offscreen render view. Y is up; FovY is in radians.
    // Crosses into managed code (xgEditorKernel scene view exports): 80 bytes, Vec3 and Color are 16-byte aligned.
    struct RenderViewCamera
    {
        Vec3 Eye;
        Vec3 Target;
        Vec3 Up;
        float FovY;
        float NearZ;
        float FarZ;
        Color ClearColor;
    };
    static_assert(sizeof(RenderViewCamera) == 80, "RenderViewCamera layout is mirrored in managed code");

    class Renderer : public UIRenderTarget {
    public:
        virtual void Render() = 0;
        virtual void* GetNativeHandle() const = 0;

        // Offscreen 3D views. The returned id is also a valid UI texture id (draw it in a panel).
        virtual uint64_t CreateRenderView(int32_t width, int32_t height) = 0;
        virtual bool ResizeRenderView(uint64_t id, int32_t width, int32_t height) = 0;
        virtual void DestroyRenderView(uint64_t id) = 0;
        virtual void SetRenderViewCamera(uint64_t id, const RenderViewCamera* camera) = 0;
    };

    XG_DECLARE_MODULE_FUNCTION(CreateRenderer, Renderer*, Window&);
    XG_DECLARE_MODULE_FUNCTION(DestroyRenderer, void, Renderer*);

	XG_DECLARE_UNIQUE(Renderer)

    XG_API RendererUnique CreateRenderer(Window& window);
    XG_API void      DestroyRenderer(Renderer* renderer);

}

XG_DECLARE_MODULE(xg, Renderer)
