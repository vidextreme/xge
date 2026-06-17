#pragma once
#include "xgBase.h"
#include "xgModules.h"

namespace xg {

    class Window;

    class Renderer {
    public:
        virtual ~Renderer() = default;
        virtual void Render() = 0;
        virtual void* GetNativeHandle() const = 0;
    };

    XG_DECLARE_MODULE_FUNCTION(CreateRenderer, Renderer*, Window&);
    XG_DECLARE_MODULE_FUNCTION(DestroyRenderer, void, Renderer*);

    XG_API Renderer* CreateRenderer(Window& window);
    XG_API void      DestroyRenderer(Renderer* renderer);

}

XG_DECLARE_MODULE(xg, Renderer)