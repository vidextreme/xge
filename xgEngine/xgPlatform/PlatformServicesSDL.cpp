// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "WindowSDL.h"
#include "xgPlatformServices.h"

#include <SDL3/SDL.h>

namespace xg {

    namespace
    {
        class PlatformServicesSDL : public PlatformServices
        {
        public:
            Window* CreateSecondaryWindow(const WindowDesc& desc) override
            {
                auto* window = new WindowSDL(desc);
                if (!window->GetPlatformWindow())
                {
                    delete window;
                    return nullptr;
                }
                return window;
            }

            void DestroySecondaryWindow(Window* window) override
            {
                delete static_cast<WindowSDL*>(window);
            }

            int GetMonitors(MonitorInfo* monitors, int maxCount) override
            {
                int count = 0;
                SDL_DisplayID* displays = SDL_GetDisplays(&count);
                if (!displays)
                    return 0;
                int written = 0;
                for (int i = 0; i < count && monitors && written < maxCount; ++i)
                {
                    SDL_Rect bounds = {}, usable = {};
                    if (!SDL_GetDisplayBounds(displays[i], &bounds))
                        continue;
                    if (!SDL_GetDisplayUsableBounds(displays[i], &usable))
                        usable = bounds;
                    const float scale = SDL_GetDisplayContentScale(displays[i]);
                    MonitorInfo& info = monitors[written++];
                    info.Bounds = { bounds.x, bounds.y, bounds.w, bounds.h };
                    info.WorkBounds = { usable.x, usable.y, usable.w, usable.h };
                    info.DpiScale = scale > 0.0f ? scale : 1.0f;
                }
                SDL_free(displays);
                return monitors ? written : count;
            }

            bool GetGlobalMousePosition(float* x, float* y) override
            {
                float gx = 0.0f, gy = 0.0f;
                SDL_GetGlobalMouseState(&gx, &gy);
                if (x) *x = gx;
                if (y) *y = gy;
                return true;
            }
        };
    }

    XG_API PlatformServices* GetPlatformServices()
    {
        static PlatformServicesSDL services;
        return &services;
    }

} // namespace xg
