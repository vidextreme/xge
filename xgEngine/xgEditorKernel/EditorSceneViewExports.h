// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
//
// Editor-only C ABI over the active renderer's offscreen render views, so managed editor code can own 3D views.
// The view id doubles as a UI texture id (xg::imui::SceneViewState::TextureId).
#pragma once
#include "xgBase.h"
#include <stdint.h>

namespace xg
{
    struct RenderViewCamera;
}

XG_MODULE_EXPORT int XG_EditorHasSceneRenderer(void);
XG_MODULE_EXPORT uint64_t XG_EditorCreateSceneView(int32_t width, int32_t height);
XG_MODULE_EXPORT int XG_EditorResizeSceneView(uint64_t id, int32_t width, int32_t height);
XG_MODULE_EXPORT void XG_EditorDestroySceneView(uint64_t id);
XG_MODULE_EXPORT void XG_EditorSetSceneViewCamera(uint64_t id, const xg::RenderViewCamera* camera);

namespace xg
{
    class Renderer;
    void SetSceneViewRenderer(Renderer* renderer);
}
