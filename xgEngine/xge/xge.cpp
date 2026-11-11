// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include <map>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include "xgJson.h"
#include "xgReflectJson.h"

#include "xgStream.h"
#include "xgEngine.h"
#include <cstdlib>
#include <cstring>

void _LogMsg(xg::MessageType _type, const char* _message)
{
    printf("%s\n", _message);
    fflush(stdout);
}

static bool LoadGameConfig(xg::Engine& engine, const wchar_t* path, xg::EngineConfig& config, xg::ReflectStringStore& strings)
{
    const xg::TypeSchema* schema = engine.GetTypeRegistry()->Get(xg::EngineConfig::TypeName);
    if (!schema)
    {
        xg::Log(xg::MessageType::Warning, "%s is not registered, using default config", xg::EngineConfig::TypeName);
        return false;
    }

    xg::FileUnique file = xg::LoadFile(path, xg::FileAccessMode::Read);
    if (!file)
    {
        xg::Log(xg::MessageType::Warning, "%ls not found, using default config", path);
        return false;
    }

    xg::Json json;
    const bool loaded = json.Load(*file);
    file->Close();
    if (!loaded)
    {
        xg::Log(xg::MessageType::Warning, "%ls is not valid JSON, using default config", path);
        return false;
    }

    xg::ReadReflected(json, *schema, &config, strings);
    xg::Log(xg::MessageType::Info, "%ls: %s", path, xg::DescribeReflected(*schema, &config).c_str());
    return true;
}

static std::string AbsolutePath(const char* path)
{
    char buffer[4096];
    return _fullpath(buffer, path, sizeof(buffer)) ? std::string(buffer) : std::string(path);
}

int main(int argc, char** argv)
{
    xg::Engine engine;
    engine.AddLogCallback(_LogMsg);
    xg::SetLogger(engine.GetLogger());

    xg::ReflectStringStore configStrings;
    xg::EngineConfig config;
    LoadGameConfig(engine, L"gameconfig.json", config, configStrings);

    std::string projectPath = config.ProjectPath ? config.ProjectPath : "";
    std::string newProjectPath;
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (std::strcmp(argv[i], "--project") == 0)
            projectPath = argv[++i];
        else if (std::strcmp(argv[i], "--new-project") == 0)
            newProjectPath = projectPath = argv[++i];
    }
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--no-script-build") == 0)
            config.ProjectAutoBuild = false;
    }
    if (!projectPath.empty())
        projectPath = AbsolutePath(projectPath.c_str());
    if (!newProjectPath.empty())
        newProjectPath = projectPath;
    config.ProjectPath = projectPath.empty() ? nullptr : projectPath.c_str();
    config.NewProjectPath = newProjectPath.empty() ? nullptr : newProjectPath.c_str();
    if (config.ProjectPath)
        xg::Log(xg::MessageType::Info, "Project: %s%s", config.ProjectPath, config.NewProjectPath ? " (new)" : "");

    engine.MainWindow = xg::NewWindow("xgEngine", 1280, 720);
    if (!engine.Initialize(config))
        return -1;

    //
    // Load Editor.CoreCLR.dll using a CoreCLR host
    //
    //xg::ScriptModule* module =
    //    engine.AddScriptModule(
    //        "editor.kernel",
    //        "xgEditorKernel.dll", nullptr, "engine me");

    xg::ScriptModule* module =
        engine.AddScriptModule(
            "editor.kernel",
            "xgEditorKernel.dll", nullptr, "group.main");

    if (!module)
    {
		xg::Log(xg::MessageType::Error, "Failed to load Editor.CoreCLR.dll");
    }

	xg::Log(xg::MessageType::Info, "Initialization complete. Entering main loop.");

    //
    // Run engine loop
    //
    engine.Run();

    //
    // Shutdown
    //
    engine.Shutdown();
    
    return 0;
}
