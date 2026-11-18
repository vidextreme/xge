// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Buffers.Binary;
using System.Text.Json.Nodes;

namespace xg.editor
{
    public sealed record DecodedImage(int Width, int Height, byte[] Pixels);

    // Textures: a source image becomes <name>.xdt. The decoded pixels stay in memory, so no .xrt file is stored.
    //   TXHD: width, height, format (1 = RGBA8) and mip count as 32-bit values
    //   TXPX: tightly packed pixels, largest mip first
    // Import setting: flipY (bool). Process setting: mipmaps (bool, box-filtered chain down to 1x1).
    public static class TextureAssets
    {
        public const string ItemKind = "texture";
        public const string RawExtension = ".xrt";
        public const string ProcessedExtension = ".xdt";
        public const int FormatRGBA8 = 1;

        public static readonly uint RawKind = ChunkTag.Make("XRT ");
        public static readonly uint ProcessedKind = ChunkTag.Make("XDT ");
        public static readonly uint HeaderChunk = ChunkTag.Make("TXHD");
        public static readonly uint PixelsChunk = ChunkTag.Make("TXPX");

        public static void Register(AssetPipeline pipeline, Func<string, DecodedImage?> decode)
        {
            pipeline.Register(new Importer(decode));
            pipeline.Register(new Processor());
        }

        public static byte[] Header(int width, int height, int mipCount)
        {
            byte[] header = new byte[16];
            BinaryPrimitives.WriteInt32LittleEndian(header, width);
            BinaryPrimitives.WriteInt32LittleEndian(header.AsSpan(4), height);
            BinaryPrimitives.WriteInt32LittleEndian(header.AsSpan(8), FormatRGBA8);
            BinaryPrimitives.WriteInt32LittleEndian(header.AsSpan(12), mipCount);
            return header;
        }

        private static bool Flag(JsonObject settings, string name) =>
            settings[name] is JsonValue value && value.TryGetValue(out bool enabled) && enabled;

        private static ChunkData Find(AssetDataItem item, uint type) =>
            item.Chunks.FirstOrDefault(c => c.Type == type) ?? throw new InvalidDataException($"Texture data has no {ChunkTag.Name(type)} chunk");

        private sealed class Importer : IAssetImporter
        {
            private readonly Func<string, DecodedImage?> _decode;

            public Importer(Func<string, DecodedImage?> decode) => _decode = decode;

            public string Id => "texture.image";
            public int Version => 1;

            public bool Accepts(string extension) =>
                extension.Equals(".png", StringComparison.OrdinalIgnoreCase) || extension.Equals(".bmp", StringComparison.OrdinalIgnoreCase);

            public IReadOnlyList<AssetDataFile> Import(string sourcePath, JsonObject settings)
            {
                DecodedImage image = _decode(sourcePath) ?? throw new InvalidDataException($"Could not decode {Path.GetFileName(sourcePath)}");
                long expected = (long)image.Width * image.Height * 4;
                if (image.Width <= 0 || image.Height <= 0 || image.Pixels.Length != expected)
                    throw new InvalidDataException($"{Path.GetFileName(sourcePath)} decoded to an invalid image");
                byte[] pixels = Flag(settings, "flipY") ? FlipY(image) : image.Pixels;
                AssetDataItem item = new(ItemKind, string.Empty, new List<ChunkData>
                {
                    new(HeaderChunk, Header(image.Width, image.Height, 1)),
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
            public string Id => "texture.rgba8";
            public int Version => 1;

            public bool Accepts(string rawExtension) => rawExtension.Equals(RawExtension, StringComparison.OrdinalIgnoreCase);

            public AssetDataFile Process(AssetDataFile raw, JsonObject settings)
            {
                List<AssetDataItem> items = new();
                foreach (AssetDataItem item in raw.Items)
                {
                    ReadOnlySpan<byte> header = Find(item, HeaderChunk).Data;
                    int width = BinaryPrimitives.ReadInt32LittleEndian(header);
                    int height = BinaryPrimitives.ReadInt32LittleEndian(header[4..]);
                    byte[] pixels = Find(item, PixelsChunk).Data;
                    int mipCount = 1;
                    if (Flag(settings, "mipmaps"))
                        pixels = BuildMips(pixels, width, height, out mipCount);
                    items.Add(new AssetDataItem(item.Kind, item.Path, new List<ChunkData>
                    {
                        new(HeaderChunk, Header(width, height, mipCount)),
                        new(PixelsChunk, pixels),
                    }));
                }
                return new AssetDataFile(ProcessedExtension, ProcessedKind, items);
            }

            private static byte[] BuildMips(byte[] level, int width, int height, out int mipCount)
            {
                List<byte[]> levels = new() { level };
                while (width > 1 || height > 1)
                {
                    int nextWidth = Math.Max(1, width / 2);
                    int nextHeight = Math.Max(1, height / 2);
                    byte[] next = new byte[nextWidth * nextHeight * 4];
                    for (int y = 0; y < nextHeight; y++)
                    {
                        int y0 = y * height / nextHeight;
                        int y1 = Math.Min(height - 1, y0 + (height > 1 ? 1 : 0));
                        for (int x = 0; x < nextWidth; x++)
                        {
                            int x0 = x * width / nextWidth;
                            int x1 = Math.Min(width - 1, x0 + (width > 1 ? 1 : 0));
                            for (int c = 0; c < 4; c++)
                            {
                                int sum = level[(y0 * width + x0) * 4 + c] + level[(y0 * width + x1) * 4 + c]
                                    + level[(y1 * width + x0) * 4 + c] + level[(y1 * width + x1) * 4 + c];
                                next[(y * nextWidth + x) * 4 + c] = (byte)((sum + 2) / 4);
                            }
                        }
                    }
                    levels.Add(next);
                    level = next;
                    width = nextWidth;
                    height = nextHeight;
                }
                mipCount = levels.Count;
                byte[] all = new byte[levels.Sum(l => l.Length)];
                int offset = 0;
                foreach (byte[] mip in levels)
                {
                    Buffer.BlockCopy(mip, 0, all, offset, mip.Length);
                    offset += mip.Length;
                }
                return all;
            }
        }
    }
}
