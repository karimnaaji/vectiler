#pragma once

#include "glm/glm.hpp"
#include "util.h"
#include <cmath>
#include <functional>

#define R_EARTH 6378137.0
#define PI M_PI
constexpr static double INV_360 = 1.0 / 360.0;
constexpr static double INV_180 = 1.0 / 180.0;
constexpr static double HALF_CIRCUMFERENCE = PI * R_EARTH;

glm::dvec2 lonLatToMeters(const glm::dvec2 _lonLat);
glm::dvec2 pixelsToMeters(const glm::dvec2 _pix, const int _zoom, double _invTileSize);
glm::dvec4 tileBounds(int x, int y, int z, double _tileSize);
glm::dvec2 tileCenter(int x, int y, int z, double _tileSize);

struct Tile {
    int x;
    int y;
    int z;

    double invScale = 0.0;
    glm::dvec2 tileOrigin;

    bool operator==(const Tile& rhs) const {
        return x == rhs.x && y == rhs.y && z == rhs.z;
    }

    Tile(int x, int y, int z) : x(x), y(y), z(z) {
        glm::dvec4 bounds = tileBounds(x, y, z, 256.0);
        tileOrigin = glm::dvec2(0.5 * (bounds.x + bounds.z), -0.5 * (bounds.y + bounds.w));
        double scale = 0.5 * glm::abs(bounds.x - bounds.z);
        invScale = 1.0 / scale;
    }
};

namespace std {
    template<>
    struct hash<Tile> {
        size_t operator()(const Tile &tile) const {
            return std::hash<int>()(tile.x) ^ std::hash<int>()(tile.y) ^ std::hash<int>()(tile.z);
        }
    };
}
