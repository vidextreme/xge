#include "xgWindow.h"
#include "xgRenderer.h"

int main() {
    // Create the engine window
    auto window = xg::NewWindow("xgEngine", 1280, 720);

	xg::RendererDLL = "xgRendererDX12.dll";
    // Create the renderer backend (DX12, Vulkan, etc.)
    xg::Renderer* renderer = xg::CreateRenderer(*window);

    // Main loop
    while (!window->ShouldClose()) {
        window->PollEvents();
        renderer->Render();
    }

    // Cleanup
    xg::DestroyRenderer(renderer);
    return 0;
}
