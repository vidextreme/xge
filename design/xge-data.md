## Extension Names

* .xmd - meta data (similar to unity's .meta, we are changing .xgm to .xmd)
* .xc<> - composition: includes parameters, pure refences to *.xd<>, these are authored by users on their respective editors within xgEditor, staged in Assets
    * .xco - object (mesh + materials + hierarchy + animation + physics), a renderer/drawer component uses these
    * .xcm - material (shaders + textures)
    * .xcf - texture flipbook
    * .xca - texture atlas
* .xd<> - processed data
    * .xdv - mesh (vertex data are in stride, ie. pos + uv + normal + tangent + binormal)
    * .xdh - hierarchy (+skin)
    * .xdt - texture
    * .xda - animation
    * .xds - shader (compiled binaries)
* .xr<> - raw data: imported from FBX, obj, or external support formats
    * .xrv - raw mesh+submeshes (vertex data are in separated chunks)
    * .xrh - raw hierarchy
    etc...

## How it woks

* Import settings from .xmd will be used to generate .xr<>. 
* It will use the same name as the source files (dog.fbx, cat.png)
* They will become dog.fbx.xrv, cat.png.xrt(I am still thinking if it is benefitial to store this raw).
* All the .xd<> and .xr<> are unstaged in Library.
* References mean GUID references (file GUID and chunk GUID).
* .xc<> will have their own editor, users will be able to connect things via names (GUID are visible, but names are better for workflow)
* Name to guid tree is stored in .xmd of .xc<>, user can delete Library and able to reimport/reconstruct everything

---

# Expanded design

The two sections above are the original notes and remain the authority. This expansion records how they are understood.

Status labels:

* **Decided**: stated in the notes or confirmed during design review.
* **Proposed**: a suggested answer, not agreed yet.
* **Open**: needs a decision before the tier that depends on it.

## Data layers

**Decided.**

| Layer | Examples | Location | Produced by | In git | Role |
|---|---|---|---|---|---|
| Source | `dog.fbx`, `cat.png` | `Assets/` | External tools | Yes | Original input |
| Metadata | `dog.fbx.xmd`, `dog.xco.xmd` | Next to each asset | Editor | Yes | GUID, tags, import settings, name tables |
| Composition | `.xco`, `.xcm`, `.xcf`, `.xca` | `Assets/` | Editors inside xgEditor | Yes | Parameters and references to `.xd*` data |
| Raw | `<guid>.xrv`, `<guid>.xrh` | `Library/` | Importers | No | Import result, with vertex data in separate chunks |
| Processed | `.xdv`, `.xdh`, `.xdt`, `.xda`, `.xds` | `Library/` | Processing step | No | Runtime-ready data, with vertex data interleaved by stride |

## Flow

**Decided.**

```text
dog.fbx + dog.fbx.xmd (import settings)
        |
        v
Library: <guid>.xrv, <guid>.xrh        raw
        |
        v
Library: <guid>.xdv, <guid>.xdh        processed
        ^
        | <file-guid>/<chunk-guid> references
        |
Assets: dog.xco, dog.xcm               authored composition
        |
        v
renderer/drawer component
```

`Library/` is disposable. Deleting it and reimporting must reconstruct everything from what is in `Assets/`, including the `.xmd` files, without breaking a reference.

## References

**Decided.**

* References to `.xd*` data are breadcrumbs: `<file-guid>` then `<chunk-guid>`.
* Resolution finds the file first, then the chunk inside it. A chunk GUID is looked up only within its file.
* The file GUID is the source asset's `.xmd` GUID. Data generated from `dog.fbx` uses the GUID in `dog.fbx.xmd`.
* Chunk GUIDs are generated at random the first time an item is imported, then kept. They are not derived from names or hashes.
* All links between files use GUIDs only.
* Internal names, such as a submesh name, are the key for reconstruction. On reimport, an item matched by name gets its original GUID back.
* GUIDs are authoritative. Users connect things by name in the `.xc*` editors, and GUIDs stay visible.
* The name-to-GUID tree in the `.xmd` of an `.xc*` provides readable labels, and repairs a reference whose GUID no longer resolves.

* The text form is `<file-guid>/<chunk-guid>`. `<file-guid>` alone addresses the whole file.
* The source's `.xmd` holds the authoritative item table: data kind + hierarchical name path -> chunk GUID. This is what lets chunk GUIDs survive deleting `Library/`.
* The `.xmd` of an `.xc*` keeps its own copy of the names it uses.
* An item that disappears from the source keeps its table entry, marked missing, so it recovers its GUID if it returns.
* A renamed item gets a new GUID, and the editor offers to relink existing references through the name tree.
* Duplicate names within one source produce an import warning. Occurrence order is only the fallback that lets the import finish.

The `.xmd` format:

```json
{
  "version": 2,
  "guid": "<file-guid>",
  "tags": [],
  "importer": {
    "import": {},
    "process": {}
  },
  "items": [
    { "kind": "mesh", "path": "Root/Body", "guid": "<chunk-guid>" },
    { "kind": "mesh", "path": "Root/Old", "guid": "<chunk-guid>", "missing": true }
  ]
}
```

## Chunks

Inspired by the chunk format of the old XGE (`XGEngine/XGFormat.h`).

**Decided.**

* Chunk **bits** are a 32-bit four-character type tag, built like the old `XGCHUNK('P', 'V', 'E', 'C')`. They say what the data is.
* The chunk **GUID** says which item it is, such as one submesh. References use the GUID, and the bits allow type checks.

```text
dog.fbx.xrv                 file GUID = GUID in dog.fbx.xmd
  chunk 7f3a... "Body"      submesh item
    PVEC                    positions
    NVEC                    normals
    IVEC                    indices

reference: <file-guid>/7f3a...
```

**File format (version 1).** All values are little-endian.

| Part | Layout |
|---|---|
| File header, 32 bytes | magic `XGCF`, version, file kind bits, flags, file GUID |
| Chunk header, 24 bytes | type bits, flags, 64-bit size, 64-bit extra |
| Chunk | header, `size` payload bytes, padding to the next 8-byte boundary |
| Item chunk | type `ITEM`, payload is a 16-byte GUID followed by typed data chunks |

* Readers reject a bad magic, a newer version, truncated data, zero or repeated item GUIDs, and nested items.
* `.xr*` files store separate channel tags. `.xd*` files store interleaved vertex data under its own tag.

## Shaders

**Decided** (Tier 7).

* Compiled shader binaries are the processed data, stored as `.xds`.
* `.xcm` materials reference `.xds` shaders and `.xdt` textures.
* A project `.hlsl` compiles straight to `Library/Data/Shaders/<guid>.xds`, with no stored raw stage. `.hlsli` files are include-only and aren't tracked as dependencies yet.
* Shaders follow the surface contract in `surface.common.hlsli` (installed to the SDK's `shaders/` folder): the engine's 128-byte constants at `b0` (push constants on Vulkan), the 52-byte lit vertex, `XGMaterial` (`float4 Params[4]`) at `b1`, textures `t0`-`t3` and a wrapping sampler `s0`, with `VSMain`/`PSMain` entry points. File names follow `<category>.<feature>.hlsl` (`surface.tinted.hlsl`).
* Annotations name the slots: `//@param <name> <float|float2|float3|float4|color> [default]`, `//@texture <name> <color|normal|luminance>` (in declaration order, at most four of each), and `//@group <name>` for the editor's cards.
* One `shader` item holds `SHHD` (counts), one `SHBC` per stage and backend (`Extra` = stage, 0 VS or 1 PS, `| backend << 8`, 1 DXIL or 2 SPIR-V), `SHPR` (56-byte param records: name, type, slot, default) and `SHTX` (40-byte texture records). Import tags: `@shader:<n>params`, `@shader:<n>textures`.
* Project shaders compile with the DXC copied from `3rdparty/dxc` into the SDK (or `XG_DXC`, or `dxc` on PATH), with the built-in shaders' flags. A compile error fails the build with DXC's message and keeps the last good `.xds`. The build key includes a hash of the SDK's shader headers.
* The engine's built-in shaders in `xgShaders` stay compiled into the renderer DLLs.
* **Runtime:** the `shader` resource type loads `<guid>.xds` by the asset GUID alone, keeps the renderer's bytecode (`Renderer::GetShaderFormat`) and creates the pipeline (`Renderer::CreateShader`).

## Materials

**Decided** (Tier 7).

* An `.xcm` is pretty-printed JSON: `version`, `shader` (asset GUID), `params` (name -> up to four floats) and `textures` (name -> `<file-guid>` or `<file-guid>/<chunk-guid>`). Its `.xmd` keeps a name tree, `names` (GUID -> asset path), used to repair a reference whose GUID no longer resolves.
* It builds to `Library/Data/Materials/<guid>.xdm`: one `material` item with `MTHD` (counts and the shader GUID), `MTPR` (48-byte records: name, float4) and `MTTX` (64-byte records: name, resource ID).
* Params and textures stay keyed by name, so a material never depends on its shader at build time. At runtime the `material` resource type maps the names to the shader's slots (defaults for the rest) once the shader and textures it names are loaded, and creates the renderer material.
* **Drawing:** `MeshRenderer` components write `{ model GUID, material GUID, flags }` to the engine's `xg.MeshRenderer` data pool. The native mesh render system acquires the resources, places the model's parts with the actor's transform and hands the list to the renderer's scene draws, which every render view draws.
* **Rebuilds:** after a successful build the editor invalidates the asset's cached resources (`ResourceSystem::Invalidate`), so holders such as the mesh render system and materials load the new data without a restart.

## Library and rebuilds

**Decided.**

* `Library/` is addressed by GUID, with one folder per data type and no per-asset folders: `Library/Data/Textures/<guid>.xdt`, `Library/Data/Models/<guid>.xdv`. Renames don't move data, and same-named sources don't collide.
* One manifest per asset sits next to its data, named after the main generated file: `<guid>.xdt.json`. It records the source name, build keys, generated files and read-only import tags. It is written last, so an interrupted build runs again.
* Processors return read-only import tags with their output, such as `@texture:color`. They live only in the manifest, never in `.xmd`, so they are rebuilt with `Library/`. The editor shows them before an asset's user tags on request, and tag filters can use them like `@type:` and `@ext:`.
* A new source file gets a `.xmd` with its importer's default settings and is built once it stops being written.
* Raw output is keyed by source hash + import settings + importer version.
* Processed output is keyed by raw hash + process settings + processor versions.
* A failed build keeps the last good output and isn't retried until its inputs change.
* Generated data of deleted assets is removed.
* `.xrt` isn't stored. Textures keep decoded pixels in memory between importing and processing, and go from source to `.xdt`.
* Builds run as job system jobs: the worker does the import and processing, the main thread reads and writes the `.xmd` and applies the result. Builds report their stage and progress and can be cancelled between stages, which keeps the last good output.

**Texture data.** `.xdt` has one `texture` item with:

* `TXHD`: width, height, mip count, usage, flags (sRGB, alpha) and layout version as 32-bit values.
* `TXPX`: one chunk per hardware format, with the format ID in `Extra`. Mips are stored largest first.

Import tags: `@texture:<usage>` (color, normal, luminance), `@texture:<format>` (bc1, bc3, bc4, bc5, rgba8), `@texture:<width>x<height>` of the output, and `@texture:alpha` when alpha is kept.

Settings live in `importer.process.texture`: `format`, `type`, `alpha`, `quality`, `mipFilter`, `scale` and `flipGreen`. `importer.process.platforms.<name>` is reserved for platform overrides. `import.flipY` applies while decoding.

| Settings | Hardware format |
|---|---|
| Uncompressed | RGBA8 |
| Standard, color, no alpha | BC1 |
| Standard, color, alpha | BC3 |
| Standard, luminance | BC4 |
| Standard, normal | BC5 |

Color mips are averaged in linear space, and normal mips are renormalized. BC textures are resized to multiples of 4. At runtime, color BC1/BC3/RGBA8 are sampled through sRGB views, and normal Z is rebuilt in shaders.

## Runtime loading

**Decided** (Tier 5).

* **Threading:** an engine job system (`xg::JobSystem`, `xgBase/public/xgJobs.h`). Worker threads run jobs once their dependencies are done; main-thread jobs run from a per-frame pump (8 ms budget) or a main-thread `Wait`. The engine waits for a module's jobs before unloading it.
* **Async resources:** a type flagged `Async` runs `Load` on a worker (file reads, parsing; no GPU), then `Finalize` as a main-thread continuation (GPU upload). `Acquire` returns `Pending`; the resource becomes `Ready` or `Failed` in a later frame, or at once through `ResourceSystem::Wait`. The resource system's calls stay main thread only.
* **Sources:** `ResourceSystem::ReadFile(file-guid, extension)` searches directory sources for `<guid>.<extension>`, directly or one folder down. The editor adds `<project>/Library/Data`; a player build will add a package source instead.
* **Textures:** the engine registers the `texture` type when a renderer exists. `<asset-guid>/<texture-item-guid>` loads the item's `TXHD` and the first `TXPX` the renderer supports; the data is a renderer texture id (also a UI texture id) with size, format and usage. DX12 always supports BC; Vulkan needs `textureCompressionBC`.
* **Render views** render in linear color through an sRGB target, so sampled sRGB textures and blending are correct; their test cube shows a texture (`SetRenderViewTexture`).

## Meshes

**Decided** (Tier 6).

* **Importer:** FBX through ufbx (`3rdparty/ufbx`, compiled only into the editor kernel). Files are converted to the engine's space: left-handed, Y up, meters, mirrored on Z. Embedded content and animation are skipped, and missing normals are generated. Import setting: `import.scale`.
* **Items:** one `submesh` item per material part of a mesh, path `<mesh>/<material>` (an unnamed mesh takes its first node's name, an unnamed material is `material<index>`), plus one `hierarchy` item.
* `.xrv` (raw, stored): `MSHD` (mesh index, material index, counts, channel flags, bounds) and one chunk per channel: `PVEC`, `NVEC`, `TVEC`, `UVC0`, `CVEC`, `IVEC`.
* `.xrh` (raw, stored): `NODE` (parent, mesh index, name, local and geometry matrices) and `NAME`.
* `.xdv`: `VHDR` plus `VTXS`, a 52-byte interleaved vertex (position, normal, tangent with handedness in w, uv0, RGBA8 colour; missing channels defaulted) and `IX16` or `IX32`.
* `.xdh`: the node table with world matrices.
* Matrices are row-major for row vectors. Parts and nodes link by mesh index, and a part is drawn once per node that uses its mesh.
* Import tags: `@mesh:<N>tris`, `@mesh:<N>parts`, `@mesh:normals`, `@mesh:uv`, `@mesh:colors`.
* **Runtime:** the `model` resource type loads `<guid>.xdv` and `<guid>.xdh` by the asset GUID alone, creates one GPU mesh per part and exposes the placed parts.

## Open questions

* **Physics:** `.xco` includes physics, but no data type holds it.
* **Composition format:** proposed as pretty-printed JSON, for readable diffs and merges.
* **Player builds:** proposed to package `.xd*` and composition data by GUID and never ship `.xr*`.

## Implementation tiers

| Tier | Goal | Main work | Done when |
|---|---|---|---|
| 0. Design | Lock the data model | This document | Reviewed |
| 1. Runtime core | Share and own resources safely | `ResourceGUID`, `ResourceID`, type registry, cache, handles, module cleanup, C ABI, managed API | CoreCLR and NativeAOT share one loaded test resource, with no leaks on Windows or Linux |
| 2. Metadata | Make identity durable | `.xgm` -> `.xmd` with project migration, import and process settings, item table | Reimport gives every item its original GUID |
| 3. Chunk container | Define the binary format | File header, chunk bits, item chunks, nested typed data | Round-trip tests pass, and bad or old files are rejected cleanly |
| 4. Library pipeline | Generate data from assets | GUID-addressed `Library/`, importer and processor interfaces, rebuild keys, `cat.png` -> `.xdt` | Deleting `Library/` rebuilds identical references |
| 5. Textures at runtime | Reach the GPU | Source layer under resource loading, texture API in DX12 and Vulkan, threading model | A `.xdt` loads by breadcrumb and displays on both renderers (done: job system, async loads, test cube) |
| 6. Meshes | Bring in models | Importer module, `.xrv`/`.xrh` -> `.xdv`/`.xdh`, mesh GPU API | An imported mesh draws in the Scene view (done: FBX via ufbx, static meshes and hierarchy) |
| 7. Compositions | Let users author content | `.xds` shaders, `.xcm`, `.xco`, their editors, name-tree repair, dependency loading, drawer component | A saved `.xco` survives a `Library/` rebuild and renders (in progress: shaders, materials, Material Editor, MeshRenderer; `.xco` next) |
| 8. Later | Complete the system | `.xda`, `.xcf`, `.xca`, hot reload, eviction, player packaging | Planned separately |

Gates:

* The physics data type must be decided before Tier 7.
* Inspector references need scene or component saving, which must land before Tier 7 is finished.
