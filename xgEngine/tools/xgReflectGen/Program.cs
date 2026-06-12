using System;
using System.IO;
using System.Collections.Generic;
using System.Text;
using TreeSitter;

class Program
{
    [System.Runtime.InteropServices.DllImport("tree-sitter-cpp")]
    public static extern IntPtr tree_sitter_cpp();

    static bool IsGeneratedHeader(string path)
    {
        string file = Path.GetFileName(path);
        return file.Contains(".generated.", StringComparison.OrdinalIgnoreCase);
    }

    static void Main(string[] args)
    {
        Console.WriteLine("=== XG Reflection Generator ===");

        if (args.Length < 2)
        {
            Console.WriteLine("Usage: xgReflectGen <DEST_FOLDER> <ROOT_FOLDER>");
            return;
        }

        string destFolder = Path.GetFullPath(args[0]);
        string rootFolder = Path.GetFullPath(args[1]);

        Directory.CreateDirectory(destFolder);

        if (!Directory.Exists(rootFolder))
        {
            Console.WriteLine("ERROR: Root folder does not exist: " + rootFolder);
            return;
        }

        var langPtr = tree_sitter_cpp();
        var language = new Language(langPtr);

        var parser = new Parser();
        parser.Language = language;

        var headers = Directory.GetFiles(rootFolder, "*.h", SearchOption.AllDirectories);

        foreach (var header in headers)
        {
            if (IsGeneratedHeader(header))
                continue;

            ProcessHeader(parser, header, destFolder);
        }

        Console.WriteLine("=== DONE ===");
    }

    static void ProcessHeader(Parser parser, string headerPath, string destFolder)
    {
        string code = File.ReadAllText(headerPath);
        var tree = parser.Parse(code);
        var root = tree.RootNode;

        var structs = StructExtractor.Extract(root);
        if (structs.Count == 0)
            return;

        GenerateHeaderFile(headerPath, structs, destFolder);
        GenerateCppFile(headerPath, structs, destFolder);

        Console.WriteLine($"[REFLECT] {headerPath}");
    }

    // ============================================================
    //  HEADER GENERATION (.generated.h)
    // ============================================================
    static void GenerateHeaderFile(string originalHeaderPath, List<ReflectedStruct> structs, string destFolder)
    {
        Directory.CreateDirectory(destFolder);

        string fileName = Path.GetFileNameWithoutExtension(originalHeaderPath);
        string outputPath = Path.Combine(destFolder, fileName + ".generated.h");

        var sb = new StringBuilder();

        sb.AppendLine("#pragma once");
        sb.AppendLine("// AUTO-GENERATED. DO NOT EDIT.");
        sb.AppendLine();
        sb.AppendLine("#include \"xgReflection.h\"");
        sb.AppendLine();

        foreach (var s in structs)
        {
            string ns = string.IsNullOrEmpty(s.Namespace) ? "xg" : s.Namespace;
            string keyword = s.IsClass ? "class" : "struct";
            string fullName = $"{ns}::{s.Name}";

            // Forward declaration
            sb.AppendLine($"namespace {ns} {{ {keyword} {s.Name}; }}");
            sb.AppendLine();

            // TypeInfo declaration (no member references!)
            sb.AppendLine($"namespace {ns} {{");
            sb.AppendLine($"// {keyword}: {s.Name}");
            sb.AppendLine("template<>");
            sb.AppendLine($"struct TypeInfo<{fullName}>");
            sb.AppendLine("{");
            sb.AppendLine($"    static constexpr const char* Name = \"{s.Name}\";");
            sb.AppendLine();
            sb.AppendLine("    static const RawFieldInfo* GetFields();");
            sb.AppendLine("    static int GetFieldCount();");
            sb.AppendLine("};");
            sb.AppendLine($"}} // namespace {ns}");
            sb.AppendLine();
        }

        File.WriteAllText(outputPath, NormalizeNewlines(sb.ToString()), new UTF8Encoding(false));
        Console.WriteLine($"Generated: {outputPath}");
    }

    // ============================================================
    //  CPP GENERATION (.generated.cpp)
    // ============================================================
    static void GenerateCppFile(string originalHeaderPath, List<ReflectedStruct> structs, string destFolder)
    {
        Directory.CreateDirectory(destFolder);

        string fileName = Path.GetFileNameWithoutExtension(originalHeaderPath);
        string outputPath = Path.Combine(destFolder, fileName + ".generated.cpp");

        var sb = new StringBuilder();

        sb.AppendLine("// AUTO-GENERATED. DO NOT EDIT.");
        sb.AppendLine("#include \"pch.h\""); // <<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<<
        sb.AppendLine($"#include \"{fileName}.h\"");
        sb.AppendLine($"#include \"{fileName}.generated.h\"");
        sb.AppendLine();

        foreach (var s in structs)
        {
            string ns = string.IsNullOrEmpty(s.Namespace) ? "xg" : s.Namespace;

            sb.AppendLine($"namespace {ns} {{");
            sb.AppendLine();

            // Field array
            sb.AppendLine($"static const RawFieldInfo {s.Name}_Fields[] =");
            sb.AppendLine("{");

            foreach (var f in s.Fields)
            {
                sb.AppendLine("    {");
                sb.AppendLine($"        \"{f.Name}\", \"{f.Name}\",");
                sb.AppendLine($"        offsetof({s.Name}, {f.Name}),");
                sb.AppendLine($"        \"{f.Type}\"");
                sb.AppendLine("    },");
            }

            sb.AppendLine("};");
            sb.AppendLine();

            // TypeInfo functions
            sb.AppendLine($"const RawFieldInfo* TypeInfo<{s.Name}>::GetFields() {{ return {s.Name}_Fields; }}");
            sb.AppendLine($"int TypeInfo<{s.Name}>::GetFieldCount() {{ return sizeof({s.Name}_Fields) / sizeof(RawFieldInfo); }}");

            sb.AppendLine();
            sb.AppendLine($"}} // namespace {ns}");
            sb.AppendLine();
        }

        File.WriteAllText(outputPath, NormalizeNewlines(sb.ToString()), new UTF8Encoding(false));
        Console.WriteLine($"Generated: {outputPath}");
    }

    // ============================================================
    //  UTIL
    // ============================================================
    static string NormalizeNewlines(string s)
    {
        s = s.Replace("\r\n", "\n").Replace("\r", "\n");
        s = s.Replace("\n", "\r\n");
        if (!s.EndsWith("\r\n"))
            s += "\r\n";
        return s;
    }
}
