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
    class Logger;

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

    // GPU texture formats. The values match the editor's TextureHardwareFormat and a .xdt TXPX chunk's Extra.
    enum class TextureFormat : uint32_t
    {
        RGBA8 = 1,
        BC1 = 2,
        BC3 = 3,
        BC4 = 4,
        BC5 = 5,
    };

    enum class TextureFlags : uint32_t
    {
        None = 0,
        // Color data stored in sRGB: sampled through an sRGB view, so shaders read linear values.
        SRGB = 1u << 0,
    };

    struct TextureDesc
    {
        int32_t Width;
        int32_t Height;
        int32_t MipCount;
        TextureFormat Format;
        TextureFlags Flags;
    };

    // One mip level, tightly packed rows (RowPitch = bytes per row of pixels or of 4x4 blocks).
    struct TextureMip
    {
        const void* Data;
        uint64_t Size;
        uint32_t RowPitch;
    };

    // The data of a Ready "texture" resource (ResourceSystem::GetData), loaded from a .xdt by breadcrumb
    // <file-guid>/<texture item guid>. Usage is the editor's TextureUsage (0 color, 1 normal, 2 luminance).
    struct TextureResourceInfo
    {
        uint64_t TextureId;
        int32_t Width;
        int32_t Height;
        int32_t MipCount;
        TextureFormat Format;
        uint32_t Usage;
        TextureFlags Flags;
    };

    inline constexpr const char* TextureResourceType = "texture";

    // How a render view's test cube reads its texture.
    enum class RenderViewTextureMode : int32_t
    {
        Color = 0,
        // Tangent-space normal in RG (BC5): Z is rebuilt and the normal is shown as a color.
        Normal = 1,
        // One channel in R (BC4), shown as gray.
        Luminance = 2,
    };

    class Renderer : public UIRenderTarget {
    public:
        virtual void Render() = 0;
        virtual void* GetNativeHandle() const = 0;

        // Offscreen 3D views. The returned id is also a valid UI texture id (draw it in a panel).
        virtual uint64_t CreateRenderView(int32_t width, int32_t height) = 0;
        virtual bool ResizeRenderView(uint64_t id, int32_t width, int32_t height) = 0;
        virtual void DestroyRenderView(uint64_t id) = 0;
        virtual void SetRenderViewCamera(uint64_t id, const RenderViewCamera* camera) = 0;

        // Sampled textures with mips. The id is also a valid UI texture id; destroy it with DestroyTexture.
        // Main thread only. Returns 0 when the format isn't supported or creation fails.
        virtual bool SupportsTextureFormat(TextureFormat format) const = 0;
        virtual uint64_t CreateTexture(const TextureDesc* desc, const TextureMip* mips) = 0;
        virtual void DestroyTexture(uint64_t id) = 0;

        // Texture shown on the view's test cube (0 = untextured).
        virtual void SetRenderViewTexture(uint64_t viewId, uint64_t textureId, RenderViewTextureMode mode) = 0;
    };

    XG_DECLARE_MODULE_FUNCTION(XG_CreateRenderer, Renderer*, Window&);
    XG_DECLARE_MODULE_FUNCTION(XG_DestroyRenderer, void, Renderer*);
    // Optional: renderer DLLs link their own xgBase, so the engine hands them its logger before XG_CreateRenderer.
    XG_DECLARE_MODULE_FUNCTION(XG_RendererSetLogger, void, Logger*);

	XG_DECLARE_UNIQUE(Renderer)

    XG_API RendererUnique CreateRenderer(Window& window);
    XG_API void      DestroyRenderer(Renderer* renderer);

}

XG_DECLARE_MODULE(xg, Renderer)
