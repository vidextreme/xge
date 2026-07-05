#include "pch.h"
#include "WindowSDL.h"

namespace xg {

    XG_DECLARE_UNIQUE_TYPE(WindowSDL)
    XG_API WindowUnique NewWindow(const char* title, int width, int height) {
        return WindowUnique(MakeWindowSDLUniqueHandle(new WindowSDL(title, width, height)));
    }

} // namespace xg


