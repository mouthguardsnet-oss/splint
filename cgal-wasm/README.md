# CGAL WASM experiment

This is the first CGAL step for the splint app. It does not replace the current generator yet.

The goal of this first module is deliberately small:

- load the STL triangles into `CGAL::Surface_mesh`
- remove degenerate faces and stitch simple borders
- report diagnostics such as invalid faces, open borders and self-intersections
- project the drawn margin points back onto the real mesh surface using a CGAL AABB tree

Once this is reliable on real scans, the next step is generating the splint surface from the repaired mesh and projected margin.

## Why this is separate

CGAL cannot be dropped into a normal static HTML file as a script. It has to be compiled from C++ to WebAssembly with Emscripten. The final `.js` and `.wasm` files are static files, so they can be uploaded to ordinary hosting together with `splint-v6.html`.

The hosting must serve `.wasm` files as `application/wasm`. Most normal static hosts already do this.

## Build

This machine currently does not have the C++/Emscripten toolchain installed, so the intended first build path is GitHub Actions:

1. Push this repository to GitHub.
2. Open the `CGAL WASM` workflow.
3. Run it manually or push to the branch.
4. Download the artifact containing `cgal-splint.js` and `cgal-splint.wasm`.

Local build later, on a machine with Emscripten and CGAL available:

```powershell
emcmake cmake -S cgal-wasm -B build-cgal-wasm -DCMAKE_BUILD_TYPE=Release
cmake --build build-cgal-wasm --config Release
```

## License note

CGAL is not just a small permissive JavaScript dependency. Before using this commercially, review the exact licenses of every CGAL package used by the final generator.
