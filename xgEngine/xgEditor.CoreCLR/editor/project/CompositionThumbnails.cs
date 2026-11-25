// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.editor;
using xg.math;

namespace xgEditor.CoreCLR
{
    // Rendered thumbnails of compositions, stored as Library/Data/<data folder>/<guid>.thumb.png next to the data they
    // come from: materials as a sphere drawn with the built material, objects (.xco) as their saved tree (ObjectDrawList)
    // framed by its bounds. The pipeline's orphan sweep deletes them with their asset, and they're never in a build
    // manifest, so players don't ship them. A thumbnail is stale when it's missing or older than the composition's data
    // or the data it uses (shaders, textures, models, materials); stale ones are queued and rendered one at a time in a
    // hidden render view, then read back and written as PNG.
    public static unsafe class CompositionThumbnails
    {
        public const int Size = 128;
        public const string Suffix = ".thumb.png";
        private const int TimeoutFrames = 300;
        private const long CheckIntervalMilliseconds = 1000;

        private sealed class Job
        {
            public required string Guid;
            public ResourceHandle Handle;
            public int Frames;
            public int DrawnAt = -1;
            public ObjectAsset? Object;
            public ObjectDrawList? Draws;
        }

        private static readonly Queue<string> _queue = new();
        private static readonly HashSet<string> _queued = new(StringComparer.OrdinalIgnoreCase);
        private static readonly Dictionary<string, (long CheckedAt, DateTime Version)> _checks = new(StringComparer.OrdinalIgnoreCase);
        private static Job? _job;
        private static ulong _view;

        // Raised on the main thread after a thumbnail was written (asset GUID, PNG path).
        public static event Action<string, string>? Written;

        public static bool Supports(AssetEntry entry) =>
            (entry.Kind == AssetKind.Material && MaterialAsset.IsMaterial(entry.RelPath)) || (entry.Kind == AssetKind.Object && ObjectAsset.IsObject(entry.RelPath));

        // The thumbnail file of a composition, or null without a project.
        public static string? PathFor(string guid)
        {
            if (ProjectService.Assets is not { } assets)
                return null;
            string folder = assets.PathOf(guid) is string path && ObjectAsset.IsObject(path) ? "Objects" : "Materials";
            return Path.Combine(assets.Pipeline.Root, folder, guid.ToLowerInvariant() + Suffix);
        }

        // The thumbnail's write time when it's usable (MinValue when there is none yet). Queues a render when it's
        // missing or stale (checked at most once a second per asset); an old thumbnail stays shown meanwhile.
        public static DateTime Version(AssetEntry entry)
        {
            if (ProjectService.Assets?.GuidOf(entry.RelPath) is not string guid || PathFor(guid) is not string path)
                return DateTime.MinValue;
            long now = Environment.TickCount64;
            if (_checks.TryGetValue(guid, out var check) && now - check.CheckedAt < CheckIntervalMilliseconds)
                return check.Version;
            DateTime version = File.Exists(path) ? File.GetLastWriteTimeUtc(path) : DateTime.MinValue;
            if (Stale(entry, guid, version))
                Request(guid);
            _checks[guid] = (now, version);
            return version;
        }

        // Queues a render of the composition's thumbnail.
        public static void Request(string guid)
        {
            if (_queued.Add(guid))
                _queue.Enqueue(guid);
        }

        // Drops the cached staleness checks (a thumbnail was written or deleted outside this class).
        public static void Forget(string guid) => _checks.Remove(guid);

