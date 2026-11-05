// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once

struct ImDrawData;

namespace xg
{
    class Renderer;

    namespace ui_bridge
    {
        void ProcessTextures(Renderer& renderer, ImDrawData* drawData);
        void SubmitDrawData(Renderer& renderer, ImDrawData* drawData);
        void DestroyAllTextures(Renderer& renderer);
    }
}
