// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.ComponentModel;
using System.Runtime.InteropServices;
using System.Text.Json.Nodes;

namespace xg.editor
{
    // Stored in .xdt files: the values must not change.
    public enum TextureHardwareFormat
    {
        RGBA8 = 1,
        BC1 = 2,
        BC3 = 3,
        BC4 = 4,
        BC5 = 5,
    }

    // The [Description] of each setting value is its label in the editor.
    public enum TextureCompression
    {
        [Description("Standard (BC)")] Standard,
        [Description("Uncompressed")] Uncompressed,
    }

    public enum TextureUsage
    {
        [Description("Color")] Color,
        [Description("Normal")] Normal,
        [Description("Luminance")] Luminance,
    }

    public enum TextureAlpha
    {
        [Description("Auto")] Auto,
        [Description("No alpha")] None,
        [Description("Keep alpha")] Keep,
    }

    public enum TextureQuality
    {
        [Description("Normal")] Normal,
        [Description("High")] High,
    }

    public enum TextureMipFilter
    {
        [Description("None")] None,
        [Description("Box (default)")] Box,
    }

    public enum TextureScale
    {
        [Description("No scaling")] None,
        [Description("Half")] Half,
        [Description("Quarter")] Quarter,
    }

    // The settings in importer.process.texture of a texture's .xmd.
    public sealed record TextureSettings
    {
        private const string Section = "texture";

        public TextureCompression Format { get; init; } = TextureCompression.Standard;
        public TextureUsage Type { get; init; } = TextureUsage.Color;
        public TextureAlpha Alpha { get; init; } = TextureAlpha.Auto;
        public TextureQuality Quality { get; init; } = TextureQuality.Normal;
        public TextureMipFilter MipFilter { get; init; } = TextureMipFilter.Box;
        public TextureScale Scale { get; init; } = TextureScale.None;
        public bool FlipGreen { get; init; }

        public static TextureSettings From(JsonObject process)
        {
            JsonObject values = process[Section] as JsonObject ?? new JsonObject();
            TextureMipFilter mipDefault = process["mipmaps"] is JsonValue legacy && legacy.TryGetValue(out bool mipmaps) && !mipmaps
                ? TextureMipFilter.None
                : TextureMipFilter.Box;
            return new TextureSettings
            {
                Format = Read(values, "format", TextureCompression.Standard),
                Type = Read(values, "type", TextureUsage.Color),
                Alpha = Read(values, "alpha", TextureAlpha.Auto),
                Quality = Read(values, "quality", TextureQuality.Normal),
                MipFilter = Read(values, "mipFilter", mipDefault),
                Scale = Read(values, "scale", TextureScale.None),
                FlipGreen = values["flipGreen"] is JsonValue flip && flip.TryGetValue(out bool flipped) && flipped,
            };
        }

        // Stores the settings in a process section, keeping its other keys, and drops the old mipmaps flag.
        public void WriteTo(JsonObject process)
        {
            process.Remove("mipmaps");
            process[Section] = new JsonObject
            {
                ["format"] = Name(Format),
                ["type"] = Name(Type),
                ["alpha"] = Name(Alpha),
                ["quality"] = Name(Quality),
                ["mipFilter"] = Name(MipFilter),
                ["scale"] = Name(Scale),
                ["flipGreen"] = FlipGreen,
            };
        }

        // Applies "key=value" pairs separated by commas, using the .xmd key names (test hooks).
        public TextureSettings WithText(string text)
        {
            JsonObject process = new();
            WriteTo(process);
            JsonObject values = (JsonObject)process[Section]!;
            foreach (string pair in text.Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries))
            {
                string[] parts = pair.Split('=', 2, StringSplitOptions.TrimEntries);
                if (parts.Length != 2)
                    continue;
                values[parts[0]] = parts[0] == "flipGreen" ? bool.Parse(parts[1]) : parts[1];
            }
            return From(process);
        }

        private static T Read<T>(JsonObject values, string key, T fallback) where T : struct, Enum =>
            values[key] is JsonValue value && value.TryGetValue(out string? text) && Enum.TryParse(text, true, out T parsed) && Enum.IsDefined(parsed) ? parsed : fallback;

