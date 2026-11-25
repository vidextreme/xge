// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Buffers.Binary;
using System.Diagnostics;
using System.Globalization;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json.Nodes;
using System.Text.RegularExpressions;

namespace xg.editor
{
    public enum ShaderParamType
    {
        Float = 1,
        Float2 = 2,
        Float3 = 3,
        Float4 = 4,
        Color = 5,
    }

    public enum ShaderTextureUsage
    {
        Color = 0,
        Normal = 1,
        Luminance = 2,
    }

    // A //@param line: a float4 slot of the XGMaterial constant buffer. NotchTags and Min/Max come from the editor-only
    // options notchtags= and range= (colour params default to r,g,b,a over 0..1); they don't reach the built shader.
    public sealed record ShaderParam(string Name, ShaderParamType Type, float[] Default, string Group, int Slot, int Order,
        string? NotchTags = null, float? Min = null, float? Max = null)
    {
        public bool HasRange => Min is float min && Max is float max && max > min;

        public int Components => Type switch
        {
            ShaderParamType.Float => 1,
            ShaderParamType.Float2 => 2,
            ShaderParamType.Float3 => 3,
            _ => 4,
        };
    }

    // A //@texture line: one of the surface textures t0..t3.
    public sealed record ShaderTexture(string Name, ShaderTextureUsage Usage, string Group, int Slot, int Order);

    // The //@group, //@param and //@texture annotations of a surface shader (see surface.common.hlsli).
    public sealed class ShaderAnnotations
    {
        public const int MaxParams = 4;
        public const int MaxTextures = 4;
        public const int MaxNameBytes = 31;
        public const string DefaultGroup = "General";

        public List<ShaderParam> Params { get; } = new();
        public List<ShaderTexture> Textures { get; } = new();
        // Group names in first-use order.
        public List<string> Groups { get; } = new();
        public List<string> Errors { get; } = new();

        public static string DisplayName(string name) => name.Replace('_', ' ');

        public ShaderParam? FindParam(string name) => Params.FirstOrDefault(p => p.Name == name);
        public ShaderTexture? FindTexture(string name) => Textures.FirstOrDefault(t => t.Name == name);

        // The params and textures of a group, in declaration order.
        public IEnumerable<object> Entries(string group) =>
            Params.Where(p => p.Group == group).Cast<object>().Concat(Textures.Where(t => t.Group == group))
                .OrderBy(e => e is ShaderParam p ? p.Order : ((ShaderTexture)e).Order);

        private static readonly Regex _option = new(@"(?<=\s)([A-Za-z_]+)=(""[^""]*""|\S+)", RegexOptions.CultureInvariant);

