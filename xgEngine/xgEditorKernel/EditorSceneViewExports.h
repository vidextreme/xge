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
// Texture on the view's test cube (a renderer texture id, e.g. from a "texture" resource; 0 = none). mode is
// xg::RenderViewTextureMode (0 color, 1 normal, 2 luminance).
XG_MODULE_EXPORT void XG_EditorSetSceneViewTexture(uint64_t id, uint64_t textureId, int32_t mode);

namespace xg
{
    class Renderer;
    void SetSceneViewRenderer(Renderer* renderer);
}
