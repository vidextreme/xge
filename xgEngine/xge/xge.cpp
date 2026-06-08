#include "xgEngine.h"
#include "xgScriptModule.h"

// If you have a CoreCLR or NativeAOT host, include it here:
// #include "CoreClrScriptHost.h"
// #include "NativeAotScriptHost.h"

int main(int argc, char** argv)
{
    xg::EngineConfig config;
    config.RendererModule = "xgRendererDX12.dll";

    xg::Engine engine;
    if (!engine.Initialize(config))
        return -1;

    //
    // Script modules are created OUTSIDE the engine.
    // Example (pseudo-code):
    //
    // auto* editorHost = new CoreClrScriptHost();
    // xg::ScriptModule* editorModule = editorHost->LoadModule("Editor.CoreCLR.dll");
    // engine.AddScriptModule("editor", editorModule);
    //
    // auto* gameHost = new CoreClrScriptHost();
    // xg::ScriptModule* gameModule = gameHost->LoadModule("Game.CoreCLR.dll");
    // engine.AddScriptModule("game", gameModule);
    //
    // For now, we run with no script modules.
    //

    engine.Run();
    engine.Shutdown();

    return 0;
}
