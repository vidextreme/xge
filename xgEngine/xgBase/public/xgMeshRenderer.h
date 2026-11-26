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

    enum class MeshRendererSubmeshFlags : uint32_t
    {
        None = 0,
        // The submesh isn't drawn.
        Hidden = 1u << 0,
    };

    // One submesh's setting, keyed by the submesh item GUID of the model's meta: its material (zero = the default
    // material) and whether it is drawn.
    struct MeshRendererSubmesh
    {
        ResourceGUID Submesh;
        ResourceGUID Material;
        MeshRendererSubmeshFlags Flags;
        uint32_t Reserved[3];
    };
    static_assert(sizeof(MeshRendererSubmesh) == 48, "MeshRendererSubmesh is mirrored in managed code");

    inline constexpr uint32_t MeshRendererMaxSubmeshes = 8;

    // A mesh drawer: an imported model asset drawn with materials (zero GUIDs = nothing / the default material).
    // Submeshes[0..SubmeshCount) set a material per submesh or hide it; a submesh without an entry draws with Material.
    // Lives in the "xg.MeshRenderer" pool, so components of any module (CoreCLR, NativeAOT, native) can draw.
    struct MeshRendererData
    {
        ResourceGUID Model;
        ResourceGUID Material;
        MeshRendererFlags Flags;
        uint32_t SubmeshCount;
        MeshRendererSubmesh Submeshes[MeshRendererMaxSubmeshes];
    };
    static_assert(sizeof(MeshRendererData) == 424, "MeshRendererData is mirrored in managed code");

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

        // Replaces the actors hidden in the editor: their scene draws (mesh renderers and object drivers) get
        // SceneDrawFlags::EditorHidden, which the Scene view skips. Main thread only.
        virtual void SetEditorHidden(const ActorID* actors, uint32_t count) = 0;
        virtual bool IsEditorHidden(ActorID actor) const = 0;

        // Editor preview of an unsaved material: the actor's parts that draw this material GUID draw rendererMaterial
        // instead. A zero rendererMaterial (or null material) removes the actor's override. Main thread only.
        virtual void SetEditorMaterialOverride(ActorID actor, const ResourceGUID* material, uint64_t rendererMaterial) = 0;
    };
}
