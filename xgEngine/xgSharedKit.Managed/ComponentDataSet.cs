// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg
{
    public abstract class ComponentDataSet
    {
        static protected List<string> _dataNames = [];
    }

    public abstract class ComponentDataSet<T1> : ComponentDataSet
            where T1 : struct, IComponentData
    {
        protected ComponentData<T1> D1;
        static ComponentDataSet()
        {
            _dataNames.Add(typeof(T1).Name);
        }
    }

    public abstract class ComponentDataSet<T1, T2> : ComponentDataSet<T1>
            where T1 : struct, IComponentData
            where T2 : struct, IComponentData
    {
        protected ComponentData<T2> D2;
        static ComponentDataSet()
        {
            _dataNames.Add(typeof(T2).Name);
        }
    }
}
