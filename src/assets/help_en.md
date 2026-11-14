# Bézier surfaces – help

## About

The program displays cubic Bézier surfaces read from a text file. It divides every surface into a triangle mesh, rotates the whole scene with the mouse, fills the triangles with a depth buffer (Z-buffer) and lights them with the Lambert model plus a specular term. The light source moves along a closed, editable Bézier curve. The object color can come from a solid color or from a texture, and the normal vector can be modified by a normal map.

## Quick start

1. Drag with the mouse in the drawing area (left button) to rotate the scene.
2. `Space` stops and resumes the moving light.
3. `T` switches the texture on, `N` switches the normal map on.
4. `R` restores the initial rotation of the scene.
5. `F1` opens this window; the "Polski" button at the bottom switches the language of the help.

## Program window

The drawing area is on the left, the control panel on the right. The origin of the coordinate system is in the middle of the drawing area; the scene always fits in the window, whatever the rotation.

### Triangulation

- "Resolution n" – accuracy of the triangulation (from 1 to 50). Every surface has `(n+1)²` vertices and `2n²` triangles.

### Display

The check boxes decide what is drawn:

| Check box | What it draws |
|---|---|
| "Bezier polygon" | the Bézier polygon (the 4×4 grid of control points) and the control points |
| "Triangle mesh" | the edges of the mesh triangles |
| "Mesh points" | the mesh vertices |
| "Triangle fill" | filled, lit triangles |
| "Light path" | the light curve, its control polygon, its control points and the current light position |

### Surfaces

- "Surface 1", "Surface 2", "Surface 3" – show or hide surfaces. Surface number `k` (counting from 1) belongs to group `((k−1) mod 3) + 1`: "Surface 1" controls surfaces 1, 4, 7…, "Surface 2" – 2, 5, 8… A hidden surface doesn't cover what is behind it.
- Color swatches under the check boxes – a click opens the choice of the solid color of that group of surfaces (the same choice is in the "Surface" menu).

### Surface texture

- "Solid color" / "Texture" – the object color `IO` in the lighting equation: a solid color or a texture.
- Texture list and the "Load..." button – choose a built-in texture or load an image from a file. Choosing a texture switches it on at once; a loaded file is added to the list.
- "Normal map" – switches on the modification of the normal vector by a normal map, independently of the object color.
- Normal map list and the "Load..." button – choose a built-in normal map or load one from a file.

### Lighting

- "kd", "ks" (from 0 to 1) – share of the diffuse and the specular term.
- "m" (from 1 to 100) – the larger, the smaller and sharper the highlight.
- "z" (from 0 to 20) – height of the plane the light curve lies on.
- "Light color..." – the light color `IL` (white by default).
- "Stop animation" / "Start animation" – stops and resumes the moving light.

## Menu

| Menu | Item | Action |
|---|---|---|
| "File" | "Quit" | closes the program |
| "Scene" | "Reset rotation" | restores the initial rotation of the scene |
| "Scene" | "Full screen" | toggles full screen mode |
| "Surface" | "Texture" | toggles the object color: texture / solid color |
| "Surface" | "Normal map" | switches the normal map on and off |
| "Surface" | "Load texture..." | loads a texture from a file |
| "Surface" | "Load normal map..." | loads a normal map from a file |
| "Surface" | "Surface color 1..." – "Surface color 3..." | choice of the solid color of a group of surfaces |
| "Lighting" | "Animate light" | stops and resumes the moving light |
| "Lighting" | "Light color..." | choice of the light color |
| "Lighting" | "Reset light path" | restores the default shape of the light curve (the height `z` stays) |
| "Help" | "Documentation" | opens this window |

## Mouse controls

- **Rotating the scene:** press the left button in the drawing area and drag. The cursor turns into a closed hand. Moving right and left rotates the scene about the vertical axis of the screen, moving up and down – about the horizontal one. The front of the scene follows the cursor. The angles are not limited. Details – section "Technical description: from mouse movement to rotation".
- **Editing the light curve:** when "Light path" is checked, pressing the left button closer than 8 px to a control point of the curve grabs that point and dragging moves it (instead of rotating the scene). The point nearest to the cursor is grabbed; it doesn't jump to the cursor and can't be dragged out of the window. The curve is bound to the camera – rotating the scene doesn't rotate it.
- The right mouse button does nothing.

