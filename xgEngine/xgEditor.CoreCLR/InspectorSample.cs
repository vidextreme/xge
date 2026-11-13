// Copyright (c) 2026 John David Uy
// Licensed under the MIT License. See LICENSE for details.

using xg;
using xg.math;

namespace xgEditor.CoreCLR
{
    // Test hook: a component on the sample "Weapon" actor that exercises the Inspector's built-in drawers.
    public enum WeaponKind
    {
        Pistol,
        Rifle,
        Shotgun,
        Launcher,
    }

    public struct RecoilSettings
    {
        public float Kick;
        public Vector2 Spread;
        public bool Randomize;
    }

    public sealed class WeaponSample
    {
        [Header("Firing")]
        [Range(0.5f, 20.0f)]
        [Tooltip("Shots per second")]
        public float FireRate = 8.0f;

        [Range(1, 200)]
        public int MagazineSize = 30;

        public int Ammo = 24;
        public bool Automatic = true;
        public WeaponKind Kind = WeaponKind.Rifle;

        [Header("Presentation")]
        public string DisplayName = "Carbine";
        public Vector3 MuzzleOffset = new(0.0f, 0.05f, 0.6f);
        public RecoilSettings Recoil = new() { Kick = 1.5f, Spread = new Vector2(0.2f, 0.4f) };

        [SerializeField]
        private float _heat = 0.25f;

        [HideInInspector]
        public int HiddenCounter;

        public float Heat => _heat;
    }
}
