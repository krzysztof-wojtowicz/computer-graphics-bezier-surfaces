# Bézier Surfaces

The second project from the Computer Graphics (Grafika Komputerowa) course at MiNI WUT: displaying, rotating,
filling and lighting cubic Bézier surfaces. Every pixel is computed by the program itself (scan-line filling,
Z-buffer, lighting) and drawn into an image – no OpenGL.

## Features

- Cubic Bézier surfaces read from a text file at start-up (three built-in surfaces: a dome, a wave, a saddle),
  triangulated with an adjustable accuracy.
- Rotating the scene by dragging the mouse – incremental, unlimited, always about the screen axes.
- Triangle filling with vertex sorting, a Z-buffer, per-pixel interpolation of the normal vector and depth
  with barycentric coordinates.
- Lambert + specular lighting with adjustable `kd`, `ks`, `m` and light color.
- A point light moving along a closed, editable Bézier curve (drag its control points), animation can be stopped,
  height of the curve set with a slider.
- Object color from a solid color or a texture; normal mapping (`N = [Pu, Pv, N] · N_map`). Built-in textures and
  normal maps, others loaded from files.
- Showing / hiding the Bézier polygons, the mesh, the vertices, the fill, the light path and single surfaces.
- Keyboard shortcuts for every menu action and built-in documentation in Polish and English (`F1`).

## Download

Every push is built and tested by GitHub Actions on Linux, Windows and macOS. Ready-to-run packages are attached
to each successful run (**Actions → Build → the latest run → Artifacts**):

- `bezier_surfaces-windows` – a zip with `bezier_surfaces.exe` and the Qt libraries; unpack and run the `.exe`,
- `bezier_surfaces-macos` – a `.dmg` with the app bundle. The app is not signed: on the first start open it with
  right click → *Open*.

## Usage

Drag with the left mouse button to rotate the scene. The side panel controls the triangulation, what is drawn,
the surfaces, the texture and the lighting. Press `F1` (menu *Help → Documentation*) for the full documentation:
the user manual, all keyboard shortcuts and a detailed technical description of how the mouse movement becomes
a rotation.

Most useful shortcuts (`Ctrl` is `⌘` on macOS):

| Key | Action |
|---|---|
| `Space` | stop / resume the moving light |
| `T` / `N` | texture / normal map on and off |
| `R` | reset the rotation |
| `F11` | full screen |
| `F1` | documentation |

## Building

Requirements:

- a C++20 compiler (GCC, Clang or MSVC 2022),
- CMake ≥ 3.21, Ninja (optional, recommended),
- Qt ≥ 6.4 (module *Qt Widgets*; *Qt Test* for the tests – both are part of *qtbase*),
- an internet connection at the first configuration – GoogleTest is downloaded by CMake (`FetchContent`).

### macOS

```bash
brew install qt cmake ninja
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$(brew --prefix qt)"
cmake --build build
open build/bezier_surfaces.app
```

### Linux (Ubuntu 24.04)

```bash
sudo apt install g++ cmake ninja-build qt6-base-dev
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
./build/bezier_surfaces
```

### Windows

Install Qt with the official online installer (component *MSVC 2022 64-bit*) and Visual Studio 2022 with
*Desktop development with C++*. In the *x64 Native Tools Command Prompt for VS 2022*:

```bat
cmake -B build -DCMAKE_PREFIX_PATH=C:\Qt\6.8.3\msvc2022_64
cmake --build build --config Release
C:\Qt\6.8.3\msvc2022_64\bin\windeployqt --release build\Release\bezier_surfaces.exe
build\Release\bezier_surfaces.exe
```

Adjust the Qt path to the installed version. `windeployqt` copies the Qt libraries next to the `.exe`, so it can be
run (or zipped) without Qt on `PATH`.

## Tests

```bash
ctest --test-dir build --output-on-failure            # add -C Release on Windows (multi-config generator)
./build/tests/core_tests --gtest_filter='MeshTest.*'  # a single group
```

- `core_tests` – the geometry and rendering core (no Qt): math, Bézier surfaces and curves, the mesh, the file
  parser, projection, mouse rotation, filling, the Z-buffer, lighting, textures, normal mapping.
- `ui_tests` – the Qt widgets rendered off-screen (`QT_QPA_PLATFORM=offscreen` is set automatically): the
  drawing area checked pixel by pixel, the side panel, the menus and keyboard shortcuts, the documentation window.
- Death tests (`assert`) run only in Debug builds. Configure with `-DBEZIER_BUILD_TESTS=OFF` to skip the tests
  (and the GoogleTest download).

## Project structure

```
src/
  math/        Vec3, Mat3 – vectors, matrices, rotations                      ┐
  geometry/    Bernstein polynomials, Bézier surface and curve, mesh, parser  │ bezier_core: plain C++,
  renderer/    projection, mouse rotation, rasterizer, Z-buffer, shading,    │ no Qt
               textures, normal mapping                                       ┘
  ui/          main window, drawing area, image loading, help window (Qt)
  assets/      surfaces.txt, textures, normal maps, help_pl.md, help_en.md (embedded as Qt resources)
tests/         core_tests (GoogleTest), ui/ – ui_tests (Qt + GoogleTest)
.github/       CI: build, test and package on Linux (Qt 6.4), Windows (Qt 6.8), macOS (Qt 6.10)
```

The surface file format: every surface is 16 lines `x y z`, one per control point, in the order
`P00, P01, …, P33` (`i` along `u`, `j` along `v`); further surfaces follow as further groups of 16 lines.

---

**Author:** Krzysztof Wójtowicz<br>
**Course:** Computer Graphics (Grafika Komputerowa) @ MiNI WUT
