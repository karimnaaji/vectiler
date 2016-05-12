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
