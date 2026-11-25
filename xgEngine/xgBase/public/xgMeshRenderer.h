// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <cstdint>
#include "xgComponentData.h"
#include "xgResources.h"

namespace xg
{
    enum class MeshRendererFlags : uint32_t
    {
        None = 0,
        Hidden = 1u << 0,
    };

    // A mesh drawer: an imported model asset drawn with a material asset (zero GUIDs = nothing / the built-in look).
    // Lives in the "xg.MeshRenderer" pool, so components of any module (CoreCLR, NativeAOT, native) can draw.
    struct MeshRendererData
    {
        ResourceGUID Model;
        ResourceGUID Material;
        MeshRendererFlags Flags;
        uint32_t Reserved;
    };
    static_assert(sizeof(MeshRendererData) == 40, "MeshRendererData is mirrored in managed code");

    inline constexpr const char* MeshRendererDataKey = "xg.MeshRenderer";

    // Draws every element of the "xg.MeshRenderer" pool: it acquires each element's model and material resources (and
    // follows changes and rebuilt assets), places the model's parts with the actor's transform (identity without one)
    // and hands the list to the renderer's scene draws once a frame, in edit mode too. Main thread only.
    class MeshRenderSystem : public Subsystem
    {
        XG_DECLARE_BASE_SUBSYSTEM(MeshRenderSystem)

    public:
        virtual ~MeshRenderSystem() = default;

        virtual DataTypeID GetDataType() const = 0;
        // Draws handed to the renderer last frame, and how many of them had their material resolved.
        virtual int32_t GetDrawCount() const = 0;
        virtual int32_t GetMaterialDrawCount() const = 0;

        // The renderer material of a Ready "material" resource, as the scene draws it (0 until its shader and textures
        // are loaded). Main thread only.
        virtual uint64_t ResolveMaterial(ResourceHandle material) = 0;
    };
}
