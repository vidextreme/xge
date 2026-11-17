// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.
#include <map>
#include <string>
#include <memory>
#include <unordered_map>
#include <vector>
#include "xgJSON.h"
#include "xgReflectJSON.h"

#include "xgStream.h"
#include "xgPathUtils.h"
#include "xgEngine.h"
#include "xgPlatformServices.h"
#include "xgSplash.h"
#include "xgVersion.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>

namespace
{
    namespace fs = std::filesystem;

    // File name of the config in use, for log messages.
    std::string _configName = "gameconfig.json";

    // Splash progress: module loads share this range of the bar evenly.
    constexpr float ModuleProgressBegin = 0.15f;
    constexpr float ModuleProgressEnd = 0.95f;
    int _startupModuleTotal = 0;
    int _startupModuleIndex = 0;

    enum class ConfigResult
    {
        Loaded,
        Missing,
        Invalid,
    };

    std::string ToUtf8(const fs::path& path)
    {
        const std::u8string text = path.u8string();
        return std::string(text.begin(), text.end());
    }

    fs::path FromUtf8(const char* text)
    {
        return fs::path(reinterpret_cast<const char8_t*>(text));
    }

    fs::path ExecutableDirectory()
    {
        std::string buffer(4096, '\0');
        const size_t length = xg::GetExecutablePath(buffer.data(), buffer.size());
        if (length == 0)
            return fs::current_path();
        buffer.resize(length);
        return FromUtf8(buffer.c_str()).parent_path();
    }

    const char* RendererLabel(const char* module)
    {
        if (!module || !*module)
            return "SDL GPU";
        const std::string name = module;
        if (name.find("DX12") != std::string::npos)
            return "DX12";
        if (name.find("Vulkan") != std::string::npos)
            return "Vulkan";
        return module;
    }

    int CountScriptModules(const xg::JSON& json, const char* key)
    {
        int count = 0;
        const int size = json.GetObjectSize(key);
        for (int i = 0; i < size; ++i)
        {
            ++count;
            xg::JSON entry;
            if (!json.GetObjectItem(key, i, entry))
                continue;
            if (const char* childrenKey = entry.FindKey("children"))
            {
                const std::string children = childrenKey;
                count += CountScriptModules(entry, children.c_str());
            }
        }
        return count;
    }
}

void _LogMsg(xg::MessageType _type, const char* _message)
{
    printf("%s\n", _message);
    fflush(stdout);
}

static ConfigResult LoadGameConfig(xg::Engine& engine, const char* path, xg::JSON& json, xg::EngineConfig& config, xg::ReflectStringStore& strings)
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
        xg::Log(xg::MessageType::Warning, "%s has no \"%s\" table; reading engine settings from the top level (deprecated)",
            path, xg::EngineConfig::TypeName);
        xg::ReadReflected(json, *schema, &config, strings);
    }
    xg::Log(xg::MessageType::Info, "%s: %s", path, xg::DescribeReflected(*schema, &config).c_str());
    return ConfigResult::Loaded;
}

// Loads the module tree stored as an object under key: each member's name is the module id and its value is either
// the module path (string) or { "path", "group", "dispatchgroup", "children" }. group/dispatchgroup default to the
// parent's. Members load in file order; a parent's Init runs before its children load.
static int LoadScriptModules(xg::Engine& engine, const xg::JSON& json, const char* key, xg::ScriptModule* parent,
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
        xg::JSON entry;
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

        const float progress = ModuleProgressBegin + (ModuleProgressEnd - ModuleProgressBegin)
            * static_cast<float>(_startupModuleIndex++) / static_cast<float>((std::max)(_startupModuleTotal, 1));
        const std::string step = std::string("Loading module ") + desc.Id;
        engine.ReportStartup(step.c_str(), desc.Path, progress);
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
    const std::string configPathUtf8 = ToUtf8(configPath);

    xg::ReflectStringStore configStrings;
    xg::EngineConfig config;
    xg::JSON gameConfig;
    const ConfigResult configResult = LoadGameConfig(engine, configPathUtf8.c_str(), gameConfig, config, configStrings);
    if (configResult != ConfigResult::Loaded)
    {
        const char* problem = configResult == ConfigResult::Missing ? "not found" : "could not be read as a valid config";
        if (explicitConfig)
        {
            xg::Log(xg::MessageType::Error, "Config %s %s (--config)", configPathUtf8.c_str(), problem);
            return 1;
        }
        xg::Log(xg::MessageType::Warning, "Config %s %s, using default settings", configPathUtf8.c_str(), problem);
    }
    else
    {
        xg::Log(xg::MessageType::Info, "Config: %s", configPathUtf8.c_str());
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
        else if (std::strcmp(argv[i], "--no-splash") == 0)
            config.Splash = false;
    }
    if (!newProjectPath.empty())
        newProjectPath = projectPath;

    const fs::path exeDirectory = ExecutableDirectory();
    if (fs::current_path(pathError) != exeDirectory)
    {
        fs::current_path(exeDirectory, pathError);
        if (!pathError)
            xg::Log(xg::MessageType::Info, "Working directory: %s", ToUtf8(exeDirectory).c_str());
    }
    config.ProjectPath = projectPath.empty() ? nullptr : projectPath.c_str();
    config.NewProjectPath = newProjectPath.empty() ? nullptr : newProjectPath.c_str();
    if (config.ProjectPath)
        xg::Log(xg::MessageType::Info, "Project: %s%s", config.ProjectPath, config.NewProjectPath ? " (new)" : "");

    if (config.Splash)
    {
        const std::string projectName = projectPath.empty() ? std::string() : ToUtf8(FromUtf8(projectPath.c_str()).filename());
        xg::SplashDesc splash = {};
        splash.Size = sizeof(splash);
        splash.Title = "xge";
        splash.Tagline = "Cross-Game Engine";
        splash.Version = XGE_VERSION;
        splash.Config = XGE_BUILD_CONFIG;
        splash.Renderer = RendererLabel(config.RendererModule);
        splash.Project = projectName.c_str();
        splash.Copyright = "\xC2\xA9 2026 John David Uy \xC2\xB7 MIT";
        splash.LogoPath = "../editor/splash-mark.png";
        splash.FontDirectory = "../editor/fonts";
        engine.Splash = xg::NewSplashScreen(splash);
        if (!engine.Splash)
            xg::Log(xg::MessageType::Warning, "Splash screen unavailable");
    }
    _startupModuleTotal = CountScriptModules(gameConfig, "ScriptModules");

    engine.ReportStartup("Creating main window", "", 0.03f);
    const char* windowTitle = (config.WindowTitle && *config.WindowTitle) ? config.WindowTitle : "xgEngine";
    engine.MainWindow = xg::NewWindow(windowTitle, config.WindowWidth > 0 ? config.WindowWidth : 1280,
        config.WindowHeight > 0 ? config.WindowHeight : 720, engine.Splash ? xg::WindowCreateFlagsHidden : 0u);
    const bool hasRenderer = config.RendererModule && *config.RendererModule;
    engine.ReportStartup(hasRenderer ? "Creating renderer" : "Starting engine",
        hasRenderer ? (std::string(config.RendererModule) + ".dll").c_str() : "", 0.07f);
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
