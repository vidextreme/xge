// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

namespace xg
{
    public partial class Actor
    {
        public ActorID ID;

        internal Actor()
        {
        }

        public static Actor Create()
        {
            Actor actor = new() { ID = ActorUtils.XGCreateActor() };
            return actor;
        }

        public T GetComponent<T>() where T : class
        {
            return null;
        }

        public string Name { get => ActorUtils.GetActorName(ID); }

        internal partial class ActorUtils
        {
            [LibraryImport("xgGameKernel", EntryPoint = "XGCreateActor")]
            internal static partial ulong XGCreateActor();

            internal static string GetActorName(ActorID id)
            {
                nint ptr = XGGetActorName(id);
                return Marshal.PtrToStringUTF8(ptr);
            }

            [LibraryImport("xgGameKernel", EntryPoint = "XGGetActorName")]
            internal static partial nint XGGetActorName(ulong actorID);
        }
    }
}