        // Advances the current render (call once per editor frame, before the frame renders).
        public static void Update()
        {
            if (SceneViews.HasRenderer() == 0 || ProjectService.Assets is null)
                return;
            if (_job is null)
            {
                if (!_queue.TryDequeue(out string? guid))
                    return;
                _queued.Remove(guid);
                if (ProjectService.Assets.PathOf(guid) is string objectPath && ObjectAsset.IsObject(objectPath))
                {
                    try
                    {
                        _job = new Job { Guid = guid, Object = ObjectAsset.Read(ProjectService.Assets.FullPath(objectPath)), Draws = new ObjectDrawList() };
                    }
                    catch (Exception e) when (e is IOException or System.Text.Json.JsonException or InvalidDataException)
                    {
                        Debug.LogWarning($"[Thumbnails] Could not read {objectPath}: {e.Message}");
                    }
                    return;
                }
                uint type = Resources.FindType(Resources.MaterialType);
                if (type == 0 || !ResourceID.TryParse(guid, out ResourceID id))
                    return;
                _job = new Job { Guid = guid, Handle = Resources.Acquire(type, id) };
            }

            Job job = _job;
            job.Frames++;
            if (job.Object is not null && job.Draws is not null)
            {
                UpdateObject(job);
                return;
            }
            ResourceState state = job.Handle.State;
            if (state is ResourceState.Failed or ResourceState.Invalid || job.Frames > TimeoutFrames)
            {
                Debug.LogWarning($"[Thumbnails] Could not render {ProjectService.Assets.PathOf(job.Guid) ?? job.Guid} ({(job.Frames > TimeoutFrames ? "timed out" : state.ToString())})");
                Finish(job);
                return;
            }
            if (job.DrawnAt < 0)
            {
                ulong material = state == ResourceState.Ready ? Resources.ResolveMaterial(job.Handle) : 0;
                if (material == 0 || !EnsureView())
                    return;
                SetCamera(new Vector3(0.0f, 0.0f, 0.0f), 3.4f, 0.05f, 50.0f);
                SceneViews.MaterialDraw draw = default;
                draw.Mesh = PreviewShapes.Get(PreviewShape.Sphere);
                draw.Material = material;
                for (int i = 0; i < 16; i++)
                    draw.World[i] = i % 5 == 0 ? 1.0f : 0.0f;
                SceneViews.SetMaterialDraws(_view, &draw, draw.Mesh != 0 ? 1u : 0u);
                job.DrawnAt = job.Frames;
                return;
            }
            if (job.Frames < job.DrawnAt + 2)
                return;
            Write(job);
            Finish(job);
        }

        // An object: waits until its models and materials are loaded, frames its bounds, draws, then reads back.
        private static void UpdateObject(Job job)
        {
            if (job.DrawnAt < 0)
            {
                job.Draws!.Build(job.Object!, false);
                if (!job.Draws.Complete && job.Frames <= TimeoutFrames)
                    return;
                if (job.Draws.Draws.Count == 0 || !EnsureView())
                {
                    if (job.Draws.Draws.Count == 0)
                        Debug.LogWarning($"[Thumbnails] {ProjectService.Assets?.PathOf(job.Guid) ?? job.Guid} draws nothing; it keeps its icon");
                    Finish(job);
                    return;
                }
                float radius = job.Draws.Radius;
                SetCamera(job.Draws.Center, radius / MathF.Sin(0.3f) * 1.05f, MathF.Max(radius * 0.01f, 0.01f), MathF.Max(radius * 40.0f, 50.0f));
                job.Draws.Submit(_view);
                job.DrawnAt = job.Frames;
                return;
            }
            if (job.Frames < job.DrawnAt + 2)
                return;
            Write(job);
            Finish(job);
        }

        private static bool Stale(AssetEntry entry, string guid, DateTime version)
        {
            if (ObjectAsset.IsObject(entry.RelPath))
                return StaleObject(entry, guid, version);
            AssetPipeline pipeline = ProjectService.Assets!.Pipeline;
            if (pipeline.FindFile(guid, MaterialAssets.ProcessedExtension) is not string data)
                return false;
            if (version == DateTime.MinValue)
                return true;
            DateTime newest = File.GetLastWriteTimeUtc(data);
            try
            {
                MaterialAsset material = MaterialAsset.Read(entry.FullPath);
                foreach (string reference in material.ReferencedGuids())
                {
                    string? file = pipeline.FindFile(reference, ShaderAssets.ProcessedExtension) ?? pipeline.FindFile(reference, TextureAssets.ProcessedExtension);
                    if (file is not null && File.GetLastWriteTimeUtc(file) > newest)
                        newest = File.GetLastWriteTimeUtc(file);
                }
            }
            catch (Exception e) when (e is IOException or System.Text.Json.JsonException or InvalidDataException)
            {
            }
            return newest > version;
        }

