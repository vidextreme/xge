// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <cstdint>
#include "xgRenderer.h"
#include "xgResources.h"

namespace xg
{
    // Registers the "material" resource type (Async): a job worker reads <file-guid>.xdm (shader GUID, params and
    // texture breadcrumbs by name). The data is a MaterialResourceInfo. Register it after the shader and texture types,
    // and unregister it before them. The renderer must outlive the type.
    ResourceTypeID RegisterMaterialResources(ResourceSystem& resources, uint32_t moduleId, Renderer* renderer, ResourceTypeID shaderType,
        ResourceTypeID textureType);

    // The renderer material of a Ready material resource (main thread). The first call acquires the shader and
    // textures it names; once they're loaded it maps the params and textures to the shader's slots (defaults for the
    // rest) and creates the material. Returns 0 until then, or the previous material while a rebuilt shader or texture
    // loads. Invalidated dependencies are re-acquired.
    uint64_t ResolveMaterial(ResourceHandle material);

    // True when the material can't draw as authored: its resource failed, or its shader failed to load or couldn't be
    // acquired. A material without a shader isn't broken (it draws the built-in lit look). Call after ResolveMaterial.
    bool IsMaterialBroken(ResourceHandle material);

    // The renderer material of the built-in error material (EngineAssets.h), acquired on first use; 0 while it loads or
    // when the engine data has none.
    uint64_t ResolveErrorMaterial();

    // The renderer material of the built-in default material, drawn for meshes without a material; 0 while it loads or
    // when the engine data has none (the renderer's lit look then).
    uint64_t ResolveDefaultMaterial();
}
