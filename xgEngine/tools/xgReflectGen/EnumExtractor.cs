using TreeSitter;
using System;
using System.Collections.Generic;

public sealed class ReflectedEnum
{
    public string Name = string.Empty;
    public string? Namespace;
    public string UnderlyingType = "int";
    public List<(string Name, string? Value)> Values = new();
}

public static class EnumExtractor
{
    public static List<ReflectedEnum> Extract(Node root)
    {
        var result = new List<ReflectedEnum>();
        Walk(root, null, result);
        return result;
    }

    private static void Walk(Node node, string? currentNamespace, List<ReflectedEnum> output)
    {
        switch (node.Type)
        {
            // namespace xg { ... }
            case "namespace_definition":
                {
                    var nsName = GetNamespaceName(node) ?? currentNamespace;
                    foreach (var child in node.Children)
                        Walk(child, nsName, output);
                    return;
                }

            // XG_ENUM(...); enum class Foo { ... }
            case "expression_statement":
                {
                    if (IsXgEnumCall(node, out var enumMeta))
                    {
                        var enumNode = FindNextNamedSiblingOfType(node, "enum_specifier");
                        if (enumNode != null)
                        {
                            var re = ExtractEnum(enumNode, currentNamespace, enumMeta);
                            if (re != null)
                                output.Add(re);
                        }
                    }
                    break;
                }
        }

        foreach (var child in node.Children)
            Walk(child, currentNamespace, output);
    }

    // ------------------------------------------------------------
    // Enum extraction
    // ------------------------------------------------------------

    private static ReflectedEnum? ExtractEnum(Node enumNode, string? ns, Dictionary<string, string> meta)
    {
        string name = "";
        string underlying = "int";

        // Extract name + underlying type
        foreach (var child in enumNode.Children)
        {
            if (child.Type == "type_identifier")
                name = child.Text.Trim();

            if (child.Type == "type")
                underlying = MapCppTypeToCSharp(child.Text.Trim());
        }

        if (string.IsNullOrEmpty(name))
            return null;

        // Override underlying type if metadata says inherit=byte, etc.
        if (meta.TryGetValue("inherit", out var inheritType))
            underlying = inheritType;

        var re = new ReflectedEnum
        {
            Name = name,
            Namespace = ns,
            UnderlyingType = underlying
        };

        // Extract enumerators
        var body = FindChildOfType(enumNode, "enumerator_list");
        if (body != null)
        {
            foreach (var child in body.Children)
            {
                if (child.Type == "enumerator")
                {
                    string enumName = "";
                    string? enumValue = null;

                    foreach (var c in child.Children)
                    {
                        if (c.Type == "identifier")
                            enumName = c.Text.Trim();

                        if (c.Type == "number_literal")
                            enumValue = c.Text.Trim();
                    }

                    if (!string.IsNullOrEmpty(enumName))
                        re.Values.Add((enumName, enumValue));
                }
            }
        }

        return re;
    }

    // ------------------------------------------------------------
    // Macro detection (same style as StructExtractor)
    // ------------------------------------------------------------

    private static bool IsXgEnumCall(Node exprStmt, out Dictionary<string, string> meta)
    {
        meta = new Dictionary<string, string>();

        var call = FindChildOfType(exprStmt, "call_expression");
        if (call == null)
            return false;

        var id = FindChildOfType(call, "identifier");
        if (id == null || id.Text.Trim() != "XG_ENUM")
            return false;

        var paramList = FindChildOfType(call, "argument_list");
        if (paramList != null)
        {
            var raw = paramList.Text.Trim();

            if (raw.StartsWith("(") && raw.EndsWith(")"))
                raw = raw.Substring(1, raw.Length - 2).Trim();

            if (!string.IsNullOrWhiteSpace(raw))
                meta = ParseMetadata(raw);
        }

        return true;
    }

    // ------------------------------------------------------------
    // Metadata parsing (same as StructExtractor)
    // ------------------------------------------------------------

    private static Dictionary<string, string> ParseMetadata(string raw)
    {
        var dict = new Dictionary<string, string>();

        var parts = raw.Split(',');
        foreach (var part in parts)
        {
            var trimmed = part.Trim();
            if (string.IsNullOrEmpty(trimmed))
                continue;

            var kv = trimmed.Split('=', 2);
            if (kv.Length == 2)
            {
                var key = kv[0].Trim();
                var value = kv[1].Trim().Trim('"');
                dict[key] = value;
            }
        }

        return dict;
    }

    // ------------------------------------------------------------
    // Helpers (identical style to StructExtractor)
    // ------------------------------------------------------------

    private static Node? FindNextNamedSiblingOfType(Node node, string type)
    {
        var n = node.NextNamedSibling;
        while (n != null)
        {
            if (n.Type == type)
                return n;
            n = n.NextNamedSibling;
        }
        return null;
    }

    private static Node? FindChildOfType(Node node, string type)
    {
        foreach (var child in node.Children)
            if (child.Type == type)
                return child;
        return null;
    }

    private static string? GetNamespaceName(Node nsNode)
    {
        foreach (var child in nsNode.Children)
            if (child.Type == "namespace_identifier")
                return child.Text.Trim();
        return null;
    }

    private static string MapCppTypeToCSharp(string cpp)
    {
        return cpp switch
        {
            "uint8_t" => "byte",
            "int8_t" => "sbyte",
            "uint16_t" => "ushort",
            "int16_t" => "short",
            "uint32_t" => "uint",
            "int32_t" => "int",
            "uint64_t" => "ulong",
            "int64_t" => "long",
            _ => "int"
        };
    }
}
