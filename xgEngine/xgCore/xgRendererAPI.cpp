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

        XG_MODULE_PROCEDURE(xg, Renderer, DestroyRenderer);
        XG_MODULE_CALL(DestroyRenderer)(renderer);
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

        XG_MODULE_PROCEDURE(xg, Renderer, CreateRenderer);
        xgUniqueHandle handle = xgUniqueHandle({ XG_MODULE_CALL(CreateRenderer)(window), (void(*)(void*)) &DestroyRendererRaw});

        return RendererUnique(handle);
    }
}

#pragma message("Compiling xgRendererAPI.cpp with XG_API = " XG_STRINGIFY(XG_API))
