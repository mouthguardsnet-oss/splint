# CGAL WASM experiment

This is the first CGAL step for the splint app. It does not replace the current generator yet.

The goal of this first module is deliberately small:

- load the STL triangles into `CGAL::Surface_mesh`
- remove degenerate faces and stitch simple borders
- report diagnostics such as invalid faces and open borders
- project the drawn margin points back onto the real mesh surface using a CGAL AABB tree

Once this is reliable on real scans, the next step is generating the splint surface from the repaired mesh and projected margin.
Self-intersection diagnostics are intentionally left out of the first WASM build to avoid pulling heavier CGAL exact-arithmetic packages before the basic bridge works.

## Why this is separate

CGAL cannot be dropped into a normal static HTML file as a script. It has to be compiled from C++ to WebAssembly with Emscripten. The final `.js` and `.wasm` files are static files, so they can be uploaded to ordinary hosting together with `splint-v6.html`.

The hosting must serve `.wasm` files as `application/wasm`. Most normal static hosts already do this.

## Build

This machine currently does not have the C++/Emscripten toolchain installed, so the intended first build path is GitHub Actions:

1. Push this repository to GitHub.
2. Open the `CGAL WASM` workflow.
3. Run it manually or push to the branch.
4. Download the artifact containing `cgal-splint.js` and `cgal-splint.wasm`.

Local build later, on a machine with Emscripten and CGAL headers available:

```powershell
emcmake cmake -S cgal-wasm -B build-cgal-wasm -DCMAKE_BUILD_TYPE=Release -DCGAL_ROOT=C:\path\to\cgal -DBOOST_INCLUDE_DIR=C:\path\to\boost
cmake --build build-cgal-wasm --config Release
```

For Emscripten, do not pass a full system include path such as `/usr/include`.
That can make the compiler pick native Linux C/C++ headers instead of Emscripten's sysroot headers.
Use a copied/downloaded Boost header folder instead, where the folder contains `boost/config.hpp`.

## GitHub web steps for first-time use

If you do not use Git locally yet, upload these paths through the GitHub website:

- `splint-v6.html`
- `cgal-wasm/CMakeLists.txt`
- `cgal-wasm/src/cgal_splint.cpp`
- `cgal-wasm/js/cgal-splint-client.js`
- `cgal-wasm/README.md`
- `.github/workflows/cgal-wasm.yml`

For the workflow file, GitHub must see the exact path `.github/workflows/cgal-wasm.yml`.
If the `Actions` tab does not show `CGAL WASM`, this file is missing or in the wrong folder.

After upload:

1. Open the repository on GitHub.
2. Click `Actions`.
3. If GitHub asks, enable workflows for the repository.
4. Click `CGAL WASM`.
5. Click `Run workflow`.
6. Wait until the run is green.
7. Open the finished run and download the artifact named `cgal-splint-wasm`.
8. Extract `cgal-splint.js` and `cgal-splint.wasm`.

If the run is red, open it, click the failed step, and copy the error log. The first CGAL WASM build may need one or two dependency adjustments.

## License note

CGAL is not just a small permissive JavaScript dependency. Before using this commercially, review the exact licenses of every CGAL package used by the final generator.
