# xge command line and config files

> Status: current implementation (2026-11). See the [README](../README.md) for build steps and the [runtime architecture](xge-architecture.md) for how modules run once loaded.

`xge.exe` is the launcher. It reads a config file, creates the main window, starts the engine and loads the module tree the config describes. Everything that differs between the editor, the Project Hub or a game runtime comes from the config file and a few command-line options.

```
xge.exe [--config <file>] [--project <folder> | --new-project <folder>] [--no-script-build]
```

---

## Options

| Option | Argument | Effect |
|---|---|---|
| `--config <file>` | Config file, relative to the **current working directory** or absolute. | Uses this file instead of the default `gameconfig.json` next to `xge.exe`. A missing or unreadable file is an error: xge logs it and exits with code 1. |
| `--project <folder>` | Game project folder, relative to the working directory or absolute. | Opens that project in the editor (overrides `EngineConfig.ProjectPath`). |
| `--new-project <folder>` | Folder to create, relative to the working directory or absolute. | Creates a project there from the template, builds its scripts, then opens it. |
| `--no-script-build` | None | Skips the automatic script build when a project opens (same as `"ProjectAutoBuild": false`). The last good build is loaded. |

- Pass either `--project` or `--new-project`, not both. When both appear, the last one wins, but any `--new-project` makes the open a create.
- Unknown arguments are ignored, and so is an option that is missing its value at the end of the line.
- Paths with spaces need quotes: `xge.exe --project "D:\My Games\Space Shooter"`.

---

## Startup order

1. **Config file:** `--config <file>` if given; otherwise `gameconfig.json` in xge.exe's own folder (found through `GetModuleFileNameW`, so the launch folder doesn't matter). A missing **default** file isn't an error: xge warns and continues with built-in settings, which load no modules.
2. **Engine settings:** the `EngineConfig` table is read into the reflected `xg::EngineConfig` (`xgCore/public/xgEngineDef.h`).
3. **Paths:** the command-line paths are made absolute against the current working directory. A relative `EngineConfig.ProjectPath` is resolved against the **config file's folder**. `--project` / `--new-project` override it, and `--no-script-build` turns `ProjectAutoBuild` off.
4. **Working directory:** set to xge.exe's folder, because module DLLs, fonts (`../editor/fonts`) and `imgui.ini` are found relative to it.
5. **Window:** created from `WindowTitle`, `WindowWidth` and `WindowHeight`.
6. **Engine:** `Engine::Initialize` runs (the renderer module, the engine subsystems). If it fails, xge exits with code -1.
7. **Modules:** the `ScriptModules` table is loaded depth-first in file order, so each parent's `Init` runs before its children load. Then the frame loop runs until the main window closes.

Useful log lines (stdout) to confirm what was loaded:

```
INFO: D:\...\gameroot\bin\gameconfig.json: RendererModule=xgRendererDX12 EditorUI=false ...
INFO: Config: D:\...\gameroot\bin\gameconfig.json
INFO: Project: D:\...\samples\SampleGame
INFO: Module editor.kernel from gameconfig.json (group group.main, dispatch main)
```

| Exit code | Meaning |
|---|---|
| 0 | Normal shutdown (main window closed). |
| 1 | The `--config` file is missing or isn't a valid config. |
| -1 | `Engine::Initialize` failed. |

---

## Config file format

A config file is JSON with two tables. Table names keep their case. `EngineConfig` keys are PascalCase and module keys are lowercase. Keys are matched exactly first, then ignoring case. Member order is preserved, and it matters for `ScriptModules`.

### `EngineConfig`

| Key | Type | Default | Effect |
|---|---|---|---|
| `RendererModule` | string | empty | Renderer DLL to load, for example `xgRendererDX12`. When it's empty, the UI draws through xgPlatform's SDL GPU fallback (D3D12, no 3D views). |
| `EditorUI` | bool | `true` | `true` shows the editor shell prototype (menus, toolbar, mock panels). `false` leaves an empty dockspace with the OS frame, where the registered panels (Console, Hierarchy, ...) still dock. |
| `EditorMultiViewport` | bool | `true` | Panels dragged outside the main window become OS windows. |
| `ProjectPath` | string | empty | Game project to open. A relative path is resolved against the config file's folder. `--project` overrides it. |
| `ProjectAutoBuild` | bool | `true` | Build stale project scripts before loading them. `--no-script-build` sets it to false. |
| `WindowTitle` | string | `xgEngine` | Main window title. |
| `WindowWidth` | int | `1280` | Initial main window width. |
| `WindowHeight` | int | `720` | Initial main window height. |