        public static ShaderAnnotations Parse(string text)
        {
            ShaderAnnotations result = new();
            string group = DefaultGroup;
            int order = 0;
            int lineNumber = 0;
            foreach (string rawLine in text.Split('\n'))
            {
                lineNumber++;
                string line = rawLine.Trim();
                if (!line.StartsWith("//@", StringComparison.Ordinal))
                    continue;
                List<(string Key, string Value)> options = new();
                string body = _option.Replace(line[3..], m =>
                {
                    string value = m.Groups[2].Value;
                    if (value.Length >= 2 && value[0] == '"' && value[^1] == '"')
                        value = value[1..^1];
                    options.Add((m.Groups[1].Value, value));
                    return " ";
                });
                string[] parts = body.Split((char[]?)null, StringSplitOptions.RemoveEmptyEntries);
                if (parts.Length == 0)
                    continue;
                string keyword = parts[0];
                if (keyword == "group")
                {
                    group = parts.Length > 1 ? string.Join(" ", parts.Skip(1)) : DefaultGroup;
                    continue;
                }
                if (keyword != "param" && keyword != "texture")
                {
                    result.Errors.Add($"line {lineNumber}: unknown annotation //@{keyword}");
                    continue;
                }
                if (parts.Length < 3)
                {
                    result.Errors.Add($"line {lineNumber}: //@{keyword} needs a name and a type");
                    continue;
                }
                string name = parts[1];
                if (!ValidName(name))
                {
                    result.Errors.Add($"line {lineNumber}: '{name}' isn't a valid name (letters, digits and _, up to {MaxNameBytes} characters)");
                    continue;
                }
                if (result.FindParam(name) is not null || result.FindTexture(name) is not null)
                {
                    result.Errors.Add($"line {lineNumber}: '{name}' is declared twice");
                    continue;
                }

                if (keyword == "param")
                {
                    if (!TryParseType(parts[2], out ShaderParamType type))
                    {
                        result.Errors.Add($"line {lineNumber}: unknown param type '{parts[2]}'");
                        continue;
                    }
                    if (result.Params.Count >= MaxParams)
                    {
                        result.Errors.Add($"line {lineNumber}: more than {MaxParams} params");
                        continue;
                    }
                    float[] value = type == ShaderParamType.Color ? new[] { 1.0f, 1.0f, 1.0f, 1.0f } : new float[4];
                    if (parts.Length > 3 && !TryParseFloats(string.Join("", parts.Skip(3)), value))
                    {
                        result.Errors.Add($"line {lineNumber}: invalid default for '{name}'");
                        continue;
                    }
                    if (!TryParseOptions(options, type, lineNumber, result.Errors, out string? notchTags, out float? min, out float? max))
                        continue;
                    result.Params.Add(new ShaderParam(name, type, value, group, result.Params.Count, order++, notchTags, min, max));
                }
                else
                {
                    if (options.Count > 0)
                    {
                        result.Errors.Add($"line {lineNumber}: //@texture takes no options ({options[0].Key}=)");
                        continue;
                    }
                    if (!TryParseUsage(parts[2], out ShaderTextureUsage usage))
                    {
                        result.Errors.Add($"line {lineNumber}: unknown texture usage '{parts[2]}'");
                        continue;
                    }
                    if (result.Textures.Count >= MaxTextures)
                    {
                        result.Errors.Add($"line {lineNumber}: more than {MaxTextures} textures");
                        continue;
                    }
                    result.Textures.Add(new ShaderTexture(name, usage, group, result.Textures.Count, order++));
                }
                if (!result.Groups.Contains(group))
                    result.Groups.Add(group);
            }
            return result;
        }

        // notchtags=<tags> (one per component, see NotchTagsAttribute; quote it when it has spaces) and range=<min>,<max>.
        private static bool TryParseOptions(List<(string Key, string Value)> options, ShaderParamType type, int lineNumber, List<string> errors,
            out string? notchTags, out float? min, out float? max)
        {
            bool color = type == ShaderParamType.Color;
            notchTags = color ? "r,g,b,a" : null;
            min = color ? 0.0f : null;
            max = color ? 1.0f : null;
            int components = type switch { ShaderParamType.Float => 1, ShaderParamType.Float2 => 2, ShaderParamType.Float3 => 3, _ => 4 };
            foreach ((string key, string value) in options)
            {
                switch (key)
                {
                    case "notchtags":
                        if (value.Split(',').Length > components)
                        {
                            errors.Add($"line {lineNumber}: notchtags has more tags than the param's {components} component(s)");
                            return false;
                        }
                        notchTags = value.Trim().Length == 0 ? null : value;
                        break;
                    case "range":
                        string[] bounds = value.Split(',');
                        if (bounds.Length != 2
                            || !float.TryParse(bounds[0], NumberStyles.Float, CultureInfo.InvariantCulture, out float low)
                            || !float.TryParse(bounds[1], NumberStyles.Float, CultureInfo.InvariantCulture, out float high)
                            || high <= low)
                        {
                            errors.Add($"line {lineNumber}: range needs <min>,<max> with min < max");
                            return false;
                        }
                        min = low;
                        max = high;
                        break;
                    default:
                        errors.Add($"line {lineNumber}: unknown option '{key}=' (notchtags=, range=)");
                        return false;
                }
            }
            return true;
        }

        public static bool ValidName(string name) =>
            name.Length > 0 && Encoding.UTF8.GetByteCount(name) <= MaxNameBytes && name.All(c => char.IsAsciiLetterOrDigit(c) || c == '_');

        public static string TypeName(ShaderParamType type) => type.ToString().ToLowerInvariant();

        private static bool TryParseType(string text, out ShaderParamType type)
        {
            foreach (ShaderParamType candidate in Enum.GetValues<ShaderParamType>())
            {
                if (TypeName(candidate) == text)
                {
                    type = candidate;
                    return true;
                }
            }
            type = ShaderParamType.Float;
            return false;
        }

