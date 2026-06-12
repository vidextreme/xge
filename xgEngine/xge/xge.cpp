#include <map>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include "xgJson.h"
#include "xgJsonBackend.h"

#include "xgEngine.h"

int main(int argc, char** argv)
{
    xg::EngineConfig config;
    config.RendererModule = "xgRendererDX12.dll";

    xg::Json json;
    xg::JsonSerializer ser(json);
    xg::Serialize(ser, config);

    std::string out = json.ToString();
    printf("Serialized JSON:\n%s\n", out.c_str());

    //std::string out = 
    xg::Engine engine;
    if (!engine.Initialize(config))
        return -1;

    //
    // Load Editor.CoreCLR.dll using a CoreCLR host
    //
    xg::ScriptHost* usedHost =
        engine.AddScriptModule(
            "editor",
            "Editor.CoreCLR.dll");

    if (!usedHost)
    {
        printf("Failed to load Editor.CoreCLR.dll\n");
    }

    //
    // Run engine loop
    //
    engine.Run();

    //
    // Shutdown
    //
    engine.Shutdown();

    delete usedHost;
    return 0;
}
