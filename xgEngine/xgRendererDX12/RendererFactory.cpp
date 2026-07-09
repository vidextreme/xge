// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "RendererDX12.h"

XG_MODULE_EXPORT xg::Renderer* CreateRenderer(xg::Window& window)
{
    return new xg::RendererDX12(window);
}

XG_MODULE_EXPORT void DestroyRenderer(xg::Renderer* renderer)
{
    delete renderer;
}
