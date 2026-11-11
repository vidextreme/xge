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
#include <filesystem>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace
{
    namespace fs = std::filesystem;

    // File name of the config in use, for log messages.
    std::string _configName = "gameconfig.json";

    enum class ConfigResult
    {
        Loaded,
        Missing,
        Invalid,
    };

    fs::path ExecutableDirectory()
    {
        std::wstring buffer(MAX_PATH, L'\0');
        for (;;)
        {
            const DWORD length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
            if (length == 0)
                return fs::current_path();
            if (length < buffer.size())
            {
                buffer.resize(length);
                return fs::path(buffer).parent_path();
            }
            buffer.resize(buffer.size() * 2);
        }
    }

    std::string ToUtf8(const fs::path& path)
    {
        const std::u8string text = path.u8string();
        return std::string(text.begin(), text.end());
    }

    fs::path FromUtf8(const char* text)
    {
        return fs::path(reinterpret_cast<const char8_t*>(text));
    }
}

void _LogMsg(xg::MessageType _type, const char* _message)
{
    printf("%s\n", _message);
    fflush(stdout);
}

static ConfigResult LoadGameConfig(xg::Engine& engine, const wchar_t* path, xg::Json& json, xg::EngineConfig& config, xg::ReflectStringStore& strings)
{
    const xg::TypeSchema* schema = engine.GetTypeRegistry()->Get(xg::EngineConfig::TypeName);
    if (!schema)
    {
        xg::Log(xg::MessageType::Warning, "%s is not registered, using default config", xg::EngineConfig::TypeName);
        return ConfigResult::Invalid;
    }

    xg::FileUnique file = xg::LoadFile(path, xg::FileAccessMode::Read);
    if (!file)
        return ConfigResult::Missing;

    const bool loaded = json.Load(*file);
    file->Close();
    if (!loaded)
        return ConfigResult::Invalid;

    if (json.Has(xg::EngineConfig::TypeName))
    {
        xg::ReadReflected(json, *schema, &config, strings, xg::EngineConfig::TypeName);
    }
    else
    {
        xg::Log(xg::MessageType::Warning, "%ls has no \"%s\" table; reading engine settings from the top level (deprecated)",
            path, xg::EngineConfig::TypeName);
        xg::ReadReflected(json, *schema, &config, strings);
    }
    xg::Log(xg::MessageType::Info, "%ls: %s", path, xg::DescribeReflected(*schema, &config).c_str());
    return ConfigResult::Loaded;
}

// Loads the module tree stored as an object under key: each member's name is the module id and its value is either
// the module path (string) or { "path", "group", "dispatchgroup", "children" }. group/dispatchgroup default to the
// parent's. Members load in file order; a parent's Init runs before its children load.
static int LoadScriptModules(xg::Engine& engine, const xg::Json& json, const char* key, xg::ScriptModule* parent,
    const char* inheritedGroup, const char* inheritedDispatch, xg::ReflectStringStore& strings)
{
    const xg::TypeSchema* schema = engine.GetTypeRegistry()->Get(xg::ScriptModuleDesc::TypeName);
    if (!schema)
    {
        xg::Log(xg::MessageType::Error, "%s is not registered; cannot load script modules", xg::ScriptModuleDesc::TypeName);
        return 0;
    }

    int loaded = 0;
    const int count = json.GetObjectSize(key);
    for (int i = 0; i < count; ++i)
    {
        xg::ScriptModuleDesc desc;
        desc.Id = strings.Store(json.GetObjectKey(key, i));
        xg::Json entry;
        const bool hasEntry = json.GetObjectItem(key, i, entry);
        if (hasEntry)
            xg::ReadReflected(entry, *schema, &desc, strings);
        else if (const char* path = json.GetObjectString(key, i))
            desc.Path = path[0] ? strings.Store(path) : nullptr;
        if (!desc.Group)
            desc.Group = inheritedGroup;
        if (!desc.DispatchGroup)
            desc.DispatchGroup = inheritedDispatch;
        if (!desc.Path)
        {
            xg::Log(xg::MessageType::Error, "%s: module %s needs a path", _configName.c_str(), desc.Id);
            continue;
        }

        xg::ScriptModule* module = engine.AddScriptModule(desc.Id, desc.Path, parent, desc.Group);
        if (!module)
        {
            xg::Log(xg::MessageType::Error, "%s: failed to load module %s (%s); skipping its children", _configName.c_str(), desc.Id, desc.Path);
            continue;
        }
        xg::Log(xg::MessageType::Info, "Module %s from %s (group %s, dispatch %s)", desc.Id, _configName.c_str(),
            desc.Group ? desc.Group : "default", desc.DispatchGroup ? desc.DispatchGroup : "-");
        ++loaded;
        if (!hasEntry)
            continue;
        const char* childrenKey = entry.FindKey("children");
        if (childrenKey)
        {
            const std::string children = childrenKey;
            loaded += LoadScriptModules(engine, entry, children.c_str(), module, desc.Group, desc.DispatchGroup, strings);
        }
    }
    return loaded;
}

