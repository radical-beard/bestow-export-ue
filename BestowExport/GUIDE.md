# Exporting to bestow — the guide

You build things in Unreal. One button puts each thing in the game.

Every export works the same way: the panel tells you what it will export,
a button is grey with a plain-words reason until it's ready, and
`Copy snippet` gives you the text to paste into the game.

## The three steps (terrain)

Open your level in Unreal. In the top menu bar pick **Tools → Bestow**.
A panel opens:

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

To place it in a scene, press **`Copy snippet`** in the panel and
paste it into the game's scene file (e.g. `scenes/main.scene.toml`).
If bestow is already running, it reloads by itself — the terrain just
appears, exactly where it sat in the Unreal world, at the same heights,
in meters.

Re-exporting after more sculpting: press the button again. Same files,
updated. A running game picks the changes up live.

One heads-up: Unreal keeps its grass/rock detail textures inside
materials, so the export writes flat `grass.png` / `rock.png`
placeholders. Drop any two albedo textures over them once — re-exports
never overwrite yours.

## The other buttons (same three-step rhythm)

| Button | What it exports | When it lights up |
|---|---|---|
| **Export Selected as Mesh (.glb)** | the selected actors as one `.glb` the game renders AND collides with (exact triangle collision) | actors are selected in the level |
| **Export Scene Layout** | Target Points (markers), point lights, the camera, sun + fog (as a bestow sky), actor tags → a playable `scenes/<name>.scene.toml` | always (with a game linked) |
| **Export Animation Metadata** | **Anim Notifies** → gameplay events the game hears as `anim.event`; **Montage sections** → combo pieces (a name ending in `!` means the player can cancel out, e.g. `slash!`); **skeletal-mesh sockets** → weapon/attachment sockets | always — it scans everything in your Content folder |
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

A grey button always has its reason written right under it. The most
common ones:

| It says | Do this |
|---|---|
| "No game linked yet…" | Step 1. |
| "…has no game.toml — pick the game folder itself." | You picked a folder *near* the game. Pick the folder that directly contains `game.toml`. |
| "This level has no Landscape…" | This level has no terrain. Open one that does, or create one in Landscape mode. |
| "The Landscape has no components yet — sculpt something first." | The Landscape is empty. Sculpt, then export. |
| "Select one or more actors in the level first…" | Click the things you want in the `.glb`, then press the button. |
| "Nothing to export yet: add notifies… sections… or sockets…" | Your animations have no bestow-relevant data yet. Add a notify, a montage section, or a socket. |

---

## For project setup (once per machine, not per export)

These are done by whoever sets up the project — not part of the export
workflow:

- Copy the `BestowExport` folder into your Unreal project's `Plugins/`
  folder (create `Plugins/` if it doesn't exist) and reopen the project.
- Unreal will ask to compile the plugin — say yes. (That needs the
  normal C++ toolchain: Xcode on Mac, Visual Studio on Windows.)
- The **glTF Exporter** plugin it relies on is enabled automatically.

## For scripts/CI: headless self-test

```sh
UnrealEditor-Cmd YourProject.uproject -run=BestowSelfTest \
    -targetdir=/tmp/bestow-selftest -unattended -nop4 -nosplash
```

Spawns a fixture level (including a real synthetic Landscape), runs the
exporters, and asserts the output — positions in meters, heights
round-tripped, tags carried. Exit code 0 on success.

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