## Keyboard shortcuts

On macOS `Ctrl` means the `⌘` (Command) key, and the function keys may need `fn` held down.

| Shortcut | Action |
|---|---|
| `Space` | stop / resume the moving light |
| `T` | texture / solid color |
| `N` | normal map on / off |
| `R` | restore the initial rotation of the scene |
| `F11` | full screen (on macOS also `Ctrl+⌘+F`) |
| `Ctrl+T` | load a texture from a file |
| `Ctrl+Shift+N` | load a normal map from a file |
| `Ctrl+1`, `Ctrl+2`, `Ctrl+3` | solid color of surface group 1, 2, 3 |
| `Ctrl+L` | light color |
| `Ctrl+R` | restore the default light curve |
| `F1` | help (on macOS also `⌘?`) |
| `Ctrl+Q` | quit the program |

## Files

### Surfaces

At start the program loads the file `surfaces.txt` built into the program (three surfaces: a dome, a wave, a saddle). Every surface is 16 consecutive lines, one per control point, each with three numbers `x y z` (decimal point):

```
X00 Y00 Z00
X01 Y01 Z01
...
X33 Y33 Z33
```

The order is `P00, P01, …, P03, P10, …, P33`; in the point `Pij` the index `i` runs along the parameter `u` and `j` along `v`. Further surfaces are further groups of 16 lines. On an error the program reports the number and the content of the wrong line.

### Textures and normal maps

- Formats: PNG, JPG, BMP, any size. The alpha channel is ignored.
- The image is stretched over the whole surface: `u` from 0 to 1 maps to the columns from left to right, `v` from 0 to 1 – to the rows from top to bottom. Every screen pixel takes the nearest texel.
- Normal maps use the OpenGL convention: `RGB(128, 128, 255)` means the vector `[0, 0, 1]` (flat), the green component grows towards the top of the image.

## Technical description: from mouse movement to rotation

### Conventions

- Vectors are columns, a point after rotation is `P' = R · P`.
- The observer looks from `+z`; the program draws the orthographic projection onto the `xy` plane: `x_screen = w/2 + x·s`, `y_screen = h/2 − y·s`, where `s = 0.5·min(w, h) / 5.2` (5.2 is the radius of the sphere the whole scene fits in). The image `y` axis grows downwards, hence the minus.
- The orientation of the scene is a single rotation matrix `R` (3×3). The program stores no Euler angles.

Rotation matrices about the `x` and `y` axes:

```
         | 1    0       0    |            |  cos θ   0   sin θ |
Rx(θ) =  | 0  cos θ  −sin θ  |    Ry(θ) = |    0     1     0   |
         | 0  sin θ   cos θ  |            | −sin θ   0   cos θ |
```

### Rotation increment from mouse movement

Every mouse move event with the left button pressed gives a shift `dx`, `dy` in pixels **from the previous cursor position** (not from where the drag started). The shift gives two angles:

```
β = dx · k      (rotation about the vertical axis of the screen)
α = dy · k      (rotation about the horizontal axis of the screen)
k = 0.01 rad/px (100 px ≈ 57°)
```

The new orientation is the increment composed with the current one:

```
R ← orthonormalize( Ry(dx·k) · Rx(dy·k) · R )
```

The rotation is therefore **incremental**: every small shift adds its own increment to the current orientation.

### Why the increment is multiplied from the left

The product `Rincrement · R` first turns the scene into its current orientation and then rotates the result about the **screen** axes. Thanks to that, moving the mouse to the right always turns the scene about the vertical axis of the screen, however the scene is already rotated. Multiplying from the right (`R · Rincrement`) would rotate about the **object** axes – after turning the scene upside down, moving right would turn it left and the controls would stop being intuitive.

### Why the angles have no minus signs

Check on the front point of the scene `P = (0, 0, 1)` (closest to the observer):

- `Ry(β) · P = (sin β, 0, cos β)` – for a move to the right (`dx > 0`, `β > 0`) the point goes towards `+x`, i.e. right on the screen;
- `Rx(α) · P = (0, −sin α, cos α)` – for a move down (`dy > 0`, as the screen `y` grows downwards) the point goes towards the scene `−y`, i.e. down on the screen.

