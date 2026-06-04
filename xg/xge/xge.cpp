#include <memory>
#include "Window.h"

int main(int argc, char** argv)
{
    // Create the main engine window
    auto window = xg::CreateWindow("XG Engine", 1280, 720);


    while (!window->ShouldClose())
    {
        // Pump platform events
        window->PollEvents();

        // TODO: Add engine tick here
        // core->Tick();

        // TODO: Add renderer tick here
        // renderer->Render();

        // Simple exit condition for now
        if (window->IsMinimized())
        {
            // You can choose to skip rendering here
        }
    }

    return 0;
}
