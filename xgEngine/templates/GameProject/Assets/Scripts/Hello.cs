using xg;

namespace __NAME__
{
    // Any static method marked [GameInit] runs when the project's scripts load; [GameUpdate] runs every frame.
    public static class Hello
    {
        [GameInit]
        public static void Init()
        {
            Debug.Log("[Project] __NAME__ initialized");
            Actor.Create("__NAME__ Player");
        }
    }
}
