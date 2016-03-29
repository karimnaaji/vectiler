#pragma once

#include "earcut.hpp"

namespace mapbox {
namespace util {
template <>
struct nth<0, Point> {
    inline static float get(const Point &t) { return t.x; };
};

template <>
struct nth<1, Point> {
    inline static float get(const Point &t) { return t.y; };
};
}}