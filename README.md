# Real-Time Rendering

Coursework for the Real-Time Rendering module at Trinity College Dublin. Four labs
building up OpenGL rendering techniques, followed by a final project implementing
voxel cone tracing for real-time global illumination.

All code is C++ with OpenGL (GLEW + freeGLUT), Assimp for model loading, and Dear
ImGui for the debug UI.

## Labs

### [Lab 1 — Shading Models](lab1/)

Three per-fragment shading models applied to the Utah teapot with a directional
light: Phong, Gooch (non-photorealistic cool-to-warm), and Oren-Nayar (rough
diffuse). All parameters (Kd/Ks/Ka, roughness, warm/cool colours, etc.) are
exposed through an ImGui panel.

<p align="center">
  <a href="https://youtu.be/3ifl2vohic8"><img src="https://img.youtube.com/vi/3ifl2vohic8/maxresdefault.jpg" alt="Lab 1 video"></a>
</p>

### [Lab 2 — Reflection & Refraction](lab2/)

Environment mapping against a cubemap skybox. Implements per-channel refraction
with chromatic dispersion (separate RGB refraction vectors) blended with reflection
using a Schlick-approximated Fresnel term. Eta is tunable at runtime.

<p align="center">
  <a href="https://youtu.be/mthqgGF24gQ"><img src="https://img.youtube.com/vi/mthqgGF24gQ/maxresdefault.jpg" alt="Lab 2 video"></a>
</p>

### [Lab 3 — Normal Mapping](lab3/)

Tangent-space normal mapping over a Blinn-Phong base. Vertex shader builds the
TBN and transforms light/view/frag positions into tangent space; fragment shader
samples the normal map and blends with the flat normal by a controllable
intensity. Three material sets (brick, fabric, wicker).

<p align="center">
  <a href="https://youtu.be/ENdXuaAxSrY"><img src="https://img.youtube.com/vi/ENdXuaAxSrY/maxresdefault.jpg" alt="Lab 3 video"></a>
</p>

### [Lab 4 — Texture Filtering](lab4/)

Compares mipmap filtering modes (trilinear, bilinear, nearest, no-mip) on a
tiled cube surface. Includes a texture-scale slider so the difference between
minification strategies is visible at varying frequencies.

<p align="center">
  <a href="https://youtu.be/AWOyGcNZASI"><img src="https://img.youtube.com/vi/AWOyGcNZASI/maxresdefault.jpg" alt="Lab 4 video"></a>
</p>

## [Final Project — Voxel Cone Tracing Global Illumination](final-project/)

Real-time diffuse and specular GI in the Sponza atrium, implementing the core
of Crassin et al.'s *Interactive Indirect Illumination Using Voxel Cone Tracing*
on top of a Sparse Voxel Octree derived from Laine & Karras's *Efficient Sparse
Voxel Octrees*. Reference papers are in [final-project/docs/](final-project/docs/).

Pipeline:

- **CPU SVO voxelisation** of the static scene at startup (128³ default).
- **Light-view pass** rendering albedo + shadow from the light's viewpoint.
- **Radiance injection** — pull-style per-voxel projection into the light-view
  map (deviates from the paper's push-style photon splat to avoid atomics).
- **MIP filtering** — bottom-up propagation of radiance up the octree.
- **G-buffer + deferred shading** with diffuse and specular cones traced through
  the SVO for indirect light.

Deviations from the paper (isotropic voxels, CPU voxelisation, no dynamic
re-voxelisation, two of the three filtering passes) and benchmark results are
documented in [final-project/docs/](final-project/docs/).

<p align="center">
  <a href="https://youtu.be/weibWkdd3cc"><img src="https://img.youtube.com/vi/weibWkdd3cc/maxresdefault.jpg" alt="Final project video"></a>
</p>

## Building

Each lab and the final project is a standalone Visual Studio solution
(`labN.sln` / `final-project.sln`). Open in Visual Studio and build the x64
configuration. Shared property sheet: [PropertySheet.props](PropertySheet.props).
