# Port Carverton Sessions

A self-contained 3D skateboarding simulation in C++17 + SDL2 + OpenGL 3.3,
inspired by EA's Skate 3. No external assets and no game engine — every mesh
and every texture is generated procedurally at startup.

## Features

- **Skate 3-era visuals**: HDR pipeline with bloom, sun glare, filmic tonemap,
  barrel-distortion "filmer" lens, vignette, distance fog, 2048² PCF dynamic
  shadows, Blinn-Phong + hemisphere-ambient lighting.
- **Procedural park**: concrete plaza with expansion joints and waxed ledges,
  5-stair with handrails and hubbas, sunken bowl, funbox, parking lot with
  painted lines, planters, trees, barriers, a wooden/concrete **Mega Ramp**
  with a real gap, plus a coastal-city backdrop.
- **Physics**: push to build speed, carve, friction, true slope physics
  (bomb the Mega Ramp roll-in), board that flips/spins independently
  under your feet.
- **Flick-It-style tricks**: crouch to load, release with a direction for
  ollie / kickflip / heelflip / pop shove-it / 360 flip; grabs in the air;
  physics-based 50-50 and boardslide grinds with a balance meter.
- **Hall of Meat**: land wrong, clip an edge or slam a wall at speed and you
  ragdoll, with an impact readout.
- **Session marker**: drop a marker anywhere and teleport back to retry a line.

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

## Controls

| Input | Action |
|---|---|
| `W` / `Up` | Push (skating) |
| `S` / `Down` | Brake / revert |
| `A` `D` / `Left` `Right` | Carve |
| `Ctrl` or `C` (hold) | Crouch — release to ollie |
| `Ctrl` + direction on release | Kickflip (`←`), Heelflip (`→`), Pop Shove-it (`↓`), 360 Flip (`←+→` quick) |
| `Space` | Pop out of grinds / extra ollie |
| Arrow keys in air | Flip/spin tricks |
| `Q` `E` `F` `G` in air | Grabs (indy, mute, melon, nosegrab) |
| `M` | Set session marker |
| `T` | Teleport to marker |
| `Esc` | Quit |

Approach a rail or ledge and ollie onto it: landing parallel gives a 50-50,
perpendicular a boardslide. Keep `|balance| < 1` with `A`/`D` or you slam.