        private static bool TryParseUsage(string text, out ShaderTextureUsage usage)
        {
            foreach (ShaderTextureUsage candidate in Enum.GetValues<ShaderTextureUsage>())
            {
                if (candidate.ToString().ToLowerInvariant() == text)
                {
                    usage = candidate;
                    return true;
                }
            }
            usage = ShaderTextureUsage.Color;
            return false;
        }

        private static bool TryParseFloats(string text, float[] values)
        {
            string[] items = text.Split(',', StringSplitOptions.RemoveEmptyEntries);
            if (items.Length == 0 || items.Length > 4)
                return false;
            for (int i = 0; i < items.Length; i++)
            {
                if (!float.TryParse(items[i], NumberStyles.Float, CultureInfo.InvariantCulture, out values[i]))
                    return false;
            }
            return true;
        }
    }

    // Shaders: a project .hlsl becomes <guid>.xds, compiled by dxc for both renderers. The source stays in memory, so no
    // raw file is stored. One "shader" item holds:
    //   SHHD: layout version, param count, texture count (32-bit values)
    //   SHBC: one chunk per stage and backend; Extra = stage (0 VS, 1 PS) | backend << 8 (1 DXIL, 2 SPIR-V)
    //   SHPR: per param a 32-byte name, type, slot and the default float4
    //   SHTX: per texture a 32-byte name, usage and slot
    public static class ShaderAssets
    {
        public const string ItemKind = "shader";
        public const string RawExtension = ".xrs";
        public const string ProcessedExtension = ".xds";
        public const int LayoutVersion = 1;
        public const int StageVertex = 0;
        public const int StagePixel = 1;
        public const int BackendDXIL = 1;
        public const int BackendSPIRV = 2;
        public const int ParamRecordSize = 56;
        public const int TextureRecordSize = 40;

        public static readonly uint RawKind = ChunkTag.Make("XRS ");
        public static readonly uint ProcessedKind = ChunkTag.Make("XDS ");
        public static readonly uint SourceChunk = ChunkTag.Make("SRCT");
        public static readonly uint DirectoryChunk = ChunkTag.Make("SRCD");
        public static readonly uint HeaderChunk = ChunkTag.Make("SHHD");
        public static readonly uint BytecodeChunk = ChunkTag.Make("SHBC");
        public static readonly uint ParamsChunk = ChunkTag.Make("SHPR");
        public static readonly uint TexturesChunk = ChunkTag.Make("SHTX");

        // compiler: compiles one stage for one backend and returns the bytecode, or throws with dxc's message.
        public static void Register(AssetPipeline pipeline, IShaderCompiler compiler)
        {
            pipeline.Register(new Importer());
            pipeline.Register(new Processor(compiler));
        }

        public static bool IsShaderSource(string path) => Path.GetExtension(path).Equals(".hlsl", StringComparison.OrdinalIgnoreCase);

        public static byte[] PackParams(IReadOnlyList<ShaderParam> parameters)
        {
            byte[] bytes = new byte[parameters.Count * ParamRecordSize];
            for (int i = 0; i < parameters.Count; i++)
            {
                Span<byte> record = bytes.AsSpan(i * ParamRecordSize, ParamRecordSize);
                Encoding.UTF8.GetBytes(parameters[i].Name, record[..32]);
                BinaryPrimitives.WriteInt32LittleEndian(record[32..], (int)parameters[i].Type);
                BinaryPrimitives.WriteInt32LittleEndian(record[36..], parameters[i].Slot);
                for (int c = 0; c < 4; c++)
                    BinaryPrimitives.WriteSingleLittleEndian(record[(40 + c * 4)..], parameters[i].Default[c]);
            }
            return bytes;
        }

        public static byte[] PackTextures(IReadOnlyList<ShaderTexture> textures)
        {
            byte[] bytes = new byte[textures.Count * TextureRecordSize];
            for (int i = 0; i < textures.Count; i++)
            {
                Span<byte> record = bytes.AsSpan(i * TextureRecordSize, TextureRecordSize);
                Encoding.UTF8.GetBytes(textures[i].Name, record[..32]);
                BinaryPrimitives.WriteInt32LittleEndian(record[32..], (int)textures[i].Usage);
                BinaryPrimitives.WriteInt32LittleEndian(record[36..], textures[i].Slot);
            }
            return bytes;
        }

