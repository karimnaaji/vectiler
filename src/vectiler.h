#ifndef VECTILER_H
#define VECTILER_H

#ifdef __cplusplus

#include <unordered_map>
#include <vector>
#include "tiledata.h"
#include "glm/glm.hpp"

#endif

struct DownloadParams {
    const char* apiKey;
    const char* tilex;
    const char* tiley;
    int tilez;
    bool terrain;
    bool roads;
    bool buildings;
};

struct Params {
    const char* filename;
    bool splitMesh;
    int aoSizeHint;
    int aoSamples;
    bool aoBaking;
    bool append;
    int terrainSubdivision;
    float terrainExtrusionScale;
    float buildingsExtrusionScale;
    float roadsHeight;
    float roadsExtrusionWidth;
    bool normals;

    struct DownloadParams download;
};

#ifdef __cplusplus

struct Tile {
    int x, y, z;
    double invScale = 0.0;
    glm::dvec2 tileOrigin;

    bool operator==(const Tile& rhs) const {
        return x == rhs.x && y == rhs.y && z == rhs.z;
    }

    Tile(int x, int y, int z);
};

namespace std {
    template<>
    struct hash<Tile> {
        size_t operator()(const Tile &tile) const {
            return std::hash<int>()(tile.x) ^ std::hash<int>()(tile.y) ^ std::hash<int>()(tile.z);
        }
    };
}

struct PolygonVertex {
    glm::vec3 position;
    glm::vec3 normal;
};

struct HeightData {
    std::vector<std::vector<float>> elevation;
    int width;
    int height;
};

struct PolygonMesh {
    std::vector<unsigned int> indices;
    std::vector<PolygonVertex> vertices;
    glm::vec2 offset;
};

bool vectiler_download(DownloadParams parameters,
    const std::vector<Tile> tiles,
    std::unordered_map<Tile, std::unique_ptr<HeightData>>& heightData,
    std::unordered_map<Tile, std::unique_ptr<TileData>>& vectorTileData);

std::vector<std::unique_ptr<PolygonMesh>> vectiler_build(Params params,
    std::unordered_map<Tile, std::unique_ptr<HeightData>>& heightData,
    std::unordered_map<Tile, std::unique_ptr<TileData>>& vectorTileData,
    const std::vector<Tile>& tiles);

bool vectiler_export(Params params,
    const std::vector<std::unique_ptr<PolygonMesh>>& meshes,
    std::vector<Tile> tiles);

bool vectiler_parse_tile_range(const std::string& tilex,
    const std::string& tiley,
    int tilez,
    std::vector<Tile>& tiles);

extern "C" {
#endif

int vectiler(struct Params parameters);

#ifdef __cplusplus
}
#endif

#ifdef VECTILER_UNIT_TESTS
#include "tiledata.h"
#endif

#endif
