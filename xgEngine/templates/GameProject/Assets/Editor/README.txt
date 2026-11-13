Editor-only scripts go in any folder named "Editor" under Assets (this one, or Assets/<Feature>/Editor).
They build into <Name>.Editor.dll, which only the editor loads, and can use the editor API (xg.editor):
property drawers ([CustomPropertyDrawer] classes deriving from PropertyDrawer) and other editor extensions.
Runtime scripts (everywhere else under Assets) cannot reference the editor API, and Editor scripts never ship in a game build.
