#include "pch.h"
#include "public/xgRenderer.h"
#include "public/xgModules.h"

//
// Generate EnsureRendererLoaded() using the macro system
//
XG_DEFINE_MODULE_LOADER(xg, Renderer);

namespace xg
{
    Renderer* CreateRenderer(Window& window)
    {
        EnsureRendererLoaded();

        XG_MODULE_PROCEDURE(xg, Renderer, CreateRenderer);
        return XG_MODULE_CALL(CreateRenderer)(window);
    }

    void DestroyRenderer(Renderer* renderer)
    {
        if (!renderer)
            return;

        EnsureRendererLoaded();

        XG_MODULE_PROCEDURE(xg, Renderer, DestroyRenderer);
        XG_MODULE_CALL(DestroyRenderer)(renderer);
    }
}

#pragma message("Compiling xgRendererAPI.cpp with XG_API = " XG_STRINGIFY(XG_API))
