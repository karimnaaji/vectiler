#include "geoJson.h"
#include "glm/glm.hpp"

void GeoJson::extractPoint(const rapidjson::Value& _in, Point& _out, const Tile& _tile) {
    glm::dvec4 bounds = tileBounds(_tile, 256.0);
    glm::dvec2 tileOrigin = glm::dvec2(0.5 * (bounds.x + bounds.z), -0.5 * (bounds.y + bounds.w));
    double scale = 0.5 * glm::abs(bounds.x - bounds.z);
    double invScale = 1.0 / scale;
    glm::vec2 pos = lonLatToMeters(glm::dvec2(_in[0].GetDouble(), _in[1].GetDouble()));
    _out.x = (pos.x - tileOrigin.x) * invScale;
    _out.y = (pos.y - tileOrigin.y) * invScale;
}

void GeoJson::extractLine(const rapidjson::Value& _in, Line& _out, const Tile& _tile) {
    for (auto itr = _in.Begin(); itr != _in.End(); ++itr) {
        _out.emplace_back();
        extractPoint(*itr, _out.back(), _tile);
    }
}

void GeoJson::extractPoly(const rapidjson::Value& _in, Polygon& _out, const Tile& _tile) {
    for (auto itr = _in.Begin(); itr != _in.End(); ++itr) {
        _out.emplace_back();
        extractLine(*itr, _out.back(), _tile);
    }
}

void GeoJson::extractFeature(const rapidjson::Value& _in, Feature& _out, const Tile& _tile) {
    const rapidjson::Value& properties = _in["properties"];

    for (auto itr = properties.MemberBegin(); itr != properties.MemberEnd(); ++itr) {
        const auto& member = itr->name.GetString();
        const rapidjson::Value& prop = properties[member];

        if (strcmp(member, "height") == 0) {
            _out.props.numericProps[member] = prop.GetDouble();
            continue;
        }

        if (strcmp(member, "min_height") == 0) {
            _out.props.numericProps[member] = prop.GetDouble();
            continue;
        }

        if (prop.IsNumber()) {
            //_out.props.numericProps[member] = prop.GetDouble();
        } else if (prop.IsString()) {
            //_out.props.stringProps[member] = prop.GetString();
        }
    }

    // Copy geometry into tile data
    const rapidjson::Value& geometry = _in["geometry"];
    const rapidjson::Value& coords = geometry["coordinates"];
    const std::string& geometryType = geometry["type"].GetString();

    if (geometryType == "Point") {
        _out.geometryType = GeometryType::points;
        _out.points.emplace_back();
        extractPoint(coords, _out.points.back(), _tile);
    } else if (geometryType == "MultiPoint") {
        _out.geometryType= GeometryType::points;
        for (auto pointCoords = coords.Begin(); pointCoords != coords.End(); ++pointCoords) {
            extractPoint(*pointCoords, _out.points.back(), _tile);
        }
    } else if (geometryType == "LineString") {
        _out.geometryType = GeometryType::lines;
        _out.lines.emplace_back();
        extractLine(coords, _out.lines.back(), _tile);
    } else if (geometryType == "MultiLineString") {
        _out.geometryType = GeometryType::lines;
        for (auto lineCoords = coords.Begin(); lineCoords != coords.End(); ++lineCoords) {
            _out.lines.emplace_back();
            extractLine(*lineCoords, _out.lines.back(), _tile);
        }
    } else if (geometryType == "Polygon") {
        _out.geometryType = GeometryType::polygons;
        _out.polygons.emplace_back();
        extractPoly(coords, _out.polygons.back(), _tile);
    } else if (geometryType == "MultiPolygon") {
        _out.geometryType = GeometryType::polygons;
        for (auto polyCoords = coords.Begin(); polyCoords != coords.End(); ++polyCoords) {
            _out.polygons.emplace_back();
            extractPoly(*polyCoords, _out.polygons.back(), _tile);
        }
    }
}

void GeoJson::extractLayer(const rapidjson::Value& _in, Layer& _out, const Tile& _tile) {
    const auto& featureIter = _in.FindMember("features");

    if (featureIter == _in.MemberEnd()) {
        printf("ERROR: GeoJSON missing 'features' member\n");
        return;
    }

    const auto& features = featureIter->value;
    for (auto featureJson = features.Begin(); featureJson != features.End(); ++featureJson) {
        _out.features.emplace_back();
        extractFeature(*featureJson, _out.features.back(), _tile);
    }
}