So the front of the scene follows the cursor without any extra sign changes:

| Mouse movement | Front of the scene |
|---|---|
| right | right |
| left | left |
| up | up |
| down | down |

Example: dragging 100 px to the right (`dx = 100`, `dy = 0`) gives `β = 1 rad ≈ 57°` and `R ← Ry(1) · R`.

### Orthonormalization

After thousands of multiplications the rounding errors would make the matrix stop being a rotation (the scene would start to stretch and shear). That is why after every step the columns of the matrix are orthonormalized with the Gram–Schmidt method: `c0 ← c0 / |c0|`, `c1 ← normalize(c1 − (c1·c0)·c0)`, `c2 ← c0 × c1`.

### Rotating the geometry

- Every vertex keeps the point `P`, the tangent vectors `Pu`, `Pv` and the normal vector `N` **before rotation** and **after rotation**. The rotated version is always computed from the original one: `P_after = R · P_before` – errors don't accumulate in the geometry.
- `Pu`, `Pv` and `N` are rotated by the same matrix as direction vectors (no translation), and `N` is normalized afterwards.
- The angles are not limited and there is no gimbal lock, because the orientation is a matrix, not three angles.

## Technical description: rendering

### Triangulation

Mesh vertex `(i, j)` has parameters `u = i/n`, `v = j/n` and index `i·(n+1) + j`. For every vertex the program computes the point `P(u, v)`, the derivatives `Pu`, `Pv` (Bernstein polynomials) and the normal vector `N = normalize(Pu × Pv)`; when `Pu × Pv ≈ 0`, it takes `N = [0, 0, 1]`. Every mesh cell is two triangles.

### Filling and Z-buffer

- Triangles are filled with the **vertex sorting** algorithm: the vertices are sorted by `y` and the scan line moves from top to bottom, adding and removing edges at the consecutive vertices. A pixel is filled when its center lies in the triangle (the bottom and right edges don't belong to the triangle, so neighbouring triangles neither overlap nor leave gaps).
- For every pixel the program computes barycentric coordinates and interpolates `z`, `N`, `P` and `u`, `v` with them.
- Depth buffer: a pixel is drawn only when its `z` is larger (closer to the observer) than the one stored at that place so far.

### Lighting

The color of every pixel is computed separately for R, G and B (values from 0 to 1, the result converted to 0–255):

```
I = kd · IL · IO · cos(N, L) + ks · IL · IO · cos^m(V, R)
L = normalize(Plight − P),  V = [0, 0, 1],  R = 2·⟨N, L⟩·N − L
```

Negative cosines are replaced by zero. The surfaces are open, so the lighting is two-sided: when `N` points away from the observer (`N.z < 0`), the program uses `−N`.

### Light source

The light is a point light moving along a closed Bézier curve made of `K` cubic segments (`3K` control points; segment `i` is points `3i, 3i+1, 3i+2` and the first point of the next segment). By default it is a circle of radius 4.3 made of 4 segments at the height `z = 8`. The parameter `t` grows by 0.5 segment per second (a full lap in 8 s), computed from real time, not from the number of frames. The curve is bound to the camera.

### Texture and normal map

- Texture: the object color `IO` in a pixel is the texel at the interpolated `(u, v)`.
- Normal map: the texel color `c` gives the vector `Nmap = 2c − 1` (with the sign of the `y` component changed, because maps have `y` up the image while `v` grows downwards). The modified normal vector is `N' = normalize(M · Nmap)`, where `M = [Pu, Pv, N]` is the matrix whose columns are the normalized vectors `Pu`, `Pv` and `N`.

## Architecture

| Directory | Contents |
|---|---|
| `src/math/` | `Vec3`, `Mat3` – vectors, matrices, rotations |
| `src/geometry/` | Bernstein polynomials, Bézier surface and curve, triangle mesh, reading the file |
| `src/renderer/` | projection, mouse rotation, filling, barycentric coordinates, Z-buffer, lighting, textures, normal maps |
| `src/ui/` | program window, drawing area, panel, image loading, help window (Qt) |

The `math/`, `geometry/` and `renderer/` directories are plain C++ without Qt; Qt is only in `ui/`. The drawing area draws its own image pixel by pixel, without OpenGL.
