using System;
using System.Collections.Generic;
using TreeSitter;

public sealed class ReflectedStruct
{
    public string Name { get; set; } = string.Empty;
    public string? Namespace { get; set; }

    public bool IsClass;

    // Struct-level metadata (M2 key=value)
    public Dictionary<string, string> Meta { get; set; } = new();

    public List<ReflectedField> Fields { get; set; } = new();
}

public sealed class ReflectedField
{
    public string Name { get; set; } = string.Empty;
    public string Type { get; set; } = string.Empty;

    // Field-level metadata (M2 key=value)
    public Dictionary<string, string> Meta { get; set; } = new();
}

public static class StructExtractor
{
    public static List<ReflectedStruct> Extract(Node root)
    {
        var result = new List<ReflectedStruct>();
        Walk(root, null, result);
        return result;
    }

    [ThreadStatic]
    private static ReflectedStruct? _currentStruct;

    private static void Walk(Node node, string? currentNamespace, List<ReflectedStruct> output)
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

            // XG_SERIALIZABLE(); struct/class Foo { ... }
            case "expression_statement":
                {
                    if (IsXgSerializableCall(node, out var structMeta))
                    {
                        var structNode = FindNextNamedSiblingOfType(node, "struct_specifier")
                                      ?? FindNextNamedSiblingOfType(node, "class_specifier");

                        if (structNode != null)
                        {
                            var rs = ExtractStruct(structNode, currentNamespace, structMeta);
                            if (rs != null)
                                output.Add(rs);
                        }
                    }
                    break;
                }

            // XG_FIELD(); <field_declaration>
            case "declaration":
                {
                    if (IsXgFieldDeclaration(node, out var fieldMeta))
                    {
                        var fieldDecl = FindNextNamedSiblingOfType(node, "field_declaration");
                        if (fieldDecl != null && _currentStruct != null)
                        {
                            var field = ExtractField(fieldDecl, fieldMeta);
                            if (field != null)
                                _currentStruct.Fields.Add(field);
                        }
                    }
                    break;
                }
        }

        foreach (var child in node.Children)
            Walk(child, currentNamespace, output);
    }

    // ------------------------------------------------------------
    // Struct extraction
    // ------------------------------------------------------------

    private static ReflectedStruct? ExtractStruct(Node structNode, string? ns, Dictionary<string, string> meta)
    {
        string name = "";
        bool isClass = structNode.Type == "class_specifier";

        foreach (var child in structNode.Children)
        {
            if (child.Type == "type_identifier")
            {
                name = child.Text.Trim();
                break;
            }
        }

        if (string.IsNullOrEmpty(name))
            return null;

        var rs = new ReflectedStruct
        {
            Name = name,
            Namespace = ns,
            Meta = meta,
            IsClass = isClass
        };

        var prev = _currentStruct;
        _currentStruct = rs;

        var body = FindChildOfType(structNode, "field_declaration_list");
        if (body != null)
        {
            foreach (var child in body.Children)
                Walk(child, ns, new List<ReflectedStruct>());
        }

        _currentStruct = prev;
        return rs;
    }

    // ------------------------------------------------------------
    // Field extraction
    // ------------------------------------------------------------

    private static ReflectedField? ExtractField(Node fieldDecl, Dictionary<string, string> meta)
    {
        string baseType = "";
        bool isConst = false;
        int pointerCount = 0;
        string name = "";

        void Scan(Node n)
        {
            string text = n.Text.Trim();

            // Detect const ANYWHERE
            if (text == "const")
                isConst = true;

            // Detect base type
            if (n.Type == "primitive_type" ||
                n.Type == "type_identifier" ||
                n.Type == "qualified_identifier")
            {
                baseType = text;
            }

            // Detect pointers
            if (n.Type == "pointer_declarator")
                pointerCount++;

            // Detect field name
            if (n.Type == "field_identifier")
                name = text;

            foreach (var c in n.Children)
                Scan(c);
        }

        Scan(fieldDecl);

        if (string.IsNullOrEmpty(baseType) || string.IsNullOrEmpty(name))
            return null;

        // Build final type
        string finalType = "";
        if (isConst)
            finalType += "const ";

        finalType += baseType;

        for (int i = 0; i < pointerCount; i++)
            finalType += "*";

        return new ReflectedField
        {
            Name = name,
            Type = finalType,
            Meta = meta
        };
    }





    // ------------------------------------------------------------
    // Macro detection
    // ------------------------------------------------------------

    private static bool IsXgSerializableCall(Node exprStmt, out Dictionary<string, string> meta)
    {
        meta = new Dictionary<string, string>();

        var call = FindChildOfType(exprStmt, "call_expression");
        if (call == null)
            return false;

        var id = FindChildOfType(call, "identifier");
        if (id == null || id.Text.Trim() != "XG_SERIALIZABLE")
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

    private static bool IsXgFieldDeclaration(Node decl, out Dictionary<string, string> meta)
    {
        meta = new Dictionary<string, string>();

        var func = FindChildOfType(decl, "function_declarator");
        if (func == null)
            return false;

        var id = FindChildOfType(func, "identifier");
        if (id == null || id.Text.Trim() != "XG_FIELD")
            return false;

        var paramList = FindChildOfType(func, "parameter_list");
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
    // Metadata parsing (M2: key=value pairs)
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
    // Utility helpers
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

    private static Node? FindDescendantOfType(Node node, string type)
    {
        var stack = new Stack<Node>();
        stack.Push(node);

        while (stack.Count > 0)
        {
            var n = stack.Pop();
            if (n.Type == type)
                return n;

            foreach (var c in n.Children)
                stack.Push(c);
        }

        return null;
    }

    private static string? GetNamespaceName(Node nsNode)
    {
        foreach (var child in nsNode.Children)
            if (child.Type == "namespace_identifier")
                return child.Text.Trim();
        return null;
    }
}
