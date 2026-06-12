# Exporting to bestow — the guide

You build things in Unreal. One button puts each thing in the game.

Every export works the same way: the panel tells you what it will export,
a button is grey with a plain-words reason until it's ready, and
`Copy snippet` gives you the text to paste into the game.

## Installing it (once per computer)

The plugin is one folder named `BestowExport`. You want a
**ready-to-use** copy — one with a `Binaries` folder inside it. The
easiest place to get one is the
[Releases page](https://github.com/radical-beard/bestow-export-ue/releases):
download the zip and unzip it. You should end up with a folder named
exactly `BestowExport` with `Binaries` inside — that whole folder is
what you copy. Then:

1. Quit Unreal.
2. Copy the `BestowExport` folder into the engine's `Marketplace`
   plugin folder — create the `Marketplace` folder if it isn't there:
   - **Mac:** `/Users/Shared/Epic Games/UE_5.7/Engine/Plugins/Marketplace/`
     (in Finder press **Cmd-Shift-G**, paste the path, press Return)
   - **Windows:** `C:\Program Files\Epic Games\UE_5.7\Engine\Plugins\Marketplace\`
     (paste the path into the Explorer address bar)
3. **Mac only:** macOS quietly blocks plugins that came in through a
   browser download. Open Terminal (Cmd-Space, type `Terminal`) and
   paste this, then press Return:

   ```sh
   xattr -dr com.apple.quarantine "/Users/Shared/Epic Games/UE_5.7/Engine/Plugins/Marketplace/BestowExport"
   ```

4. Open any project. **Tools → Bestow** is just there. (No project
   yet? In the Epic Games Launcher press **Launch** next to UE 5.7,
   pick any template — Games → Third Person is fine — name it, press
   **Create**.)

That's the whole install: every project on this computer gets it
(Blueprint-only ones too), there's nothing to enable, and the glTF
Exporter plugin it leans on ships with the engine.

**Only have the source (a fresh `git clone`)?** Make the ready-to-use
copy yourself — this is the only step that needs the C++ toolchain
(Xcode on Mac, Visual Studio on Windows). Run this from inside the
cloned repo's folder:

```sh
"/Users/Shared/Epic Games/UE_5.7/Engine/Build/BatchFiles/RunUAT.sh" BuildPlugin \
    -plugin="$PWD/BestowExport/BestowExport.uplugin" \
    -package=/tmp/BestowExport -TargetPlatforms=Mac -Rocket
```

(On Windows it's `RunUAT.bat` and `-TargetPlatforms=Win64`.)

One catch before you install it: the packager drops a line from the
descriptor. Open `/tmp/BestowExport/BestowExport.uplugin` in any text
editor and add this line right under `"Installed": true,` — without it
the plugin sits disabled and there's no Tools → Bestow:

```
"EnabledByDefault": true,
```

(The Releases-page copy already has it.) Now install `/tmp/BestowExport`
with the numbered steps above.

**Want it in just one project instead?** If that project is a C++
project, copy the source `BestowExport` folder into its `Plugins/`
folder and reopen — Unreal offers to compile it for you.
(Blueprint-only projects can't compile plugins; use the engine-wide
install above for those.)

## The three steps (terrain)

Open your level in Unreal — one with a **Landscape** (UE's terrain) in
it. In the top menu bar pick **Tools → Bestow**. A panel opens:

1. **Your bestow game** — press `Choose game folder…` and pick the
   game's folder (the one that has `game.toml` inside). You do this
   once; it shows `✓` when it's right.
2. **Name this export** — leave it empty and it uses the level's name,
   or type your own (e.g. `gloom_isle`).
3. Press **`Export Terrain (Landscape)`**.

That's it. The panel shows `Exported ✓` with where the files went.

## Seeing it in the game

The export made a folder `assets/terrain/<name>/` inside the game with
everything in it — heights, painted layers, the shader.

To place it in a scene, press **`Copy snippet`** in the panel, then
open the game's scene file in any text editor — it's inside the game
folder you linked in step 1, e.g. `scenes/main.scene.toml` — and paste
the snippet at the very bottom of the file. If bestow is already
running, it reloads by itself — the terrain just appears, exactly
where it sat in the Unreal world, at the same heights, in meters.

Re-exporting after more sculpting: press the button again. Same files,
updated. A running game picks the changes up live.

One heads-up: Unreal keeps its grass/rock detail textures inside
materials, so the export writes flat `grass.png` / `rock.png`
placeholders. Replace those two files with any two color images you
like — keep the names `grass.png` and `rock.png`. Re-exports never
overwrite yours.

## The other buttons (same three-step rhythm)

| Button | What it exports | When it lights up |
|---|---|---|
| **Export Selected as Mesh (.glb)** | the selected actors as one `.glb` the game renders AND collides with (exact triangle collision) | actors are selected in the level |
| **Export Scene Layout** | Target Points (markers), point lights, the camera, sun + fog (as a bestow sky), actor tags → a playable `scenes/<name>.scene.toml` | always (with a game linked) |
| **Export Animation Metadata** | **Anim Notifies** → gameplay events the game hears as `anim.event`; **Montage sections** → combo pieces (a name ending in `!` means the player can cancel out, e.g. `slash!`); **skeletal-mesh sockets** → weapon/attachment sockets | always (with a game linked) — it scans everything in your Content folder |
| **Export Splines** | every actor with a Spline component → path files (points, tangents, and a 1 m-step baked polyline) | always (with a game linked) |

### Building a scene Unreal-side, in plain words

- Drop a **Target Point** where something should stand. Its **label**
  becomes the entity name. Add an **Actor Tag** like `player_spawn` and
  the game sees it as a bestow tag.
- An Actor Tag of **`template:<name>`** tells bestow to spawn that
  entity template there — that's how you place game things (enemies,
  chests) with Unreal's move/rotate tools.
- **Point Lights**, a **Camera Actor**, a **Directional Light** and
  **Exponential Height Fog** all come across as the matching bestow
  components. (bestow supports 8 lights — the export warns past that.)

### Playing a combo section from the game's Lua

```lua
anim.play(entity, "assets/anims/attack_combo.fbx", { section = "slash" })
-- and listen for the notify events:
-- hook: on-event / events: [anim.event] → ev.payload.name, ev.payload.time
```

Name your Unreal animation assets the same as the clip files your game
loads (an asset named `attack_combo` writes
`assets/anims/attack_combo.fbx.anim.toml`) and bestow matches them up
automatically.

## If a step won't go

A grey button has its reason written in the note under the buttons.
The most common ones:

| It says | Do this |
|---|---|
| There's no **Tools → Bestow** menu entry at all | The plugin isn't installed — do [Installing it](#installing-it-once-per-computer), then restart Unreal. Already did? On Mac, run the `xattr` command from install step 3 — a browser-downloaded copy stays blocked until you do. |
| "No game linked yet…" | Step 1. |
| "…has no game.toml — pick the game folder itself." | You picked a folder *near* the game. Pick the folder that directly contains `game.toml`. |
| "Terrain needs a Landscape in the level." | This level has no terrain. Make one: in the toolbar above the viewport, open the **Select Mode** dropdown and pick **Landscape**, press the green **Create** button, sculpt with a drag — then switch the dropdown back to **Select**. |
| "Mesh export needs selected actors…" | Click the things you want in the `.glb`, then press the button. |
| "The Landscape has no components yet — sculpt something first." | The Landscape is empty. Sculpt, then export. |
| "Nothing to export yet: add notifies… sections… or sockets…" | Your animations have no bestow-relevant data yet. Add a notify, a montage section, or a socket. |

## What exactly gets written

Everything lands inside the game you linked, next to its other assets,
with bestow `*.import.toml` id sidecars that stay stable across
re-exports:

| Export | Files |
|---|---|
| Terrain | `assets/terrain/<name>/` — `<name>.hgt.png` + `.hgt.toml` (16-bit heights + placement), `<name>.ctl.png` (painted rock/grass + autoshader mask), `<name>.layer.<layer>.png` (every painted layer, lossless), `grass.png` / `rock.png` (placeholders, yours are kept), `terrain_baked.slang`, `<name>.entity.toml` (the snippet) |
| Mesh | `assets/models/<name>/<name>.glb` + `<name>.entity.toml` (render + exact-mesh physics) |
| Scene | `scenes/<name>.scene.toml` — play it with `scene.load("scenes/<name>.scene.toml")` |
| Animation | `assets/anims/<clip>.fbx.anim.toml` (events + sections) and `assets/anims/<mesh>.sockets.toml` |
| Splines | `assets/splines/<name>.spline.toml` |

Coordinates convert exactly the way Epic's own glTF exporter does, so
the `.glb` meshes and the TOML placements always agree: centimeters
become meters, and Unreal's Z-up left-handed frame becomes bestow's
Y-up right-handed one.

---

## For scripts/CI: headless self-test

```sh
UnrealEditor-Cmd YourProject.uproject -run=BestowSelfTest \
    -targetdir=/tmp/bestow-selftest -unattended -nop4 -nosplash
```

Spawns a fixture level (including a real synthetic Landscape), runs the
terrain, scene, animation, and spline exporters, and asserts the output
— positions in meters, heights round-tripped, tags carried. (Mesh
export needs an editor selection, so it isn't covered.) Exit code 0 on
success.
