using System;
using System.Collections.Generic;
using TreeSitter;

public class MacroInfo
{
    public string MacroName { get; set; }
    public string TargetKind { get; set; }
    public string TargetName { get; set; }
}

public static class MacroExtractor
{
    public static List<MacroInfo> Extract(Node root)
    {
        var results = new List<MacroInfo>();
        Walk(root, results);
        return results;
    }

    private static void Walk(Node node, List<MacroInfo> results)
    {
        Node? pendingMacro = null;

        foreach (var child in node.Children)
        {
            // Detect macro call
            if (child.Type == "preproc_call")
            {
                pendingMacro = child;
            }
            else if (pendingMacro != null)
            {
                // Macro applies to this child
                var macroName = ExtractMacroName(pendingMacro);

                results.Add(new MacroInfo
                {
                    MacroName = macroName,
                    TargetKind = DetectTargetKind(child),
                    TargetName = DetectTargetName(child)
                });

                pendingMacro = null;
            }

            // Recurse
            Walk(child, results);
        }
    }

    private static string ExtractMacroName(Node macroNode)
    {
        // preproc_call → identifier + arguments
        foreach (var child in macroNode.Children)
        {
            if (child.Type == "identifier")
                return child.Text;
        }

        return "<unknown_macro>";
    }

    private static string DetectTargetKind(Node node)
    {
        return node.Type switch
        {
            "class_specifier" => "class",
            "struct_specifier" => "struct",
            "field_declaration" => "field",
            "enum_specifier" => "enum",
            _ => node.Type
        };
    }

    private static string DetectTargetName(Node node)
    {
        // class Foo { ... }
        var name = node.GetChildForField("name");
        if (name != null)
            return name.Text;

        // int health;
        var declarator = node.GetChildForField("declarator");
        if (declarator != null)
            return declarator.Text;

        return "<unknown>";
    }
}
