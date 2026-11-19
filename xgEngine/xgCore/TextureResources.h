// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <cstdint>
#include "xgRenderer.h"
#include "xgResources.h"

namespace xg
{
    // Registers the "texture" resource type (Async): a job worker reads <file-guid>.xdt through the resource sources and
    // picks the item's first TXPX format the renderer supports; the main thread uploads it. The data is a
    // TextureResourceInfo. The renderer must outlive the type.
    ResourceTypeID RegisterTextureResources(ResourceSystem& resources, uint32_t moduleId, Renderer* renderer);
}
