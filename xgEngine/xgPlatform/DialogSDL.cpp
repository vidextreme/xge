// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include "pch.h"
#include "xgDialogs.h"
#include "xgWindow.h"

#include <SDL3/SDL.h>
#include <string>

namespace xg {

    namespace
    {
        struct DialogRequest
        {
            DialogCallback Callback = nullptr;
            void* User = nullptr;
            std::string FilterName;
            std::string FilterPattern;
            SDL_DialogFileFilter Filter = {};
        };

        void SDLCALL OnDialogResult(void* userdata, const char* const* filelist, int)
        {
            DialogRequest* request = static_cast<DialogRequest*>(userdata);
            const char* path = (filelist && filelist[0]) ? filelist[0] : nullptr;
            if (request->Callback)
                request->Callback(request->User, path);
            delete request;
        }

        SDL_Window* ParentWindow(Window* parent)
        {
            return parent ? static_cast<SDL_Window*>(parent->GetPlatformWindow()) : nullptr;
        }

        const char* LocationOrNull(const char* location)
        {
            return (location && *location) ? location : nullptr;
        }
    }

    XG_API void ShowOpenFolderDialog(Window* parent, const char* defaultLocation, DialogCallback callback, void* user)
    {
        DialogRequest* request = new DialogRequest();
        request->Callback = callback;
        request->User = user;
        SDL_ShowOpenFolderDialog(OnDialogResult, request, ParentWindow(parent), LocationOrNull(defaultLocation), false);
    }

    XG_API void ShowOpenFileDialog(Window* parent, const char* filterName, const char* filterPattern,
        const char* defaultLocation, DialogCallback callback, void* user)
    {
        DialogRequest* request = new DialogRequest();
        request->Callback = callback;
        request->User = user;
        const bool filtered = filterPattern && *filterPattern;
        if (filtered)
        {
            request->FilterName = filterName && *filterName ? filterName : filterPattern;
            request->FilterPattern = filterPattern;
            request->Filter.name = request->FilterName.c_str();
            request->Filter.pattern = request->FilterPattern.c_str();
        }
        SDL_ShowOpenFileDialog(OnDialogResult, request, ParentWindow(parent), filtered ? &request->Filter : nullptr,
            filtered ? 1 : 0, LocationOrNull(defaultLocation), false);
    }

} // namespace xg
