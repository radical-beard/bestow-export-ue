# bestow-export-ue

**The Unreal Engine sibling of [argh's `bestow_export`](https://github.com/radical-beard/argh) Godot addon:**
author content with UE's mature editors — Landscape sculpting/painting,
level layout, animation tools — and export it in **exactly the formats
the [bestow](https://github.com/radical-beard/bestow) engine consumes**,
with one button per content type.

Licensing note: this usage is explicitly permitted by the UE EULA —
exported assets are "Non-Engine Products" ("…remain Non-Engine Products
even if included in Products that use or rely on other video game
engines"), royalty-free. Seat licenses apply above $1M revenue/12mo.

## Install

Grab the prebuilt `BestowExport` folder from
[Releases](https://github.com/radical-beard/bestow-export-ue/releases)
and copy it into the engine:
`…/UE_5.7/Engine/Plugins/Marketplace/BestowExport`. Every project on
the machine then has **Tools → Bestow** — nothing to enable. Full
steps (and per-project / from-source installs):
[`BestowExport/GUIDE.md`](BestowExport/GUIDE.md).

## Layout

- `BestowExport/` — **the plugin.** Start with
  [`BestowExport/GUIDE.md`](BestowExport/GUIDE.md).
- `HostProject/` — minimal C++ project used to compile the plugin and run
  its self-test commandlet in CI. Not needed to *use* the plugin. (It
  references the plugin by symlink, so with an engine-wide copy also
  installed UE logs a duplicate-plugin notice and uses the project one —
  that's fine for development.)

## Build (development)

```sh
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/Mac/Build.sh" \
    BestowHostEditor Mac Development \
    -project="$PWD/HostProject/BestowHost.uproject" -waitmutex
```

## Self-test (headless CI)

```sh
"/Users/Shared/Epic Games/UE_5.7/Engine/Binaries/Mac/UnrealEditor-Cmd" \
    "$PWD/HostProject/BestowHost.uproject" \
    -run=BestowSelfTest -targetdir=/tmp/bestow-ue-e2e -unattended -nop4 -nosplash
```

## License

MIT OR Apache-2.0 (the plugin code). Unreal Engine itself is governed by
Epic's EULA.
