// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Buffers.Binary;
using System.Globalization;
using System.Text;

namespace xg.editor
{
    public static class ChunkTag
    {
        public static readonly uint Item = Make("ITEM");

        public static uint Make(string name)
        {
            if (name.Length != 4 || name.Any(c => c > 0x7F))
                throw new ArgumentException($"A chunk tag has four ASCII characters: '{name}'", nameof(name));
            return (uint)(name[0] | (name[1] << 8) | (name[2] << 16) | (name[3] << 24));
        }

        public static string Name(uint tag) =>
            new(new[] { (char)(tag & 0xFF), (char)((tag >> 8) & 0xFF), (char)((tag >> 16) & 0xFF), (char)(tag >> 24) });
    }

    public sealed record ChunkData(uint Type, byte[] Data, uint Flags = 0, ulong Extra = 0);

    public sealed record ChunkItem(string Guid, List<ChunkData> Chunks);

    // The generated .xr* and .xd* file format, matching xgBase/public/xgChunkFile.h.
    public sealed class ChunkFile
    {
        public const int Version = 1;
        private const int FileHeaderSize = 32;
        private const int ChunkHeaderSize = 24;
        private const int GuidSize = 16;
        private static readonly uint _magic = ChunkTag.Make("XGCF");

        public uint Kind { get; init; }
        public string FileGuid { get; init; } = string.Empty;
        public List<ChunkData> Chunks { get; init; } = new();
        public List<ChunkItem> Items { get; init; } = new();

        public byte[] ToBytes()
        {
            using MemoryStream stream = new();
            using BinaryWriter writer = new(stream, Encoding.UTF8, true);
            writer.Write(_magic);
            writer.Write(Version);
            writer.Write(Kind);
            writer.Write(0u);
            WriteGuid(writer, FileGuid);
            foreach (ChunkData chunk in Chunks)
                WriteChunk(writer, chunk.Type, chunk.Flags, chunk.Extra, chunk.Data);
            foreach (ChunkItem item in Items)
            {
                using MemoryStream body = new();
                using BinaryWriter bodyWriter = new(body, Encoding.UTF8, true);
                WriteGuid(bodyWriter, item.Guid);
                foreach (ChunkData chunk in item.Chunks)
                    WriteChunk(bodyWriter, chunk.Type, chunk.Flags, chunk.Extra, chunk.Data);
                bodyWriter.Flush();
                WriteChunk(writer, ChunkTag.Item, 0, 0, body.ToArray());
            }
            writer.Flush();
            return stream.ToArray();
        }

        // Throws InvalidDataException for anything that isn't a complete file of a supported version.
        public static ChunkFile Parse(ReadOnlySpan<byte> data)
        {
            if (data.Length < FileHeaderSize)
                throw new InvalidDataException("The chunk file is too small");
            if (BinaryPrimitives.ReadUInt32LittleEndian(data) != _magic)
                throw new InvalidDataException("Not a chunk file");
            uint version = BinaryPrimitives.ReadUInt32LittleEndian(data[4..]);
            if (version == 0 || version > Version)
                throw new InvalidDataException($"Unsupported chunk file version {version}");

            ChunkFile file = new() { Kind = BinaryPrimitives.ReadUInt32LittleEndian(data[8..]), FileGuid = ReadGuid(data[16..]) };
            HashSet<string> guids = new(StringComparer.Ordinal);
            int cursor = FileHeaderSize;
            while (cursor < data.Length)
            {
                ChunkData chunk = ReadChunk(data, ref cursor);
                if (chunk.Type != ChunkTag.Item)
                {
                    file.Chunks.Add(chunk);
                    continue;
                }
                if (chunk.Data.Length < GuidSize)
                    throw new InvalidDataException("An item chunk has no GUID");
                ReadOnlySpan<byte> body = chunk.Data;
                string guid = ReadGuid(body);
                if (!AssetItemTable.IsValidChunkGuid(guid) || !guids.Add(guid))
                    throw new InvalidDataException($"Invalid or repeated item GUID {guid}");
                ChunkItem item = new(guid, new List<ChunkData>());
                int inner = GuidSize;
                while (inner < body.Length)
                {
                    ChunkData child = ReadChunk(body, ref inner);
                    if (child.Type == ChunkTag.Item)
                        throw new InvalidDataException("Items can't be nested");
                    item.Chunks.Add(child);
                }
                file.Items.Add(item);
            }
            return file;
        }

        private static void WriteChunk(BinaryWriter writer, uint type, uint flags, ulong extra, byte[] data)
        {
            writer.Write(type);
            writer.Write(flags);
            writer.Write((ulong)data.Length);
            writer.Write(extra);
            writer.Write(data);
            for (int i = data.Length; i % 8 != 0; i++)
                writer.Write((byte)0);
        }

        private static ChunkData ReadChunk(ReadOnlySpan<byte> data, ref int cursor)
        {
            if (data.Length - cursor < ChunkHeaderSize)
                throw new InvalidDataException("A chunk header is truncated");
            ReadOnlySpan<byte> header = data[cursor..];
            uint type = BinaryPrimitives.ReadUInt32LittleEndian(header);
            uint flags = BinaryPrimitives.ReadUInt32LittleEndian(header[4..]);
            ulong size = BinaryPrimitives.ReadUInt64LittleEndian(header[8..]);
            ulong extra = BinaryPrimitives.ReadUInt64LittleEndian(header[16..]);
            int payload = cursor + ChunkHeaderSize;
            int available = data.Length - payload;
            if (size > (ulong)available)
                throw new InvalidDataException($"Chunk {ChunkTag.Name(type)} is truncated");
            int length = (int)size;
            int padding = (8 - length % 8) % 8;
            if (padding > available - length)
                throw new InvalidDataException($"Chunk {ChunkTag.Name(type)} is missing its padding");
            cursor = payload + length + padding;
            return new ChunkData(type, data.Slice(payload, length).ToArray(), flags, extra);
        }

        private static void WriteGuid(BinaryWriter writer, string guid)
        {
            if (!AssetMeta.IsValidGuid(guid))
                throw new InvalidDataException($"Invalid GUID '{guid}'");
            writer.Write(ulong.Parse(guid.AsSpan(0, 16), NumberStyles.HexNumber, CultureInfo.InvariantCulture));
            writer.Write(ulong.Parse(guid.AsSpan(16, 16), NumberStyles.HexNumber, CultureInfo.InvariantCulture));
        }

        private static string ReadGuid(ReadOnlySpan<byte> data) =>
            $"{BinaryPrimitives.ReadUInt64LittleEndian(data):x16}{BinaryPrimitives.ReadUInt64LittleEndian(data[8..]):x16}";
    }
}
