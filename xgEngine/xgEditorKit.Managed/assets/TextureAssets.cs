// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Buffers.Binary;
using System.Text.Json.Nodes;

namespace xg.editor
{
    public sealed record DecodedImage(int Width, int Height, byte[] Pixels);

    // Textures: a source image becomes <name>.xdt. The decoded pixels stay in memory, so no .xrt file is stored.
    //   TXHD: width, height, mip count, texture usage, flags and layout version as 32-bit values
    //   TXPX: one chunk per hardware format, its Extra is the TextureHardwareFormat; mips are stored largest first
    // Import setting: flipY. Process settings: see TextureSettings.
    public static class TextureAssets
    {
        public const string ItemKind = "texture";
        public const string RawExtension = ".xrt";
        public const string ProcessedExtension = ".xdt";
        public const int LayoutVersion = 2;
        public const int FlagSRGB = 1;
        public const int FlagAlpha = 2;

        public static readonly uint RawKind = ChunkTag.Make("XRT ");
        public static readonly uint ProcessedKind = ChunkTag.Make("XDT ");
        public static readonly uint HeaderChunk = ChunkTag.Make("TXHD");
        public static readonly uint PixelsChunk = ChunkTag.Make("TXPX");

        public static void Register(AssetPipeline pipeline, Func<string, DecodedImage?> decode, IReadOnlyList<ITextureEncoder>? encoders = null)
        {
            pipeline.Register(new Importer(decode));
            pipeline.Register(new Processor(encoders ?? TextureEncoders.Default));
        }

        private static bool Flag(JsonObject settings, string name) =>
            settings[name] is JsonValue value && value.TryGetValue(out bool enabled) && enabled;

        private static ChunkData Find(AssetDataItem item, uint type) =>
            item.Chunks.FirstOrDefault(c => c.Type == type) ?? throw new InvalidDataException($"Texture data has no {ChunkTag.Name(type)} chunk");

        private static byte[] Ints(params int[] values)
        {
            byte[] bytes = new byte[values.Length * 4];
            for (int i = 0; i < values.Length; i++)
                BinaryPrimitives.WriteInt32LittleEndian(bytes.AsSpan(i * 4), values[i]);
            return bytes;
        }

        private sealed class Importer : IAssetImporter
        {
            private readonly Func<string, DecodedImage?> _decode;

            public Importer(Func<string, DecodedImage?> decode) => _decode = decode;

            public string Id => "texture.image";
            public int Version => 1;
            public string DataFolder => "Textures";
            public string DataExtension => ProcessedExtension;

            public bool Accepts(string extension) =>
                extension.Equals(".png", StringComparison.OrdinalIgnoreCase) || extension.Equals(".bmp", StringComparison.OrdinalIgnoreCase);

            public void WriteDefaultSettings(JsonObject import, JsonObject process) => new TextureSettings().WriteTo(process);

            public IReadOnlyList<AssetDataFile> Import(string sourcePath, JsonObject settings)
            {
                DecodedImage image = _decode(sourcePath) ?? throw new InvalidDataException($"Could not decode {Path.GetFileName(sourcePath)}");
                long expected = (long)image.Width * image.Height * 4;
                if (image.Width <= 0 || image.Height <= 0 || image.Pixels.Length != expected)
                    throw new InvalidDataException($"{Path.GetFileName(sourcePath)} decoded to an invalid image");
                byte[] pixels = Flag(settings, "flipY") ? FlipY(image) : image.Pixels;
                AssetDataItem item = new(ItemKind, string.Empty, new List<ChunkData>
                {
                    new(HeaderChunk, Ints(image.Width, image.Height)),
                    new(PixelsChunk, pixels),
                });
                return new[] { new AssetDataFile(RawExtension, RawKind, new List<AssetDataItem> { item }, Store: false) };
            }

            private static byte[] FlipY(DecodedImage image)
            {
                int stride = image.Width * 4;
                byte[] flipped = new byte[image.Pixels.Length];
                for (int y = 0; y < image.Height; y++)
                    Buffer.BlockCopy(image.Pixels, y * stride, flipped, (image.Height - 1 - y) * stride, stride);
                return flipped;
            }
        }

