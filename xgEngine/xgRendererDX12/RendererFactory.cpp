// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "RendererDX12.h"

XG_MODULE_EXPORT xg::Renderer* XG_CreateRenderer(xg::Window& window)
{
    auto* renderer = new xg::RendererDX12();
    if (!renderer->Initialize(window))
    {
        delete renderer;
        return nullptr;
    }
    return renderer;
}

XG_MODULE_EXPORT void XG_DestroyRenderer(xg::Renderer* renderer)
{
    delete renderer;
}
