// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgRenderer.h"
#include "xgModules.h"
#include "xgLog.h"

// Generate EnsureRendererLoaded() using the macro system
XG_DEFINE_MODULE_LOADER(xg, Renderer);

namespace xg
{
    void DestroyRendererRaw(Renderer* renderer)
    {
        if (!renderer)
            return;

        EnsureRendererLoaded();

        // Extract raw pointer before reset

        XG_MODULE_PROCEDURE(xg, Renderer, XG_DestroyRenderer);
        if (!XG_MODULE_CALL(XG_DestroyRenderer))
            return;
        XG_MODULE_CALL(XG_DestroyRenderer)(renderer);
    }
    void DestroyRenderer(RendererUnique& renderer)
    {
        if (!renderer)
            return;

        DestroyRendererRaw(renderer.get());

        renderer.reset();
    }
    RendererUnique CreateRenderer(Window& window)
    {
        EnsureRendererLoaded();

        XG_MODULE_PROCEDURE(xg, Renderer, XG_CreateRenderer);
        if (!XG_MODULE_CALL(XG_CreateRenderer))
        {
            xg::Log(xg::MessageType::Error, "Renderer DLL has no XG_CreateRenderer export: %s", xg::RendererDLL ? xg::RendererDLL : "(null)");
            return RendererUnique(XG_EMPTY_UNIQUE_HANDLE());
        }
        xgUniqueHandle handle = xgUniqueHandle({ XG_MODULE_CALL(XG_CreateRenderer)(window), (void(*)(void*)) &DestroyRendererRaw});

        return RendererUnique(handle);
    }
}

#pragma message("Compiling xgRendererAPI.cpp with XG_API = " XG_STRINGIFY(XG_API))