A file without an `EngineConfig` table is still read as a flat list of these keys, with a deprecation warning.

### `ScriptModules`

The module tree, keyed by module id. Each member's value is either:
- the module path, as a string
- an object with these keys:

| Key | Meaning |
|---|---|
| `path` | Module file, relative to xge.exe's folder. A file with `.CoreCLR.` in its name loads through the CoreCLR host. Other DLLs load through the native host, which handles both plain native DLLs and NativeAOT modules. |
| `group` | Script host group; modules in one group share a host (for example one CoreCLR runtime). Defaults to the parent's group. |
| `dispatchgroup` | Scheduling group. It's reserved and not used yet. Defaults to the parent's. |
| `children` | Child modules, in the same form. |

Loading rules:
- Members load in file order, depth-first, and each parent is initialized before its children.
- A module that fails to load is logged, and its subtree is skipped.
- Nothing is hard-coded in xge. The editor kernel does reserve the id `editor` for the managed editor module: it removes that module before shutting the UI host down.
- Modules can add children at runtime with `ScriptRuntime.AddScriptModule` (outside a frame update). The editor uses it to load an open project's scripts as `game.project`.

---

## Shipped configs

### `gameroot/bin/gameconfig.json` (the editor, the default)

```json
{
    "EngineConfig": {
        "RendererModule": "xgRendererDX12",
        "EditorUI": false,
        "EditorMultiViewport": true
    },
    "ScriptModules": {
        "editor.kernel": { "path": "xgEditorKernel.dll", "group": "group.main", "dispatchgroup": "main", "children": {
            "game.kernel": { "path": "xgGameKernel.dll", "children": {
                "editor": "xgEditor.CoreCLR.dll",
                "game.demo": "GameDemoModule.dll"
            } }
        } }
    }
}
```

### `gameroot/bin/hub.config` (the Project Hub)

```json
{
    "EngineConfig": {
        "RendererModule": "xgRendererDX12",
        "EditorUI": false,
        "EditorMultiViewport": false,
        "WindowTitle": "Project Hub",
        "WindowWidth": 1100,
        "WindowHeight": 680
    },
    "ScriptModules": {
        "hub": { "path": "xgHub.CoreCLR.dll", "group": "group.main" }
    }
}
```

The hub module starts the UI host itself (no editor kernel). Its project list lives in `%USERPROFILE%\ExtremeFocusGames\XGE\hub.json`. Opening or creating a project starts a separate `xge.exe --project` / `--new-project` with the default config, then the hub closes.

---

## Examples

Run these from `gameroot/bin`, or from anywhere using the full path to xge.exe.

```powershell
# Editor (default gameconfig.json next to xge.exe)
.\xge.exe

# Project Hub
.\xge.exe --config hub.config

# Open a project; the scripts are built first if they changed
.\xge.exe --project ..\..\samples\SampleGame

# Open without building (load the last good build)
.\xge.exe --project ..\..\samples\SampleGame --no-script-build

# Create a project from the template, build it and open it
.\xge.exe --new-project "D:\Games\SpaceShooter"

# A config kept somewhere else (relative to the current folder)
cd D:\Work\Configs
D:\XFG\XFGames\XGE\gameroot\bin\xge.exe --config renderer-test.json
```

A config with a project set inside it (`ProjectPath` is relative to this file's folder):

```json
{
    "EngineConfig": { "RendererModule": "xgRendererDX12", "ProjectPath": "..\\..\\samples\\SampleGame" },
    "ScriptModules": {
        "editor.kernel": { "path": "xgEditorKernel.dll", "group": "group.main", "children": {
            "game.kernel": { "path": "xgGameKernel.dll", "children": { "editor": "xgEditor.CoreCLR.dll" } }
        } }
    }
}
```

A runtime-only tree without the editor: no editor kernel, so no UI. The window shows only what the modules render.

```json
{
    "EngineConfig": { "RendererModule": "xgRendererDX12", "WindowTitle": "Game Demo" },
    "ScriptModules": {
        "game.kernel": { "path": "xgGameKernel.dll", "group": "group.main", "children": {
            "game.demo": "GameDemoModule.dll"
        } }
    }
}
```

---

## Visual Studio

`xgEngine/xgHub.CoreCLR/Properties/launchSettings.json` defines two launch profiles. Set **xgHub.CoreCLR** as the startup project, and its Start button gets a dropdown:

| Profile | Runs |
|---|---|
| XGE (default config) | `xge.exe` (the editor) |
| XGE Project Hub | `xge.exe --config hub.config` |

Both start `$(TargetDir)xge.exe` in `gameroot/bin` with native debugging enabled.
