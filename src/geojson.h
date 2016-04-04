#pragma once

#include <vector>
#include <string>
#include <iostream>

#include "rapidjson/document.h"
#include "tiledata.h"
#include "projection.h"

namespace GeoJson {
    void extractPoint(const rapidjson::Value& _in, Point& _out, const Tile& _tile);
    void extractLine(const rapidjson::Value& _in, Line& _out, const Tile& _tile);
    void extractPoly(const rapidjson::Value& _in, Polygon& _out, const Tile& _tile);
    void extractFeature(const rapidjson::Value& _in, Feature& _out, const Tile& _tile);
    void extractLayer(const rapidjson::Value& _in, Layer& _out, const Tile& _tile);
}