        private static byte[] Ints(params int[] values)
        {
            byte[] bytes = new byte[values.Length * 4];
            for (int i = 0; i < values.Length; i++)
                BinaryPrimitives.WriteInt32LittleEndian(bytes.AsSpan(i * 4), values[i]);
            return bytes;
        }

        private sealed class Importer : IAssetImporter
        {
            public string Id => "shader.hlsl";
            public int Version => 1;
            public string DataFolder => "Shaders";
            public string DataExtension => ProcessedExtension;

            public bool Accepts(string extension) => extension.Equals(".hlsl", StringComparison.OrdinalIgnoreCase);

            public IReadOnlyList<AssetDataFile> Import(string sourcePath, JsonObject settings)
            {
                string directory = Path.GetDirectoryName(Path.GetFullPath(sourcePath)) ?? string.Empty;
                AssetDataItem item = new(ItemKind, Path.GetFileNameWithoutExtension(sourcePath), new List<ChunkData>
                {
                    new(SourceChunk, File.ReadAllBytes(sourcePath)),
                    new(DirectoryChunk, Encoding.UTF8.GetBytes(directory + "|" + Path.GetFileName(sourcePath))),
                });
                return new[] { new AssetDataFile(RawExtension, RawKind, new List<AssetDataItem> { item }, Store: false) };
            }
        }

        private sealed class Processor : IAssetProcessor
        {
            private readonly IShaderCompiler _compiler;

            public Processor(IShaderCompiler compiler) => _compiler = compiler;

            public string Id => "shader.compile;" + _compiler.Key;
            public int Version => 1;

            public bool Accepts(string rawExtension) => rawExtension.Equals(RawExtension, StringComparison.OrdinalIgnoreCase);

            public AssetDataFile Process(AssetDataFile raw, JsonObject settings)
            {
                List<AssetDataItem> items = new();
                List<string> tags = new();
                foreach (AssetDataItem item in raw.Items)
                {
                    byte[] source = item.Chunks.First(c => c.Type == SourceChunk).Data;
                    string[] location = Encoding.UTF8.GetString(item.Chunks.First(c => c.Type == DirectoryChunk).Data).Split('|');
                    string text = Encoding.UTF8.GetString(source).TrimStart('﻿');
                    ShaderAnnotations annotations = ShaderAnnotations.Parse(text);
                    if (annotations.Errors.Count > 0)
                        throw new InvalidDataException($"{location[1]}: {string.Join("; ", annotations.Errors)}");

                    List<ChunkData> chunks = new()
                    {
                        new(HeaderChunk, Ints(LayoutVersion, annotations.Params.Count, annotations.Textures.Count)),
                    };
                    foreach (int backend in new[] { BackendDXIL, BackendSPIRV })
                    {
                        foreach (int stage in new[] { StageVertex, StagePixel })
                        {
                            byte[] code = _compiler.Compile(source, location[0], location[1], stage, backend);
                            chunks.Add(new ChunkData(BytecodeChunk, code, 0, (ulong)(stage | (backend << 8))));
                        }
                    }
                    chunks.Add(new ChunkData(ParamsChunk, PackParams(annotations.Params)));
                    chunks.Add(new ChunkData(TexturesChunk, PackTextures(annotations.Textures)));
                    items.Add(new AssetDataItem(item.Kind, item.Path, chunks));
                    tags.Add($"@shader:{annotations.Params.Count}params");
                    tags.Add($"@shader:{annotations.Textures.Count}textures");
                }
                return new AssetDataFile(ProcessedExtension, ProcessedKind, items, Tags: tags);
            }
        }
    }

    public interface IShaderCompiler
    {
        // Part of the processor's build key: changes when the compiler or the shared shader headers change.
        string Key { get; }
        byte[] Compile(byte[] source, string sourceDirectory, string fileName, int stage, int backend);
    }

    // Compiles with dxc (the SDK copy, XG_DXC, then dxc on PATH), with the same flags as the built-in shaders.
    public sealed class DxcShaderCompiler : IShaderCompiler
    {
        private const int TimeoutMilliseconds = 60000;
        private readonly string? _dxc;
        private readonly string _includeDirectory;