int main(int argc, char** argv)
{
    xg::Engine engine;
    engine.AddLogCallback(_LogMsg);
    xg::SetLogger(engine.GetLogger());

    fs::path configPath = ExecutableDirectory() / "gameconfig.json";
    bool explicitConfig = false;
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (std::strcmp(argv[i], "--config") == 0)
        {
            configPath = fs::path(argv[++i]);
            explicitConfig = true;
        }
    }
    std::error_code pathError;
    configPath = fs::absolute(configPath, pathError).lexically_normal();
    _configName = ToUtf8(configPath.filename());

    xg::ReflectStringStore configStrings;
    xg::EngineConfig config;
    xg::Json gameConfig;
    const ConfigResult configResult = LoadGameConfig(engine, configPath.c_str(), gameConfig, config, configStrings);
    if (configResult != ConfigResult::Loaded)
    {
        const char* problem = configResult == ConfigResult::Missing ? "not found" : "could not be read as a valid config";
        if (explicitConfig)
        {
            xg::Log(xg::MessageType::Error, "Config %ls %s (--config)", configPath.c_str(), problem);
            return 1;
        }
        xg::Log(xg::MessageType::Warning, "Config %ls %s, using default settings", configPath.c_str(), problem);
    }
    else
    {
        xg::Log(xg::MessageType::Info, "Config: %ls", configPath.c_str());
    }

    std::string projectPath;
    if (config.ProjectPath && *config.ProjectPath)
        projectPath = ToUtf8((configPath.parent_path() / FromUtf8(config.ProjectPath)).lexically_normal());
    std::string newProjectPath;
    for (int i = 1; i + 1 < argc; ++i)
    {
        if (std::strcmp(argv[i], "--project") == 0)
            projectPath = ToUtf8(fs::absolute(fs::path(argv[++i]), pathError).lexically_normal());
        else if (std::strcmp(argv[i], "--new-project") == 0)
            newProjectPath = projectPath = ToUtf8(fs::absolute(fs::path(argv[++i]), pathError).lexically_normal());
        else if (std::strcmp(argv[i], "--config") == 0)
            ++i;
    }
    for (int i = 1; i < argc; ++i)
    {
        if (std::strcmp(argv[i], "--no-script-build") == 0)
            config.ProjectAutoBuild = false;
    }
    if (!newProjectPath.empty())
        newProjectPath = projectPath;

    const fs::path exeDirectory = ExecutableDirectory();
    if (fs::current_path(pathError) != exeDirectory)
    {
        fs::current_path(exeDirectory, pathError);
        if (!pathError)
            xg::Log(xg::MessageType::Info, "Working directory: %ls", exeDirectory.c_str());
    }
    config.ProjectPath = projectPath.empty() ? nullptr : projectPath.c_str();
    config.NewProjectPath = newProjectPath.empty() ? nullptr : newProjectPath.c_str();
    if (config.ProjectPath)
        xg::Log(xg::MessageType::Info, "Project: %s%s", config.ProjectPath, config.NewProjectPath ? " (new)" : "");

    engine.MainWindow = xg::NewWindow("xgEngine", 1280, 720);
    if (!engine.Initialize(config))
        return -1;

    if (LoadScriptModules(engine, gameConfig, "ScriptModules", nullptr, nullptr, nullptr, configStrings) == 0)
        xg::Log(xg::MessageType::Error, "%s: no script modules loaded (add a \"ScriptModules\" table)", _configName.c_str());

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
