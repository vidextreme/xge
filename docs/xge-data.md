# XGE Data and Assets

> Status: current implementation (2026-11). Future design is listed at the end. See the [README](../README.md) for the overall status table.

This document covers how content gets from a project's `Assets/` folder to the GPU: asset identity and metadata, the `Library/` build pipeline, the chunk file format, runtime resources, and each asset family built so far (textures, meshes, shaders, materials, archetypes, scenes). An **asset** is a source file the editor works with (`dog.fbx`, `Bear.xcm`); a **resource** is the runtime object built from it (a GPU texture, a model, a renderer material).

| Piece | State |
|---|---|
| Asset GUIDs, metas, item tables, name trees | Working |
| Chunk file format | Working |
| `Library/` pipeline (GUID folders, manifests, build keys, import tags, job builds, cancellation) | Working |
| Runtime resource system (handles, async loads, sources, invalidation) | Working |
| Textures (PNG/BMP → BC1/3/4/5 or RGBA8, mips) | Working on DX12 and Vulkan |
| Meshes (FBX via ufbx, static meshes and hierarchy) | Working |
| Shaders (project HLSL with annotations) and materials (`.xcm`) | Working, with a Material Editor |
| Archetypes (`.xco`) | Working, with an Archetype Editor and Composition Actors |
| Scenes (`.xcs`) | Working; see [Scenes](xge-scenes.md) |
| Animation, flipbooks, atlases, player packaging | Planned |

---

## Layers and extensions

| Layer | Examples | Where | Made by | In source control |
|---|---|---|---|---|
| Source | `dog.fbx`, `cat.png`, `surface.tinted.hlsl` | `Assets/` | External tools | Yes |
| Meta | `dog.fbx.xmd`; the `meta` object inside a composition | Next to each imported file; inside each composition | Editor | Yes |
| Composition | `.xcm` material, `.xco` object, `.xcs` scene | `Assets/` | Editors in xge | Yes |
| Raw | `.xrv` raw mesh, `.xrh` raw hierarchy | `Library/` | Importers | No |
| Processed | `.xdt` texture, `.xdv` mesh, `.xdh` hierarchy, `.xds` shader, `.xdm` material, `.xdo` object | `Library/` | Processors | No |

- `.xc*` files are **compositions**: authored in the editor, they hold parameters and GUID references to processed data.
- `.xd*` files are processed, runtime-ready data. `.xr*` files are intermediate import results.
- `Library/` is disposable. Deleting it rebuilds everything from `Assets/` without breaking a reference.

## Identity: GUIDs, metas and names

- **Asset GUID:** every asset has a 32-hex-digit GUID. Imported sources keep it in a pretty-printed `<file>.xmd` next to them: `version`, `guid`, `tags`, `importer` (`import` and `process` settings) and `items`. Compositions embed the same fields in a top-level `meta` object instead, so a composition is a single file.
- **References** are breadcrumbs: `<file-guid>` or `<file-guid>/<chunk-guid>`. The file is found first, then the chunk inside it. Links between files use GUIDs only.
- **Item table:** a source's meta lists its generated items (`kind`, hierarchical name `path`, `guid`, optional `missing`). Chunk GUIDs are random on first import and kept. On reimport, items are matched by name and keep their GUID. A vanished item stays as `missing` and recovers its GUID if it returns. A renamed item gets a new GUID.
- **Names tree:** compositions record, in their meta, the path of every asset they reference (`names`: GUID → path). When a GUID no longer resolves, the editor repoints the reference to whatever asset now lives at the recorded path.
- **Reconciliation:**
  - a source without a meta gets one, unless an orphaned meta matches it (same name, or same size and content hash), in which case that meta moves next to it
  - copied files with duplicate GUIDs get new ones
  - orphaned metas are deleted after a short grace period
- **Moves and renames** in the Navigator move the meta with the file. Bookmarks, selection and the Navigator follow.

## Chunk files

Processed and raw files share one container (version 1, little-endian):

| Part | Layout |
|---|---|
| File header, 32 bytes | magic `XGCF`, version, file kind tag, flags, file GUID |
| Chunk header, 24 bytes | type tag (four characters), flags, 64-bit size, 64-bit extra |
| Chunk | header, payload, padding to 8 bytes |
| Item chunk | type `ITEM`: a 16-byte item GUID followed by typed data chunks |

