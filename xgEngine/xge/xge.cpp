#include <map>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include "xgJson.h"
#include "xgJsonBackend.h"
#include "xgStream.h"
#include "xgEngine.h"

void _LogMsg(xg::MessageType _type, const char* _message)
{
    printf("%s\n", _message);
}

int main(int argc, char** argv)
{
	xg::AddLogCallback(_LogMsg);
    xg::EngineConfig config;
    xg::Json json;
	xg::Log(xg::MessageType::Info, "Loading gameconfig.json");
    auto file = xg::LoadFile(L"gameconfig.json", xg::FileAccessMode::Read);
    if (!file)
    {
		//TODO: default config if file doesn't exist
		xg::Log(xg::MessageType::Error, "gameconfig.json not found, using default config");
        return -1;
    }
	json.Load(*file);
	xg::JsonDeserializer deser(json);
	xg::Deserialize(deser, config);
	file->Close();

    //xg::JsonSerializer ser(json);
    //xg::Serialize(ser, config);

    //std::string out = json.ToString();
	//auto file = xg::LoadFile(L"gameconfig.json", xg::FileAccessMode::Create | xg::FileAccessMode::Write);
    //json.Save(*file);
    //file->Close();
    //printf("Serialized JSON:\n%s\n", out.c_str());
    
    xg::Engine engine;
    if (!engine.Initialize(config))
        return -1;

    //
    // Load Editor.CoreCLR.dll using a CoreCLR host
    //
    xg::ScriptHost* usedHost =
        engine.AddScriptModule(
            "editor",
            "xgEditorKernel.dll");

    if (!usedHost)
    {
		xg::Log(xg::MessageType::Error, "Failed to load Editor.CoreCLR.dll");
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
