# obj-export

A wavefront obj builder and exporter based on [mapzen](https://mapzen.com) [vector tiles](https://mapzen.com/projects/vector-tiles).

- build and export mesh in obj files based on tile coordinates (find your tiles of interest [here](http://www.maptiler.org/google-maps-coordinates-tile-bounds-projection/))
- bake ambiant occlusion and save it in an atlas (using [aobaker](https://github.com/prideout/aobaker))

![](img/capture.png)

**fetch submodules**

First initialize submodules by running:
```sh
$ git submodule update --init --recursive
```

**Install dependecies (OS X)**

aobaker needs [embree](https://embree.github.io/) for optimal ray tracing and tbb, you can install it using [homebrew](http://brew.sh/):

```sh
$ cp 3rdparty/aobaker/embree.rb /usr/local/Library/Formula
$ brew install embree tbb cmake
```

**build**

To build with cmake in a `build/` directory run the following:
```sh
$ cmake . -Bbuild
$ cmake --build build
```

**usage**

```
  Usage: ./objexport [options]

  Options:
    --splitMeshes  Generate one mesh per feature in wavefront file
    --tilex        Tile X (19294)
    --tiley        Tile Y (24642)
    --tilez        Tile Z (16)
    --sizehint     Controls resolution of atlas (512)
    --nsamples     Quality of ambient occlusion (256)
    --version      Output version
    --help         Output help
```

**Example**
```sh
./objexport --tilex 19294 --tiley 24642 --tilez 16 --sizehint 512 --nsamples 128
```
Output is:
- `[tilex].[tiley].[tilez].obj`: a simple mesh containing coordinates and normals
- `[tilex].[tiley].[tilez]-ao.obj` a mesh containing coordinates and texture uvs for ambiant occlusion rendering
- `[tilex].[tiley].[tilez].png`: an atlas containing the baked ambiant occlusion.

**build and run the viewer (OS X)**

A minimal viewer using [oglw](https://github.com/karimnaaji/oglw) can be used to preview the obj tiles:

```sh
$ cd renderer
$ cmake . -Bbuild
$ cmake --build build
$ open viewer.app --args ~/dev/obj-export/build/19294.24642.16-ao.obj ~/dev/obj-export/build/19294.24642.16.png
```