Readers reject a bad magic, a newer version, truncated data, zero or repeated item GUIDs, and nested items. Native code reads through `xgBase/public/xgChunkFile.h`; the editor writes through `ChunkFile.cs`.

## The Library pipeline

- **Layout:** `Library/Data/<Type>/<guid>.<ext>`, one folder per data type and no per-asset folders, so renames don't move data. Each asset has a manifest next to its main file (`<guid>.xdt.json`) with the source name, build keys, generated files and import tags. It's written last, so an interrupted build runs again.
- **Importers and processors:** an importer turns a source into raw files; processors turn raw files into processed files. Build keys:
  - raw: source hash + import settings + importer version
  - processed: raw hash + process settings + processor versions
  - Compositions hash their content without the embedded meta, so editing tags rebuilds nothing.
- **Builds run as jobs:** the main thread reads metas and applies results; worker jobs import and process, up to four at once. A build reports its stage and progress in the status bar's task panel and can be cancelled between stages; cancelling keeps the last good output. A failed build keeps the last good output and isn't retried until its inputs change.
- **Import tags:** processors return read-only tags (`@texture:bc1`, `@mesh:12000tris`, `@object:2meshes`) stored in the manifest. Tag filters in the Navigator can use them like `@type:` and `@ext:`.
- **Rebuilds reach the runtime:** after a successful build the editor invalidates the asset's cached resources, so everything holding them reloads the new data without a restart.
- **Cleanup:** generated data of deleted assets is removed, including thumbnails.

## Runtime resources

`xg::ResourceSystem` (`xgBase/public/xgResources.h`) is one root subsystem shared by every module.

- **Types:** modules register resource types with `Load`, `Unload` and an optional `Finalize`. An `Async` type loads on a job worker and finalizes on the main thread (for GPU uploads); `Acquire` returns a handle in the `Pending` state, which becomes `Ready` or `Failed`.
- **Handles and lifetime:** handles are slot + generation, so stale handles are detected. References are counted per module; a resource with no references stays until the end-of-frame collect, so re-acquiring it in the same frame is free. Unloading a module releases its references and its types.
- **Sources:** `ReadFile(guid, extension)` searches directory sources for `<guid>.<ext>`, directly or one folder down. The editor adds `<project>/Library/Data`; a player build will add a package source instead.
- **Invalidation:** `Invalidate(file)` marks a file's resources stale; holders see `IsStale` and acquire again.
- **Jobs:** `xg::JobSystem` runs worker and main-thread jobs with dependencies; resource loads and asset builds use it.

## Textures

- PNG and BMP build to `.xdt`: a `TXHD` header (size, mips, usage, flags) and one `TXPX` chunk per hardware format.
- **Formats:** Uncompressed (RGBA8); Standard color without alpha (BC1), with alpha (BC3); luminance (BC4); normal (BC5).
- **Mips:** color mips are averaged in linear space, normal mips are renormalized, and BC textures are resized to multiples of 4.
- **Settings** live in the meta's `importer.process.texture`, and the texture inspector edits them with a live preview.
- **Runtime:** the `texture` resource type loads the first format the renderer supports and creates the GPU texture; color formats sample through sRGB views. Render views render in linear color through sRGB targets on both renderers.

## Meshes

- FBX files import through ufbx, converted to left-handed, Y up, meters. Embedded content and animation are skipped, and missing normals are generated. Setting: `import.scale`.
- **Items:** one `submesh` item per material part (path `<mesh>/<material>`) and one `hierarchy` item.
- **Files:**
  - raw `.xrv` (separate channel chunks) and `.xrh` (nodes)
  - processed `.xdv`: a 52-byte interleaved vertex (position, normal, tangent, uv0, color) with 16- or 32-bit indices
  - processed `.xdh`: the node table with world matrices
- **Runtime:** the `model` resource type creates one GPU mesh per part and places the parts by their nodes. `MeshRenderer` (a data component) draws one model in every render view, edit mode included, with a material per submesh: up to 8 submesh entries (submesh item GUID, material, hidden) and a fallback `Material` for the rest. None draws the default material; a hidden submesh isn't drawn.

