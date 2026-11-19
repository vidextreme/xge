// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using System.Reflection;
using System.Runtime.Loader;

namespace xg
{
    // A collectible load context over a build folder, so a project's scripts can be unloaded and loaded again (hot
    // reload). Assemblies come from the folder as bytes (with their PDBs), so no file stays locked and a rebuild can
    // overwrite them. resolve may supply assemblies first (for example the editor's own copies of the editor kits);
    // anything neither it nor the folder has (the framework) comes from the default context.
    public sealed class ScriptLoadContext : AssemblyLoadContext
    {
        private readonly string _folder;
        private readonly Func<AssemblyName, Assembly?>? _resolve;

        public ScriptLoadContext(string name, string folder, Func<AssemblyName, Assembly?>? resolve = null)
            : base(name, isCollectible: true)
        {
            _folder = folder;
            _resolve = resolve;
        }

        public string Folder => _folder;

        public Assembly LoadFile(string path)
        {
            byte[] assembly = File.ReadAllBytes(path);
            string pdb = Path.ChangeExtension(path, ".pdb");
            return LoadBytes(assembly, File.Exists(pdb) ? File.ReadAllBytes(pdb) : null);
        }

        public Assembly LoadBytes(byte[] assembly, byte[]? symbols)
        {
            using MemoryStream image = new(assembly);
            if (symbols is null)
                return LoadFromStream(image);
            using MemoryStream pdb = new(symbols);
            return LoadFromStream(image, pdb);
        }

        protected override Assembly? Load(AssemblyName name)
        {
            Assembly? supplied = _resolve?.Invoke(name);
            if (supplied is not null)
                return supplied;
            string candidate = Path.Combine(_folder, name.Name + ".dll");
            return File.Exists(candidate) ? LoadFile(candidate) : null;
        }
    }
}
