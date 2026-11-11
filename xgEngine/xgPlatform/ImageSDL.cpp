// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgImage.h"

#include <SDL3/SDL.h>
#include <cstdlib>
#include <cstring>

namespace xg {

    XG_API bool LoadImageRGBA(const char* path, ImageRGBA* image)
    {
        if (!image)
            return false;
        *image = {};
        if (!path)
            return false;

        SDL_Surface* loaded = SDL_LoadSurface(path);
        if (!loaded)
            return false;
        SDL_Surface* rgba = SDL_ConvertSurface(loaded, SDL_PIXELFORMAT_RGBA32);
        SDL_DestroySurface(loaded);
        if (!rgba)
            return false;

        const size_t rowBytes = static_cast<size_t>(rgba->w) * 4;
        uint8_t* pixels = static_cast<uint8_t*>(std::malloc(rowBytes * static_cast<size_t>(rgba->h)));
        if (pixels)
        {
            for (int y = 0; y < rgba->h; ++y)
                std::memcpy(pixels + rowBytes * y, static_cast<const uint8_t*>(rgba->pixels) + static_cast<size_t>(rgba->pitch) * y, rowBytes);
            image->Width = rgba->w;
            image->Height = rgba->h;
            image->Pixels = pixels;
        }
        SDL_DestroySurface(rgba);
        return pixels != nullptr;
    }

    XG_API void FreeImageRGBA(ImageRGBA* image)
    {
        if (!image)
            return;
        std::free(image->Pixels);
        *image = {};
    }

} // namespace xg
