#pragma once
#include "glm/glm.hpp"
#include <string>
#include <map>

struct Properties {
    std::map<std::string, double> numericProps;
};

enum GeometryType {
    unknown,
    points,
    lines,
    polygons
};

typedef glm::vec3 Point;
typedef std::vector<Point> Line;
typedef std::vector<Line> Polygon;

struct Feature {
    GeometryType geometryType = GeometryType::polygons;

    std::vector<Point> points;
    std::vector<Line> lines;
    std::vector<Polygon> polygons;

    Properties props;
};

struct Layer {

    Layer(const std::string& _name) : name(_name) {}

    std::string name;
    std::vector<Feature> features;
};

struct TileData {
    std::vector<Layer> layers;
};