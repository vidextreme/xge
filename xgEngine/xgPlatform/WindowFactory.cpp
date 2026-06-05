#include "pch.h"
#include "WindowSDL.h"

namespace xg {

    XG_API std::unique_ptr<Window> NewWindow(const char* title, int width, int height) {
        return std::make_unique<WindowSDL>(title, width, height);
    }

} // namespace xg