## Shaders and materials

- **Shaders:**
  - Project `.hlsl` files follow the surface contract in `surface.common.hlsli`, which is installed with the SDK: engine constants, the lit vertex, `XGMaterial` (four `float4` params), four textures and a wrap sampler.
  - Annotations declare the material slots: `//@group`, `//@param name type [default] [notchtags=...] [range=min,max]`, `//@texture name usage`.
  - They compile with DXC for both renderers into `.xds` (bytecode per stage and backend, plus param and texture records).
- **Materials (`.xcm`):**
  - JSON with the embedded meta, `shader`, `params` (by name) and `textures` (by name), built to `.xdm`.
  - Params stay keyed by name; the runtime maps them to the shader's slots once the shader and textures are loaded.
  - The **Material Editor** shows a live preview (shape, background, auto-rotate), one card per annotation group (number rows with notch tags and ranges, colour pickers, texture pickers), and Save, Revert and Save as. It can also be mounted inside other editors.
- **Thumbnails:** materials render a sphere into `Library/Data/Materials/<guid>.thumb.png`, refreshed when the material, its shader or a texture rebuilds.

## Built-in assets

The engine ships its own assets, which work exactly like project assets: same importers, metas, GUIDs and resource types. Projects reference them by GUID.

| Folder (`gameroot/engine/assets/`) | Assets |
|---|---|
| `shaders/` | `surface.lit.hlsl` (Color, Albedo, Smoothness), `surface.unlit.hlsl` (Color, Albedo), `surface.error.hlsl` (magenta checker, internal) |
| `materials/` | `default.xcm` (lit, white), `unlit.xcm`, `error.xcm`: internal (see below) |
| `meshes/` | `cube.xcp`, `sphere.xcp` (size 1), `plane.xcp` (10 x 10), `quad.xcp` (1 x 1) |
| `textures/` | `white.png`, `black.png`, `normal.png` (flat), uncompressed |

- **Primitive meshes (`.xcp`):** a small JSON composition `{ "version": 1, "meta": {...}, "shape": "sphere", "size": 1, "segments": 48, "rings": 24 }` (cube, sphere, plane or quad). Like every `.xc*` file it carries its meta inside (no `.xmd`). It builds to the same `.xdv`/`.xdh` model data as an FBX, so `MeshRenderer` draws it unchanged, and editing the tags doesn't rebuild it. Projects can use `.xcp` files too.
- **Build:** `tools/xgEngineData` runs the asset pipeline over `gameroot/engine/assets` into `gameroot/engine/data` (lower-case type folders, untracked) as part of the solution build. It is skipped when nothing changed.
- **GUIDs are permanent:** the metas are committed, and projects keep pointing at those GUIDs. The build fails on an asset without a meta instead of making one up. Add a new built-in once with `dotnet tools/xgEngineData/bin/Debug/net10.0/xgEngineData.dll --new <path under assets>` and commit its meta.
- **Runtime:** the engine adds `gameroot/engine/data` as a resource source when it starts, so the editor, the hub, standalone runs and players all find built-ins.
- **Engine materials are internal:** `default`, `unlit` and `error`, and the error shader, are never listed in pickers, assigned or opened in an editor. The engine draws them itself:
  - a mesh or object part with no material (None) draws with `materials/default.xcm`;
  - a material whose data or shader fails to load draws with `materials/error.xcm`.

  Both GUIDs are also in `xgCore/EngineAssets.h`. When the engine data is missing, the renderer's white lit look stays as the last-resort fallback.
- **Editor:**
  - Asset pickers list built-ins in a Built-in section between None and the project's assets.
  - `xg.editor.EngineAssets` lists them (`DefaultMaterial`, `LitShader`, `Sphere`, ...), and the asset database resolves their `engine/...` paths.
  - New materials use the built-in lit shader when the project has none.
  - Built-ins are read-only: the Material Editor won't open the engine materials or save over a built-in, and its Edit Code button is disabled for built-in shaders.

## Archetypes (`.xco`)

An archetype is an authored tree of **Objects** joined by **Jointers**, drawn by one `ArchetypeDriver` on one actor.