        private sealed class Processor : IAssetProcessor
        {
            private static readonly float[] _toLinear = BuildToLinear();
            private static readonly byte[] _toSRGB = BuildToSRGB();

            private readonly Dictionary<TextureHardwareFormat, ITextureEncoder> _encoders;

            public Processor(IReadOnlyList<ITextureEncoder> encoders)
            {
                _encoders = encoders.ToDictionary(e => e.Format);
                Id = "texture.process;" + string.Join(",", encoders.OrderBy(e => e.Format).Select(e => $"{e.Format}@{e.Version}"));
            }

            public string Id { get; }
            public int Version => 4;

            public bool Accepts(string rawExtension) => rawExtension.Equals(RawExtension, StringComparison.OrdinalIgnoreCase);

            public AssetDataFile Process(AssetDataFile raw, JsonObject settings)
            {
                TextureSettings texture = TextureSettings.From(settings);
                List<AssetDataItem> items = new();
                List<string> tags = new();
                foreach (AssetDataItem item in raw.Items)
                {
                    ReadOnlySpan<byte> header = Find(item, HeaderChunk).Data;
                    int width = BinaryPrimitives.ReadInt32LittleEndian(header);
                    int height = BinaryPrimitives.ReadInt32LittleEndian(header[4..]);
                    byte[] source = Find(item, PixelsChunk).Data;
                    TextureBuildPlan plan = TextureFormats.Plan(texture, width, height, TextureFormats.HasAlpha(source));
                    tags.AddRange(TextureFormats.ImportTags(texture, plan));
                    if (!_encoders.TryGetValue(plan.Format, out ITextureEncoder? encoder))
                        throw new InvalidDataException($"No encoder for {plan.Format}");

                    float[] level = Resize(ToWorking(source, texture), width, height, plan.Width, plan.Height);
                    Normalize(level, texture.Type);
                    using MemoryStream pixels = new();
                    int w = plan.Width, h = plan.Height;
                    for (int mip = 0; mip < plan.MipCount; mip++)
                    {
                        byte[] encoded = encoder.Encode(ToBytes(level, texture.Type, plan.KeepAlpha), w, h, texture.Quality);
                        pixels.Write(encoded);
                        if (mip + 1 == plan.MipCount)
                            break;
                        int nextWidth = Math.Max(1, w / 2);
                        int nextHeight = Math.Max(1, h / 2);
                        level = Resize(level, w, h, nextWidth, nextHeight);
                        Normalize(level, texture.Type);
                        w = nextWidth;
                        h = nextHeight;
                    }

                    int flags = (plan.IsSRGB ? FlagSRGB : 0) | (plan.KeepAlpha ? FlagAlpha : 0);
                    items.Add(new AssetDataItem(item.Kind, item.Path, new List<ChunkData>
                    {
                        new(HeaderChunk, Ints(plan.Width, plan.Height, plan.MipCount, (int)texture.Type, flags, LayoutVersion)),
                        new(PixelsChunk, pixels.ToArray(), 0, (ulong)plan.Format),
                    }));
                }
                return new AssetDataFile(ProcessedExtension, ProcessedKind, items, Tags: tags.ToList());
            }

            private static float[] ToWorking(byte[] rgba, TextureSettings settings)
            {
                float[] result = new float[rgba.Length];
                for (int i = 0; i < rgba.Length; i += 4)
                {
                    switch (settings.Type)
                    {
                        case TextureUsage.Normal:
                            result[i] = rgba[i] / 255.0f * 2.0f - 1.0f;
                            result[i + 1] = (rgba[i + 1] / 255.0f * 2.0f - 1.0f) * (settings.FlipGreen ? -1.0f : 1.0f);
                            result[i + 2] = rgba[i + 2] / 255.0f * 2.0f - 1.0f;
                            result[i + 3] = 1.0f;
                            break;
                        case TextureUsage.Luminance:
                            float luma = 0.2126f * _toLinear[rgba[i]] + 0.7152f * _toLinear[rgba[i + 1]] + 0.0722f * _toLinear[rgba[i + 2]];
                            result[i] = result[i + 1] = result[i + 2] = luma;
                            result[i + 3] = 1.0f;
                            break;
                        default:
                            result[i] = _toLinear[rgba[i]];
                            result[i + 1] = _toLinear[rgba[i + 1]];
                            result[i + 2] = _toLinear[rgba[i + 2]];
                            result[i + 3] = rgba[i + 3] / 255.0f;
                            break;
                    }
                }
                return result;
            }

