using System;
using System.IO;
using System.Collections.Generic;
using System.Text;
using TreeSitter;

class Program
{
    [System.Runtime.InteropServices.DllImport("tree-sitter-cpp")]
    public static extern IntPtr tree_sitter_cpp();

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
            ProcessHeader(parser, header, destFolder);

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

        Console.WriteLine($"[REFLECT] {headerPath}");
    }

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

            // Specialization
            sb.AppendLine($"namespace {ns} {{");
            sb.AppendLine($"// {keyword}: {s.Name}");
            sb.AppendLine("template<>");
            sb.AppendLine($"struct TypeInfo<{fullName}>");
            sb.AppendLine("{");
            sb.AppendLine($"    static constexpr const char* Name = \"{s.Name}\";");
            sb.AppendLine();
            sb.AppendLine($"    static constexpr int FieldCount = {s.Fields.Count};");
            sb.AppendLine();
            sb.AppendLine("    static constexpr RawFieldInfo Fields[FieldCount] =");
            sb.AppendLine("    {");

            foreach (var f in s.Fields)
            {
                sb.AppendLine("        {");
                sb.AppendLine($"            \"{f.Name}\",");
                sb.AppendLine($"            \"{f.Name}\",");

                // Pointer-to-member encoded as integer; works with forward declaration
                sb.AppendLine($"            (std::uintptr_t)&{fullName}::{f.Name},");

                sb.AppendLine($"            \"{f.Type}\"");
                sb.AppendLine("        },");
            }

            sb.AppendLine("    };");
            sb.AppendLine("};");
            sb.AppendLine($"}} // namespace {ns}");
            sb.AppendLine();
        }

        string output = sb.ToString();
        output = output.Replace("\r\n", "\n").Replace("\r", "\n");
        output = output.Replace("\n", "\r\n");
        if (!output.EndsWith("\r\n"))
            output += "\r\n";

        File.WriteAllText(outputPath, output, new UTF8Encoding(false));

        Console.WriteLine($"Generated: {outputPath}");
    }
}