- **Objects:**
  - **Mesh Objects** draw chosen submeshes of a model, each with its own material slot.
  - **Proxies** are named points.
  - Each Object has a fixed Jointer pose relative to its parent and can be exposed as a **Composition Actor** (a real actor with components) or a **Composition Proxy** (a named point scripts query with `TryGetProxy`).
- **Build:** `.xco` builds to `.xdo`:
  - nodes parents first, with poses baked relative to the root and to the nearest exposed actor
  - parts (model, submesh, material, and `hidden` when unticked: the part keeps its material but isn't built or drawn)
  - names, with hashes for fast proxy lookups
  - authored components
- **Runtime:** the native `ArchetypeDriveSystem` draws every instance: a part's world is model placement × baked node pose × actor world, recomputed per frame. Instances share the loaded object.
- **Composition Actors** are spawned under the driver with transforms held at the composition's pose, and get their authored components. The Hierarchy marks them with a lock.
- **The Archetype Editor** has:
  - tools, the Objects tree, and a live preview of the unsaved draft with proxy markers
  - Inspector-style tabs (Naming, Jointer, Mesh and material, Components)
  - a submesh/material table, and the Material Editor mounted for a part's material
  - authored components with their fields
  - "Archetype from model" fills a tree from an imported model.
- **Thumbnails** frame the drawn meshes, in `Library/Data/Archetypes/<guid>.thumb.png`.

## Scenes (`.xcs`)

Scenes save the actors set up in the editor, with components, field values and per-instance composition overrides. See [Scenes](xge-scenes.md).

## Editor tools

- **Navigator:** browses `Assets/` by folder, tags or search, with thumbnails, kind filters, bookmarks, Details view, rename and context menus. Meta files never show.
- **Inspectors** for textures (import settings), materials and objects (summaries with an Open button).
- **Scene > Assets** ribbon group and context menus: create material, shader, object, scene; "Archetype from model"; edit the selected material or object.
- **Background task panel** in the status bar: import progress with cancel.

## Code map

| Piece | Where |
|---|---|
| Metas, items, reconciliation, migration | `xgEditorKit.Managed/assets/` (`AssetMeta`, `AssetMetaStore`, `AssetItems`, `AssetReconciler`, `AssetMetaMigration`) |
| Pipeline, chunk writer | `xgEditorKit.Managed/assets/AssetPipeline.cs`, `ChunkFile.cs` |
| Importers | `TextureAssets.cs`, `MeshAssets.cs`, `PrimitiveAssets.cs`, `ShaderAssets.cs`, `MaterialAssets.cs`, `ArchetypeAssets.cs`, `SceneAssets.cs` (format only) |
| Built-in assets | `gameroot/engine/assets/`, `tools/xgEngineData/`, `xgEditorKit.Managed/assets/EngineAssets.cs`, `xgCore/EngineAssets.h` |
| Asset database (editor) | `xgEditor.CoreCLR/editor/project/AssetDatabase.cs` |
| Resource system, jobs | `xgBase/public/xgResources.h`, `xgJobs.h`; `xgCore/ResourceSystemImpl.*`, `JobSystemImpl.*` |
| Resource types | `xgCore/TextureResources.*`, `ModelResources.*`, `ShaderResources.*`, `MaterialResources.*`, `ArchetypeResources.*` |
| Drawing | `xgCore/MeshRenderSystemImpl.*`, `ArchetypeDriveSystemImpl.*` |
| Editors | `xgEditor.CoreCLR/editor/material/`, `editor/archetype/`, `editor/scene/` |

---

## Future design

Not implemented yet:

- **Animation:** `.xda` data, skinned meshes, bone attachment and animated Jointers, a Timeline in the Archetype Editor.
- **More compositions:** `.xcf` texture flipbooks and `.xca` texture atlases.
- **Physics** data in archetypes (the format reserves a key).
- **Player packaging:** packing processed data and compositions by GUID, never shipping raw data, with a GUID → path manifest, copying only the built-in assets a project references.
- **Built-in assets:** a sky shader for `CameraClear.Skybox`, thumbnails of built-in materials in pickers, and moving the renderers' test cube and grid to built-in assets.
- **Eviction** of unused resources under a memory budget.
- **Platform overrides** for texture settings.
- **Renderer instancing** and per-asset static batching of archetypes.
