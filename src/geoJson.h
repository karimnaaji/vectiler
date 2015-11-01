#pragma once

#include <vector>
#include <string>
#include <iostream>

#include "rapidjson/document.h"

namespace GeoJson {
    
    void extractPoint(const rapidjson::Value& _in);
    
    void extractLine(const rapidjson::Value& _in);
    
    void extractPoly(const rapidjson::Value& _in);
    
    void extractFeature(const rapidjson::Value& _in);
    
    void extractLayer(const rapidjson::Value& _in);
    
}