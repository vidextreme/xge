// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Text;
using System.Text.Encodings.Web;
using System.Text.Json;

namespace xg.editor
{
    // JSON files written by the editor. The project default is pretty-printed: 2-space indent, one key per line, CRLF
    // and a trailing newline. Files are written only when their text changes, so untouched files keep their time stamp.
    public static class JSONFile
    {
        public static readonly JsonSerializerOptions Options = new()
        {
            WriteIndented = true,
            IndentSize = 2,
            NewLine = "\r\n",
            Encoder = JavaScriptEncoder.UnsafeRelaxedJsonEscaping,
            ReadCommentHandling = JsonCommentHandling.Skip,
            AllowTrailingCommas = true,
        };

        private static readonly UTF8Encoding _utf8 = new(false);

        public static string Serialize<T>(T value) => JsonSerializer.Serialize(value, Options) + "\r\n";

        public static T? Deserialize<T>(string text) => JsonSerializer.Deserialize<T>(text, Options);

        // Reads without blocking writers: another thread may replace or delete the file meanwhile (build manifests).
        public static T? Read<T>(string path) => Deserialize<T>(ReadShared(path));

        public static string ReadShared(string path)
        {
            using FileStream stream = new(path, FileMode.Open, FileAccess.Read, FileShare.ReadWrite | FileShare.Delete);
            using StreamReader reader = new(stream);
            return reader.ReadToEnd();
        }

        // Returns true when the file was written.
        public static bool Write<T>(string path, T value) => WriteText(path, Serialize(value));

        public static bool WriteText(string path, string text)
        {
            if (File.Exists(path) && File.ReadAllText(path) == text)
                return false;
            string? dir = Path.GetDirectoryName(path);
            if (!string.IsNullOrEmpty(dir))
                Directory.CreateDirectory(dir);
            File.WriteAllText(path, text, _utf8);
            return true;
        }
    }
}
