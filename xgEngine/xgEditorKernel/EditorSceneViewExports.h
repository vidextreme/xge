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
// Meshes drawn in the view after its test cube (xg::RenderViewDraw: mesh id, texture id, world 4x4); count 0 clears.
XG_MODULE_EXPORT void XG_EditorSetSceneViewDraws(uint64_t id, const void* draws, uint32_t count);
// Material draws of the view (xg::SceneDraw: mesh id, material id, world 4x4); count 0 clears.
XG_MODULE_EXPORT void XG_EditorSetSceneViewMaterialDraws(uint64_t id, const void* draws, uint32_t count);
// xg::RenderViewFlags (1 hides the test cube and grid, 2 hides the scene's draws).
XG_MODULE_EXPORT void XG_EditorSetSceneViewFlags(uint64_t id, uint32_t flags);
// Orthographic size (half-height in world units; 0 = perspective).
XG_MODULE_EXPORT void XG_EditorSetSceneViewProjection(uint64_t id, float orthographicSize);
// Editor-owned renderer meshes (xg::MeshDesc) and materials (xg::MaterialDesc), e.g. a material preview's shape and
// the unsaved draft it shows. 0 = failed.
XG_MODULE_EXPORT uint64_t XG_EditorCreateMesh(const void* desc);
XG_MODULE_EXPORT void XG_EditorDestroyMesh(uint64_t id);
XG_MODULE_EXPORT uint64_t XG_EditorCreateMaterial(const void* desc);
XG_MODULE_EXPORT int XG_EditorUpdateMaterial(uint64_t id, const void* desc);
XG_MODULE_EXPORT void XG_EditorDestroyMaterial(uint64_t id);
// Copies the view's last rendered color as RGBA8 (width * height * 4 bytes). Between frames only. 1 = done.
XG_MODULE_EXPORT int XG_EditorReadSceneView(uint64_t id, void* rgba, uint64_t capacity);

namespace xg
{
    class Renderer;
    void SetSceneViewRenderer(Renderer* renderer);
}
