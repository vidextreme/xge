// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "EditorSceneViewExports.h"
#include "xgRenderer.h"
#include "xgInteropUtil.h"

namespace
{
    xg::Renderer* s_sceneRenderer = nullptr;
}

namespace xg
{
    void SetSceneViewRenderer(Renderer* renderer)
    {
        s_sceneRenderer = renderer;
    }
}

XG_MODULE_EXPORT int XG_EditorHasSceneRenderer(void)
{
    return s_sceneRenderer ? 1 : 0;
}

XG_MODULE_EXPORT uint64_t XG_EditorCreateSceneView(int32_t width, int32_t height)
{
    return s_sceneRenderer ? s_sceneRenderer->CreateRenderView(width, height) : 0;
}

XG_MODULE_EXPORT int XG_EditorResizeSceneView(uint64_t id, int32_t width, int32_t height)
{
    return s_sceneRenderer && s_sceneRenderer->ResizeRenderView(id, width, height) ? 1 : 0;
}

XG_MODULE_EXPORT void XG_EditorDestroySceneView(uint64_t id)
{
    if (s_sceneRenderer)
        s_sceneRenderer->DestroyRenderView(id);
}

XG_MODULE_EXPORT void XG_EditorSetSceneViewCamera(uint64_t id, const xg::RenderViewCamera* camera)
{
    if (!s_sceneRenderer || !camera)
        return;
    const xg::RenderViewCamera local = xg::interop::Load(camera);
    s_sceneRenderer->SetRenderViewCamera(id, &local);
}
