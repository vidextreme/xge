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
| Raw | `dog.fbx.xrv`, `dog.fbx.xrh` | `Library/` | Importers | No | Import result, with vertex data in separate chunks |
| Processed | `.xdv`, `.xdh`, `.xdt`, `.xda`, `.xds` | `Library/` | Processing step | No | Runtime-ready data, with vertex data interleaved by stride |

## Flow

**Decided.**

```text
dog.fbx + dog.fbx.xmd (import settings)
        |
        v
Library: dog.fbx.xrv, dog.fbx.xrh      raw
        |
        v
Library: dog.fbx.xdv, dog.fbx.xdh      processed
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

**Proposed.**

* The text form is `<file-guid>/<chunk-guid>`. `<file-guid>` alone addresses the whole file.
* The source's `.xmd` holds the authoritative item table: data kind + hierarchical name path -> chunk GUID. This is what lets chunk GUIDs survive deleting `Library/`.
* The `.xmd` of an `.xc*` keeps its own copy of the names it uses.
* Duplicate names within one source are told apart by occurrence order.
* An item that disappears from the source keeps its table entry, so it recovers its GUID if it returns.
* A renamed item gets a new GUID, and the editor offers to relink existing references through the name tree.

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

**Proposed.**

* Keep the old chunk header fields: type bits, flags, size and extra.
* Add what the old format lacked: a file header with magic and format version.
* Use 64-bit sizes. The old `DWORD` size limits a chunk to 4 GB.
* Item chunks carry their GUID, and typed data chunks nest inside them.
* `.xr*` files store separate channel tags. `.xd*` files store interleaved vertex data under its own tag.

## Shaders

**Decided.**

* Compiled shader binaries are the processed data, stored as `.xds`.
* `.xcm` materials reference `.xds` shaders and `.xdt` textures.

**Proposed.**

* `water.hlsl` and its `.xmd` compile directly to `water.hlsl.xds`, with no raw stage.
* Each shader entry point is an item chunk with a kept GUID. Its compiled forms are typed data chunks inside it, such as `DXIL` and `SPRV`, so one file serves both DX12 and Vulkan.
* Project shaders compile with the DXC vendored in `3rdparty/dxc`.
* The engine's built-in shaders in `xgShaders` stay compiled into the renderer DLLs.

## Library and rebuilds

**Proposed.**

* `Library/` is addressed by GUID: `Library/Data/<guid prefix>/<guid>/dog.fbx.xdv`. The file name is for people only, and nothing links through it. Renames don't move data, and same-named sources don't collide.
* Raw output is keyed by source hash + import settings + importer version.
* Processed output is keyed by raw hash + process settings + processor version.
* The `importer` block of `.xmd` is split into `import` and `process` sections.
* `.xrt` is skipped at first. PNG decoding is cheap and raw pixels are large, so textures can go from source to `.xdt`. The pipeline must allow a missing raw stage.

## Open questions

* **Physics:** `.xco` includes physics, but no data type holds it.
* **Composition format:** proposed as pretty-printed JSON, for readable diffs and merges.
* **Player builds:** proposed to package `.xd*` and composition data by GUID and never ship `.xr*`.
* **Threading:** undecided. It must be settled before runtime texture loading.

## Implementation tiers

| Tier | Goal | Main work | Done when |
|---|---|---|---|
| 0. Design | Lock the data model | This document | Reviewed |
| 1. Runtime core | Share and own resources safely | `ResourceGUID`, `ResourceID`, type registry, cache, handles, module cleanup, C ABI, managed API | CoreCLR and NativeAOT share one loaded test resource, with no leaks on Windows or Linux |
| 2. Metadata | Make identity durable | `.xgm` -> `.xmd` with project migration, import and process settings, item table | Reimport gives every item its original GUID |
| 3. Chunk container | Define the binary format | File header, chunk bits, item chunks, nested typed data | Round-trip tests pass, and bad or old files are rejected cleanly |
| 4. Library pipeline | Generate data from assets | GUID-addressed `Library/`, importer and processor interfaces, rebuild keys, `cat.png` -> `.xdt` | Deleting `Library/` rebuilds identical references |
| 5. Textures at runtime | Reach the GPU | Source layer under resource loading, texture API in DX12 and Vulkan, threading model | A `.xdt` loads by breadcrumb and displays on both renderers |
| 6. Meshes | Bring in models | Importer module, `.xrv`/`.xrh` -> `.xdv`/`.xdh`, mesh GPU API | An imported mesh draws in the Scene view |
| 7. Compositions | Let users author content | `.xds` shaders, `.xcm`, `.xco`, their editors, name-tree repair, dependency loading, drawer component | A saved `.xco` survives a `Library/` rebuild and renders |
| 8. Later | Complete the system | `.xda`, `.xcf`, `.xca`, hot reload, eviction, player packaging | Planned separately |

Gates:

* Threading must be decided before Tier 5, and ideally before Tier 4 is finished.
* The physics data type must be decided before Tier 7.
* Inspector references need scene or component saving, which must land before Tier 7 is finished.
