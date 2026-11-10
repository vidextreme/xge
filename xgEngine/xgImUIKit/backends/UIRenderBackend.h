// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#pragma once

struct ImDrawData;

namespace xg
{
    class UIRenderTarget;

    namespace imui::detail::render
    {
        void ProcessTextures(UIRenderTarget& renderer, ImDrawData* drawData);
        void SubmitDrawData(UIRenderTarget& renderer, ImDrawData* drawData);
        void DestroyAllTextures(UIRenderTarget& renderer);
        // Multi-viewport: secondary ImGui viewports render into extra surfaces of the same target.
        void InitViewports(UIRenderTarget& renderer);
        void ShutdownViewports();
    }
}
