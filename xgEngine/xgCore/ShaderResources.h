// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include <cstdint>
#include "xgRenderer.h"
#include "xgResources.h"

namespace xg
{
    // Registers the "shader" resource type (Async): a job worker reads <file-guid>.xds and keeps the renderer's bytecode
    // (GetShaderFormat) and the param and texture tables; the main thread creates the shader. The data is a
    // ShaderResourceInfo. The renderer must outlive the type.
    ResourceTypeID RegisterShaderResources(ResourceSystem& resources, uint32_t moduleId, Renderer* renderer);
}