        public DxcShaderCompiler(string sdkDirectory)
        {
            _includeDirectory = Path.Combine(sdkDirectory, "shaders");
            _dxc = Find(sdkDirectory);
            Key = HeaderHash(_includeDirectory);
        }

        public string Key { get; }

        public static string? Find(string sdkDirectory)
        {
            string local = Path.Combine(sdkDirectory, "dxc", OperatingSystem.IsWindows() ? "dxc.exe" : "dxc");
            if (File.Exists(local))
                return local;
            string? configured = Environment.GetEnvironmentVariable("XG_DXC");
            if (!string.IsNullOrEmpty(configured) && File.Exists(configured))
                return configured;
            string name = OperatingSystem.IsWindows() ? "dxc.exe" : "dxc";
            foreach (string folder in (Environment.GetEnvironmentVariable("PATH") ?? string.Empty).Split(Path.PathSeparator, StringSplitOptions.RemoveEmptyEntries))
            {
                string candidate = Path.Combine(folder, name);
                if (File.Exists(candidate))
                    return candidate;
            }
            return null;
        }

        public byte[] Compile(byte[] source, string sourceDirectory, string fileName, int stage, int backend)
        {
            if (_dxc is null)
                throw new InvalidOperationException("No shader compiler: dxc wasn't found in the SDK, XG_DXC or PATH");
            string work = Path.Combine(Path.GetTempPath(), "xge-shaders", Guid.NewGuid().ToString("N"));
            Directory.CreateDirectory(work);
            try
            {
                string input = Path.Combine(work, fileName);
                string output = Path.Combine(work, "out.bin");
                File.WriteAllBytes(input, source);
                List<string> arguments = new()
                {
                    "-nologo", "-T", stage == ShaderAssets.StageVertex ? "vs_6_0" : "ps_6_0", "-E", stage == ShaderAssets.StageVertex ? "VSMain" : "PSMain",
                    "-O3", "-I", sourceDirectory, "-I", _includeDirectory,
                };
                if (backend == ShaderAssets.BackendSPIRV)
                    arguments.AddRange(new[] { "-spirv", "-fspv-target-env=vulkan1.3" });
                else
                    arguments.Add("-Wno-ignored-attributes");
                arguments.AddRange(new[] { "-Fo", output, input });

                ProcessStartInfo start = new(_dxc) { UseShellExecute = false, RedirectStandardError = true, RedirectStandardOutput = true, CreateNoWindow = true };
                foreach (string argument in arguments)
                    start.ArgumentList.Add(argument);
                using Process process = Process.Start(start) ?? throw new InvalidOperationException("Could not start dxc");
                Task<string> errors = process.StandardError.ReadToEndAsync();
                Task<string> messages = process.StandardOutput.ReadToEndAsync();
                if (!process.WaitForExit(TimeoutMilliseconds))
                {
                    process.Kill(entireProcessTree: true);
                    throw new TimeoutException($"dxc took longer than {TimeoutMilliseconds / 1000} s on {fileName}");
                }
                string message = (errors.Result + messages.Result).Replace(input, fileName).Trim();
                if (process.ExitCode != 0 || !File.Exists(output))
                    throw new InvalidDataException(string.IsNullOrEmpty(message) ? $"dxc failed on {fileName} (exit code {process.ExitCode})" : message);
                return File.ReadAllBytes(output);
            }
            finally
            {
                try { Directory.Delete(work, true); } catch (IOException) { } catch (UnauthorizedAccessException) { }
            }
        }

        private static string HeaderHash(string directory)
        {
            using IncrementalHash hash = IncrementalHash.CreateHash(HashAlgorithmName.SHA1);
            if (Directory.Exists(directory))
            {
                foreach (string file in Directory.GetFiles(directory, "*.hlsli").OrderBy(f => f, StringComparer.Ordinal))
                {
                    hash.AppendData(Encoding.UTF8.GetBytes(Path.GetFileName(file)));
                    hash.AppendData(File.ReadAllBytes(file));
                }
            }
            return Convert.ToHexString(hash.GetHashAndReset())[..12].ToLowerInvariant();
        }
    }
}
