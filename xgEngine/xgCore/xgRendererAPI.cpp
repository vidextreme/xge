#include "pch.h"
#include "public/xgRenderer.h"
#include "public/xgModules.h"

#define XG_STRINGIFY2(x) #x
#define XG_STRINGIFY(x) XG_STRINGIFY2(x)

namespace xg {

    static bool rendererLoaded = false;

    static void EnsureRendererLoaded()
    {
        if (!rendererLoaded)
        {
            //xg::RendererDLL = "xgRendererDX12.dll";
            xg::LoadRendererLib();
            rendererLoaded = true;
        }
    }

    Renderer* CreateRenderer(Window& window)
    {
        EnsureRendererLoaded();

        XG_MODULE_PROCEDURE(xg, Renderer, CreateRenderer);
        return XG_MODULE_CALL(CreateRenderer)(window);
    }

    void DestroyRenderer(Renderer* renderer)
    {
        EnsureRendererLoaded();

        XG_MODULE_PROCEDURE(xg, Renderer, DestroyRenderer);
        XG_MODULE_CALL(DestroyRenderer)(renderer);
    }

}

#pragma message("Compiling xgRendererAPI.cpp with XG_API = " XG_STRINGIFY(XG_API))