        private static string Name<T>(T value) where T : struct, Enum => value.ToString().ToLowerInvariant() switch
        {
            "mipfilter" => "mipFilter",
            string name => name,
        };
    }

    // What a texture will be built into.
    public sealed record TextureBuildPlan(TextureHardwareFormat Format, int Width, int Height, int MipCount, long ByteSize, bool KeepAlpha, bool IsSRGB);

    public static class TextureFormats
    {
        public static TextureHardwareFormat Resolve(TextureSettings settings, bool sourceHasAlpha)
        {
            if (settings.Format == TextureCompression.Uncompressed)
                return TextureHardwareFormat.RGBA8;
            return settings.Type switch
            {
                TextureUsage.Normal => TextureHardwareFormat.BC5,
                TextureUsage.Luminance => TextureHardwareFormat.BC4,
                _ => KeepsAlpha(settings, sourceHasAlpha) ? TextureHardwareFormat.BC3 : TextureHardwareFormat.BC1,
            };
        }

        public static bool KeepsAlpha(TextureSettings settings, bool sourceHasAlpha) =>
            settings.Type == TextureUsage.Color && (settings.Alpha == TextureAlpha.Keep || (settings.Alpha == TextureAlpha.Auto && sourceHasAlpha));

        public static int BlockSize(TextureHardwareFormat format) => format == TextureHardwareFormat.RGBA8 ? 1 : 4;

        public static int BytesPerBlock(TextureHardwareFormat format) => format switch
        {
            TextureHardwareFormat.RGBA8 => 4,
            TextureHardwareFormat.BC1 or TextureHardwareFormat.BC4 => 8,
            _ => 16,
        };

        public static long LevelSize(TextureHardwareFormat format, int width, int height)
        {
            int block = BlockSize(format);
            return (long)((width + block - 1) / block) * ((height + block - 1) / block) * BytesPerBlock(format);
        }

        public const string ImportTagPrefix = "@texture:";

        // Read-only import tags of a built texture: usage, hardware format, output size and "alpha" when alpha is kept.
        public static IReadOnlyList<string> ImportTags(TextureSettings settings, TextureBuildPlan plan)
        {
            List<string> tags = new()
            {
                ImportTagPrefix + settings.Type.ToString().ToLowerInvariant(),
                ImportTagPrefix + plan.Format.ToString().ToLowerInvariant(),
                ImportTagPrefix + plan.Width + "x" + plan.Height,
            };
            if (plan.KeepAlpha)
                tags.Add(ImportTagPrefix + "alpha");
            return tags;
        }

        public static TextureBuildPlan Plan(TextureSettings settings, int sourceWidth, int sourceHeight, bool sourceHasAlpha)
        {
            TextureHardwareFormat format = Resolve(settings, sourceHasAlpha);
            int divisor = settings.Scale switch { TextureScale.Half => 2, TextureScale.Quarter => 4, _ => 1 };
            int width = Math.Max(1, sourceWidth / divisor);
            int height = Math.Max(1, sourceHeight / divisor);
            if (BlockSize(format) > 1)
            {
                width = RoundToBlock(width);
                height = RoundToBlock(height);
            }
            int mips = 1;
            long bytes = LevelSize(format, width, height);
            if (settings.MipFilter == TextureMipFilter.Box)
            {
                int w = width, h = height;
                while (w > 1 || h > 1)
                {
                    w = Math.Max(1, w / 2);
                    h = Math.Max(1, h / 2);
                    bytes += LevelSize(format, w, h);
                    mips++;
                }
            }
            return new TextureBuildPlan(format, width, height, mips, bytes, KeepsAlpha(settings, sourceHasAlpha) || (format == TextureHardwareFormat.RGBA8 && settings.Type == TextureUsage.Color && settings.Alpha == TextureAlpha.Keep),
                settings.Type == TextureUsage.Color);
        }

        public static bool HasAlpha(ReadOnlySpan<byte> rgba)
        {
            for (int i = 3; i < rgba.Length; i += 4)
                if (rgba[i] != 255)
                    return true;
            return false;
        }

        private static int RoundToBlock(int value) => Math.Max(4, (value + 2) / 4 * 4);
    }

    // Encodes one tightly packed RGBA8 mip level into a hardware format.
    public interface ITextureEncoder
    {
        TextureHardwareFormat Format { get; }
        int Version { get; }
        byte[] Encode(byte[] rgba, int width, int height, TextureQuality quality);
    }

    public static partial class TextureEncoders
    {
        public static IReadOnlyList<ITextureEncoder> Default { get; } = new ITextureEncoder[]
        {
            new Uncompressed(),
            new Block(TextureHardwareFormat.BC1),
            new Block(TextureHardwareFormat.BC3),
            new Block(TextureHardwareFormat.BC4),
            new Block(TextureHardwareFormat.BC5),
        };

        private sealed class Uncompressed : ITextureEncoder
        {
            public TextureHardwareFormat Format => TextureHardwareFormat.RGBA8;
            public int Version => 1;
            public byte[] Encode(byte[] rgba, int width, int height, TextureQuality quality) => rgba;
        }

        private sealed unsafe partial class Block : ITextureEncoder
        {
            public Block(TextureHardwareFormat format) => Format = format;

            public TextureHardwareFormat Format { get; }
            public int Version => 1;

            public byte[] Encode(byte[] rgba, int width, int height, TextureQuality quality)
            {
                byte[] output = new byte[TextureFormats.LevelSize(Format, width, height)];
                fixed (byte* source = rgba)
                fixed (byte* target = output)
                {
                    if (XG_EditorEncodeBlocks((int)Format, (int)quality, source, width, height, target, output.Length) != output.Length)
                        throw new InvalidDataException($"The {Format} encoder rejected a {width} x {height} image");
                }
                return output;
            }

            [LibraryImport("xgEditorKernel", EntryPoint = "XG_EditorEncodeBlocks")]
            private static partial long XG_EditorEncodeBlocks(int format, int quality, byte* rgba, int width, int height, byte* output, long capacity);
        }
    }

    // Texture settings being edited: Saved is what the .xmd holds, Draft what the Inspector shows.
    public sealed class TextureImportDraft
    {
        public TextureImportDraft(TextureSettings saved)
        {
            Saved = saved;
            Draft = saved;
        }

        public TextureSettings Saved { get; private set; }
        public TextureSettings Draft { get; set; }
        public bool IsDirty => Saved != Draft;

        public void Revert() => Draft = Saved;

        // Call after the draft was written to the .xmd.
        public void MarkApplied() => Saved = Draft;
    }
}
