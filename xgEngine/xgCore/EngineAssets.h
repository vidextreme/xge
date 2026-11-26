// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once
#include "xgResources.h"

namespace xg
{
    // GUIDs of built-in assets the engine itself relies on (gameroot/engine/assets). They are the GUIDs in those assets'
    // committed metas, so they never change.

    // materials/error.xcm: drawn in place of a material whose data or shader failed to load.
    constexpr ResourceGUID ErrorMaterialGuid{ 0xf61966b0fd4a4a47ull, 0x93f8781f42a56114ull };

    // materials/default.xcm: drawn for meshes and object parts without a material (None).
    constexpr ResourceGUID DefaultMaterialGuid{ 0x8110412841c64b5bull, 0xa4326f7fee93cfd4ull };
}
