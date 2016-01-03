#pragma once
#include "glm/glm.hpp"
#include <cmath>

#define R_EARTH 6378137.0
#define PI M_PI
constexpr static double INV_360 = 1.0 / 360.0;
constexpr static double INV_180 = 1.0 / 180.0;
constexpr static double HALF_CIRCUMFERENCE = PI * R_EARTH;

struct Tile {
    int x;
    int y;
    int z;
};

glm::dvec2 lonLatToMeters(const glm::dvec2 _lonLat);
glm::dvec2 pixelsToMeters(const glm::dvec2 _pix, const int _zoom, double _invTileSize);
glm::dvec4 tileBounds(const Tile& _tile, double _tileSize);
glm::dvec2 tileCenter(const Tile& _tile, double _tileSize);