        // Newer than the thumbnail: the object's .xdo, or a model's .xdv or a material's .xdm it uses.
        private static bool StaleObject(AssetEntry entry, string guid, DateTime version)
        {
            AssetPipeline pipeline = ProjectService.Assets!.Pipeline;
            if (pipeline.FindFile(guid, ObjectAssets.ProcessedExtension) is not string data)
                return false;
            if (version == DateTime.MinValue)
                return true;
            DateTime newest = File.GetLastWriteTimeUtc(data);
            try
            {
                foreach (string reference in ObjectAsset.Read(entry.FullPath).ReferencedGuids())
                {
                    string? file = pipeline.FindFile(reference, MeshAssets.ProcessedExtension) ?? pipeline.FindFile(reference, MaterialAssets.ProcessedExtension);
                    if (file is not null && File.GetLastWriteTimeUtc(file) > newest)
                        newest = File.GetLastWriteTimeUtc(file);
                }
            }
            catch (Exception e) when (e is IOException or System.Text.Json.JsonException or InvalidDataException)
            {
            }
            return newest > version;
        }

        private static bool EnsureView()
        {
            if (_view != 0)
                return true;
            _view = SceneViews.Create(Size, Size);
            if (_view == 0)
                return false;
            SceneViews.SetFlags(_view, SceneViews.FlagHideTestScene | SceneViews.FlagHideSceneDraws);
            return true;
        }

        private static void SetCamera(Vector3 target, float distance, float near, float far)
        {
            const float yaw = 0.6f;
            const float pitch = 0.35f;
            SceneViews.Camera camera = default;
            camera.Eye = new Vector3(target.x + MathF.Sin(yaw) * MathF.Cos(pitch) * distance, target.y + MathF.Sin(pitch) * distance,
                target.z - MathF.Cos(yaw) * MathF.Cos(pitch) * distance);
            camera.Target = target;
            camera.Up = new Vector3(0.0f, 1.0f, 0.0f);
            camera.FovY = 0.6f;
            camera.NearZ = near;
            camera.FarZ = far;
            camera.ClearColor = new Color(0.0f, 0.0f, 0.0f, 0.0f);
            SceneViews.SetCamera(_view, &camera);
        }

        private static void Write(Job job)
        {
            if (PathFor(job.Guid) is not string path)
                return;
            byte[] pixels = new byte[Size * Size * 4];
            fixed (byte* p = pixels)
            {
                if (SceneViews.Read(_view, p, (ulong)pixels.Length) == 0)
                {
                    Debug.LogWarning($"[Thumbnails] Could not read back the thumbnail of {job.Guid}");
                    return;
                }
            }
            string temporary = path + ".tmp";
            try
            {
                Directory.CreateDirectory(Path.GetDirectoryName(path)!);
                if (!ImageFile.SavePNG(temporary, pixels, Size, Size))
                {
                    Debug.LogWarning($"[Thumbnails] Could not write {temporary}");
                    return;
                }
                File.Move(temporary, path, true);
            }
            catch (Exception e) when (e is IOException or UnauthorizedAccessException)
            {
                Debug.LogWarning($"[Thumbnails] Could not write {path}: {e.Message}");
                return;
            }
            _checks.Remove(job.Guid);
            int opaque = 0;
            for (int i = 3; i < pixels.Length; i += 4)
                if (pixels[i] != 0)
                    opaque++;
            Debug.Log($"[Thumbnails] Wrote {Path.GetFileName(path)} for {ProjectService.Assets?.PathOf(job.Guid) ?? job.Guid}: {Size}x{Size}, {opaque} drawn pixel(s)");
            Written?.Invoke(job.Guid, path);
        }

        private static void Finish(Job job)
        {
            if (_view != 0)
                SceneViews.SetMaterialDraws(_view, null, 0);
            if (job.Handle.Value != 0)
                job.Handle.Dispose();
            job.Draws?.Dispose();
            _job = null;
        }
    }
}
