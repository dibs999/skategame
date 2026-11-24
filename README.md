# Port Carverton Sessions

A self-contained 3D skateboarding simulation in C++17 + SDL2 + OpenGL 3.3,
inspired by EA's Skate 3. No external assets and no game engine — every mesh
and every texture is generated procedurally at startup.

## Features

- **Skate 3-era visuals**: HDR pipeline with bloom, sun glare, filmic tonemap,
  chromatic aberration + film grain, barrel-distortion "filmer" lens, vignette,
  distance fog, 2048² PCF dynamic shadows, Blinn-Phong + hemisphere-ambient
  lighting, and a golden-hour mode (N) with low warm sun and haze.
- **Procedural park**: concrete plaza with expansion joints and waxed ledges,
  5-stair with handrails and grindable hubbas, sunken bowl with coping,
  funbox, flatbar, picnic table, kicker ramp, mini ramp, parking lot with
  painted lines, planters, trees, barriers, a wooden/concrete **Mega Ramp**
  with a real gap, plus a coastal-city skyline and harbor backdrop.
- **Physics**: push to build speed, real carving (velocity follows the deck),
  fakie after 180s, friction, slope physics (bomb the Mega Ramp roll-in),
  transition pumping, powerslides, wallrides, and a board that flips/spins
  independently under your feet.
- **Flick-It-style tricks**: crouch to load, release with a direction for
  ollie / kickflip / heelflip / pop shove-it / 360 flip; diagonal combos for
  varial kick/heel, hardflip and inward heelflip; aerial spins (FS/BS 180+);
  grabs in the air; manuals/nose-manuals with balance; five grind types
  (50-50, boardslide, lipslide, smith, crooked) chosen by steer at lock-on.
- **Named gaps**: MEGA GAP, STAIR SET, OVER THE FUNBOX, BOWL TRANSFER.
- **Score Attack**: press O for a 2-minute timed run — bank the biggest total.
- **Hall of Meat**: land wrong, clip an edge or slam a wall at speed and you
  ragdoll; every bail pays out meat points by speed and fall height.
- **Session tools**: drop a marker anywhere and teleport back to retry a
  line, or jump straight to five named spots (1-5).

## Build

```sh
sudo apt install libsdl2-dev libgl1-mesa-dev libglew-dev cmake   # Debian/Ubuntu
cmake -B build
cmake --build build -j
```

Run:

```sh
./build/skategame
```

Debug helpers: `SKATE_SPAWN="x,z,yaw[,y]"` sets the spawn point;
`SKATE_DEBUGPOS=1` prints position/state/speed/fps telemetry.

## Controls

| Input | Action |
|---|---|
| `W` / `Up` | Push |
| `S` / `Down` | Brake — with a hard carve: powerslide |
| `A` `D` / `Left` `Right` | Carve (ground) / spin (air) |
| `Ctrl` or `C` (hold, release) | Crouch → ollie; hold on transitions to pump |
| `Space` | Instant small ollie / pop out of grinds |
| Arrow keys in air | Kickflip `←`, Heelflip `→`, Pop Shove-it `↓`, 360 Flip `↑` |
| Two arrows in air | Varial Kick `←↓`, Varial Heel `→↓`, Hardflip `←↑`, Inward Heel `→↑` |
| `Q` `E` `F` `G` in air | Grabs: indy, melon, nosegrab, tailgrab |
| `R` / `V` | Manual / nose manual (balance with `A`/`D`) |
| `O` | Score Attack — 2-minute timed run |
| `N` | Golden-hour lighting toggle |
| `1`–`5` | Teleport: Plaza, Bowl, Mega Ramp, Mini Ramp, Stair Set |
| `M` / `T` | Set session marker / teleport to marker |
| `Esc` | Quit |

Grinds: ollie onto a rail or ledge — parallel gives a 50-50, perpendicular a
boardslide; hold `A`/`D` at lock-on for smith/crooked/lipslide. Keep the
balance meter centered or you slam.
