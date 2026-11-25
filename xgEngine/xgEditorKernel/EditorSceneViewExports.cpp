// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "EditorSceneViewExports.h"
#include "xgRenderer.h"
#include "xgInteropUtil.h"

namespace
{
    xg::Renderer* _sceneRenderer = nullptr;
}

namespace xg
{
    void SetSceneViewRenderer(Renderer* renderer)
    {
        _sceneRenderer = renderer;
    }
}

XG_MODULE_EXPORT int XG_EditorHasSceneRenderer(void)
{
    return _sceneRenderer ? 1 : 0;
}

XG_MODULE_EXPORT uint64_t XG_EditorCreateSceneView(int32_t width, int32_t height)
{
    return _sceneRenderer ? _sceneRenderer->CreateRenderView(width, height) : 0;
}

XG_MODULE_EXPORT int XG_EditorResizeSceneView(uint64_t id, int32_t width, int32_t height)
{
    return _sceneRenderer && _sceneRenderer->ResizeRenderView(id, width, height) ? 1 : 0;
}

XG_MODULE_EXPORT void XG_EditorDestroySceneView(uint64_t id)
{
    if (_sceneRenderer)
        _sceneRenderer->DestroyRenderView(id);
}

XG_MODULE_EXPORT void XG_EditorSetSceneViewCamera(uint64_t id, const xg::RenderViewCamera* camera)
{
    if (!_sceneRenderer || !camera)
        return;
    const xg::RenderViewCamera local = xg::interop::Load(camera);
    _sceneRenderer->SetRenderViewCamera(id, &local);
}

XG_MODULE_EXPORT void XG_EditorSetSceneViewDraws(uint64_t id, const void* draws, uint32_t count)
{
    if (_sceneRenderer)
        _sceneRenderer->SetRenderViewDraws(id, static_cast<const xg::RenderViewDraw*>(draws), draws ? count : 0);
}

XG_MODULE_EXPORT void XG_EditorSetSceneViewMaterialDraws(uint64_t id, const void* draws, uint32_t count)
{
    if (_sceneRenderer)
        _sceneRenderer->SetRenderViewMaterialDraws(id, static_cast<const xg::SceneDraw*>(draws), draws ? count : 0);
}

XG_MODULE_EXPORT void XG_EditorSetSceneViewFlags(uint64_t id, uint32_t flags)
{
    if (_sceneRenderer)
        _sceneRenderer->SetRenderViewFlags(id, static_cast<xg::RenderViewFlags>(flags));
}

XG_MODULE_EXPORT uint64_t XG_EditorCreateMesh(const void* desc)
{
    return _sceneRenderer && desc ? _sceneRenderer->CreateMesh(static_cast<const xg::MeshDesc*>(desc)) : 0;
}

XG_MODULE_EXPORT void XG_EditorDestroyMesh(uint64_t id)
{
    if (_sceneRenderer)
        _sceneRenderer->DestroyMesh(id);
}

XG_MODULE_EXPORT uint64_t XG_EditorCreateMaterial(const void* desc)
{
    return _sceneRenderer && desc ? _sceneRenderer->CreateMaterial(static_cast<const xg::MaterialDesc*>(desc)) : 0;
}

XG_MODULE_EXPORT int XG_EditorUpdateMaterial(uint64_t id, const void* desc)
{
    return _sceneRenderer && desc && _sceneRenderer->UpdateMaterial(id, static_cast<const xg::MaterialDesc*>(desc)) ? 1 : 0;
}

XG_MODULE_EXPORT int XG_EditorReadSceneView(uint64_t id, void* rgba, uint64_t capacity)
{
    return _sceneRenderer && _sceneRenderer->ReadRenderView(id, rgba, capacity) ? 1 : 0;
}

XG_MODULE_EXPORT void XG_EditorDestroyMaterial(uint64_t id)
{
    if (_sceneRenderer)
        _sceneRenderer->DestroyMaterial(id);
}

XG_MODULE_EXPORT void XG_EditorSetSceneViewTexture(uint64_t id, uint64_t textureId, int32_t mode)
{
    if (!_sceneRenderer || mode < 0 || mode > 2)
        return;
    _sceneRenderer->SetRenderViewTexture(id, textureId, static_cast<xg::RenderViewTextureMode>(mode));
}