            private static byte[] ToBytes(float[] level, TextureUsage usage, bool keepAlpha)
            {
                byte[] result = new byte[level.Length];
                for (int i = 0; i < level.Length; i += 4)
                {
                    for (int c = 0; c < 3; c++)
                    {
                        float value = level[i + c];
                        result[i + c] = usage switch
                        {
                            TextureUsage.Normal => Unorm(value * 0.5f + 0.5f),
                            TextureUsage.Luminance => Unorm(value),
                            _ => _toSRGB[(int)(Math.Clamp(value, 0.0f, 1.0f) * (_toSRGB.Length - 1) + 0.5f)],
                        };
                    }
                    result[i + 3] = keepAlpha ? Unorm(level[i + 3]) : (byte)255;
                }
                return result;
            }

            private static byte Unorm(float value) => (byte)(Math.Clamp(value, 0.0f, 1.0f) * 255.0f + 0.5f);

            private static void Normalize(float[] level, TextureUsage usage)
            {
                if (usage != TextureUsage.Normal)
                    return;
                for (int i = 0; i < level.Length; i += 4)
                {
                    float length = MathF.Sqrt(level[i] * level[i] + level[i + 1] * level[i + 1] + level[i + 2] * level[i + 2]);
                    if (length < 1e-5f)
                    {
                        level[i] = level[i + 1] = 0.0f;
                        level[i + 2] = 1.0f;
                        continue;
                    }
                    level[i] /= length;
                    level[i + 1] /= length;
                    level[i + 2] /= length;
                }
            }

            private static float[] Resize(float[] source, int width, int height, int newWidth, int newHeight)
            {
                if (width == newWidth && height == newHeight)
                    return source;
                float[] result = new float[newWidth * newHeight * 4];
                float scaleX = (float)width / newWidth;
                float scaleY = (float)height / newHeight;
                for (int y = 0; y < newHeight; y++)
                {
                    float top = y * scaleY;
                    float bottom = (y + 1) * scaleY;
                    int firstY = (int)top;
                    int lastY = Math.Min(height - 1, (int)MathF.Ceiling(bottom) - 1);
                    for (int x = 0; x < newWidth; x++)
                    {
                        float left = x * scaleX;
                        float right = (x + 1) * scaleX;
                        int firstX = (int)left;
                        int lastX = Math.Min(width - 1, (int)MathF.Ceiling(right) - 1);
                        float r = 0.0f, g = 0.0f, b = 0.0f, a = 0.0f, total = 0.0f;
                        for (int sy = firstY; sy <= lastY; sy++)
                        {
                            float weightY = MathF.Min(bottom, sy + 1) - MathF.Max(top, sy);
                            for (int sx = firstX; sx <= lastX; sx++)
                            {
                                float weight = weightY * (MathF.Min(right, sx + 1) - MathF.Max(left, sx));
                                int index = (sy * width + sx) * 4;
                                r += source[index] * weight;
                                g += source[index + 1] * weight;
                                b += source[index + 2] * weight;
                                a += source[index + 3] * weight;
                                total += weight;
                            }
                        }
                        int target = (y * newWidth + x) * 4;
                        result[target] = r / total;
                        result[target + 1] = g / total;
                        result[target + 2] = b / total;
                        result[target + 3] = a / total;
                    }
                }
                return result;
            }

            private static float[] BuildToLinear()
            {
                float[] table = new float[256];
                for (int i = 0; i < table.Length; i++)
                {
                    float value = i / 255.0f;
                    table[i] = value <= 0.04045f ? value / 12.92f : MathF.Pow((value + 0.055f) / 1.055f, 2.4f);
                }
                return table;
            }

            private static byte[] BuildToSRGB()
            {
                byte[] table = new byte[4096];
                for (int i = 0; i < table.Length; i++)
                {
                    float value = i / (float)(table.Length - 1);
                    float encoded = value <= 0.0031308f ? value * 12.92f : 1.055f * MathF.Pow(value, 1.0f / 2.4f) - 0.055f;
                    table[i] = Unorm(encoded);
                }
                return table;
            }
        }
    }
}
