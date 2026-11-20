// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <cstdint>
#include "xgRenderer.h"
#include "xgResources.h"

namespace xg
{
    // Registers the "model" resource type (Async): a job worker reads <guid>.xdv (submeshes) and <guid>.xdh (nodes)
    // through the resource sources and places every part on the nodes that use its mesh; the main thread creates the
    // GPU meshes. The data is a ModelResourceInfo. The renderer must outlive the type.
    ResourceTypeID RegisterModelResources(ResourceSystem& resources, uint32_t moduleId, Renderer* renderer);
}
