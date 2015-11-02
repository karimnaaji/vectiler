#include "util.h"

glm::dvec2 lonLatToMeters(const glm::dvec2 _lonLat) {
    glm::dvec2 meters;
    meters.x = _lonLat.x * HALF_CIRCUMFERENCE * INV_180;
    meters.y = log(tan(PI * 0.25 + _lonLat.y * PI * INV_360)) * (double)R_EARTH;
    return meters;
}

glm::dvec4 tileBounds(const Tile& _tile, double _tileSize) {
    return glm::dvec4(
        pixelsToMeters({ _tile.x * _tileSize, _tile.y * _tileSize }, _tile.z, 1.0 / _tileSize),
        pixelsToMeters({ (_tile.x + 1) * _tileSize, (_tile.y + 1) * _tileSize }, _tile.z, 1.0 / _tileSize)
    );
}

glm::dvec2 pixelsToMeters(const glm::dvec2 _pix, const int _zoom, double _invTileSize) {
    glm::dvec2 meters;
    double res = (2.0 * HALF_CIRCUMFERENCE * _invTileSize) / (1 << _zoom);
    meters.x = _pix.x * res - HALF_CIRCUMFERENCE;
    meters.y = _pix.y * res - HALF_CIRCUMFERENCE;
    return meters;
}

glm::dvec2 tileCenter(const Tile& _tile, double _tileSize) {
    return pixelsToMeters(glm::dvec2(_tile.x * _tileSize + _tileSize * 0.5,
                                    (_tile.y * _tileSize + _tileSize * 0.5)),
                                     _tile.z, 1.0 / _tileSize);
}