# obj-export

Wavefront .obj exporter based on Mapzen GeoJSON tile data. 

# Build

```
git submodule init --update
mkdir build
cd build
cmake ..
make
```

# Usage

```bash
./obj-export x y z
```

# Options

`-t,--tile x y z` Download tile `x`, `y`, `z`

`-T,--tiles n_tile x0 y0 x1 y1 ... xn-1, yn-1 z` Download `n_tile` with the specified coordinates at zoom level `z`

`-T,--tiles [x0-x1 y0-y1] `Download the range of tiles defined by the square `x0y0`, `x0y1`, `x1y1`, `x1y0` 

# Pipeline 

- [ ] Download the tile
- [ ] Normalize the positions from lat/lon to [-1.0 1.0] relative to tile coordinate
- [ ] Parse the features and store them in a data structure
- [ ] For each feature, tesselate the normalized polygons / lines (using libtess2)
- [ ] For each polygon extrude if a height is available
- [ ] Offset the vertices relative to the "origin" tile, saying that if a set of 3 tiles `[x0,y0]``[x0,y1]``[x1,y0]`
are downloaded, positions for tiles `[x0,y1]` and `[x1,y0]` are offseted by `[1,0]` and `[0,1]` respectively
- [ ] Export the data to .obj
