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

    static void GenerateCSharpEnumFile(string headerPath, List<ReflectedEnum> enums, string destManaged)
    {
        Directory.CreateDirectory(destManaged);

        string fileName = Path.GetFileNameWithoutExtension(headerPath);
        string outputPath = Path.Combine(destManaged, fileName + ".generated.cs");

        var sb = new StringBuilder();

        sb.AppendLine("// AUTO-GENERATED. DO NOT EDIT.");
        sb.AppendLine($"// Source: {Path.GetFileName(headerPath)}");
        sb.AppendLine();

        foreach (var e in enums)
        {
            sb.AppendLine($"namespace {e.Namespace}");
            sb.AppendLine("{");
            sb.AppendLine($"    public enum {e.Name} : {e.UnderlyingType}");
            sb.AppendLine("    {");

            foreach (var (name, value) in e.Values)
            {
                if (value != null)
                    sb.AppendLine($"        {name} = {value},");
                else
                    sb.AppendLine($"        {name},");
            }

            sb.AppendLine("    }");
            sb.AppendLine("}");
            sb.AppendLine();
        }

        File.WriteAllText(outputPath, NormalizeNewlines(sb.ToString()), new UTF8Encoding(false));
        Console.WriteLine($"Generated C#: {outputPath}");
    }


    static void Main(string[] args)
    {
        Console.WriteLine("=== XG Reflection Generator ===");

        if (args.Length < 2)
        {
            Console.WriteLine("Usage: xgReflectGen <SOURCE_FOLDER> <DEST_NATIVE> [DEST_MANAGED]");
            return;
        }

        string sourceFolder = Path.GetFullPath(args[0]);
        string destNative = Path.GetFullPath(args[1]);

        Directory.CreateDirectory(destNative);
        ClearGeneratedFiles(destNative);

        // Managed generation is optional
        bool generateManaged = args.Length >= 3;
        string destManaged = null;

        if (generateManaged)
        {
            destManaged = Path.GetFullPath(args[2]);
            Directory.CreateDirectory(destManaged);
            ClearGeneratedFiles(destManaged);
        }

        if (!Directory.Exists(sourceFolder))
        {
            Console.WriteLine("ERROR: source folder does not exist: " + sourceFolder);
            return;
        }

        var langPtr = tree_sitter_cpp();
        var language = new Language(langPtr);

        var parser = new Parser();
        parser.Language = language;

        var headers = Directory.GetFiles(sourceFolder, "*.h", SearchOption.AllDirectories);

        foreach (var header in headers)
        {
            if (IsGeneratedHeader(header))
                continue;

            ProcessHeader(parser, header, destNative, destManaged, generateManaged);
        }

        Console.WriteLine("=== DONE ===");
    }

    static void ProcessHeader(Parser parser, string headerPath, string destNative, string destManaged, bool generateManaged)
    {
        string code = File.ReadAllText(headerPath);
        var tree = parser.Parse(code);
        var root = tree.RootNode;

        var structs = StructExtractor.Extract(root);
        var enums = EnumExtractor.Extract(root);

        if (structs.Count == 0 && enums.Count == 0)
            return;

        if (structs.Count > 0)
        {
            GenerateHeaderFile(headerPath, structs, destNative);
            GenerateCppFile(headerPath, structs, destNative);
        }

        if (generateManaged && enums.Count > 0)
        {
            GenerateCSharpEnumFile(headerPath, enums, destManaged);
        }

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

            // Forward declaration
            sb.AppendLine($"namespace {ns} {{ {keyword} {s.Name}; }}");
            sb.AppendLine();

            // Function specializations (NOT struct specialization)
            sb.AppendLine($"namespace {ns} {{");
            sb.AppendLine($"    template<> const RawFieldInfo* TypeInfo<{s.Name}>::Fields();");
            sb.AppendLine($"    template<> int TypeInfo<{s.Name}>::FieldCount();");
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
        sb.AppendLine("#include \"pch.h\"");
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

            // Function specializations
            sb.AppendLine($"const RawFieldInfo* TypeInfo<{s.Name}>::Fields() {{ return {s.Name}_Fields; }}");
            sb.AppendLine($"int TypeInfo<{s.Name}>::FieldCount() {{ return sizeof({s.Name}_Fields) / sizeof(RawFieldInfo); }}");

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

    static void ClearGeneratedFiles(string folder)
    {
        if (!Directory.Exists(folder))
            return;

        foreach (var file in Directory.GetFiles(folder))
        {
            string name = Path.GetFileName(file);

            if (name.Contains(".generated.", StringComparison.OrdinalIgnoreCase))
            {
                try
                {
                    File.Delete(file);
                    Console.WriteLine($"[CLEAN] Removed {file}");
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[CLEAN] Failed to remove {file}: {ex.Message}");
                }
            }
        }
    }
}
