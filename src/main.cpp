#include <iostream>
#include <curl/curl.h>
#include <string>
#include <sstream>
#include <fstream>

#include "rapidjson/document.h"
#include "geojson.h"
#include "tileData.h"
#include "earcut.hpp"

namespace mapbox { namespace util {
template <>
struct nth<0, Point> {
    inline static float get(const Point &t) { return t.x; };
};
template <>
struct nth<1, Point> {
    inline static float get(const Point &t) { return t.y; };
};
}}

struct PolygonVertex {
    glm::vec3 position;
    glm::vec3 normal;
};

struct PolygonMesh {
    std::vector<unsigned int> indices;
    std::vector<PolygonVertex> vertices;
};

static size_t write_data(void *_ptr, size_t _size, size_t _nmemb, void *_stream) {
    ((std::stringstream*) _stream)->write(reinterpret_cast<char *>(_ptr), _size * _nmemb);
    return _size * _nmemb;
}

std::unique_ptr<TileData> downloadTile(const std::string& _url, const Tile& _tile) {
    bool success = true;

    CURL* curlHandle = curl_easy_init();

    std::stringstream out;

    // set up curl to perform fetch
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(curlHandle, CURLOPT_URL, _url.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_HEADER, 0L);
    curl_easy_setopt(curlHandle, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(curlHandle, CURLOPT_ACCEPT_ENCODING, "gzip");
    curl_easy_setopt(curlHandle, CURLOPT_NOSIGNAL, 1L);

    std::cout << "Fetching URL with curl: " << _url << std::endl;

    CURLcode result = curl_easy_perform(curlHandle);

    if (result != CURLE_OK) {
        std::cout << "curl_easy_perform failed: " << curl_easy_strerror(result) << std::endl;
        success = false;
    } else {
        std::cout << "Fetched tile: " << _url << std::endl;

        // parse written data into a JSON object
        rapidjson::Document doc;
        doc.Parse(out.str().c_str());

        if (doc.HasParseError()) {
            std::cout << "Error parsing" << std::endl;
            return nullptr;
        }

        std::unique_ptr<TileData> data = std::make_unique<TileData>();
        for (auto layer = doc.MemberBegin(); layer != doc.MemberEnd(); ++layer) {
            std::cout << "Extracting layer " << layer->name.GetString() << std::endl;
            data->layers.emplace_back(std::string(layer->name.GetString()));
            GeoJson::extractLayer(layer->value, data->layers.back(), _tile);
        }

        return std::move(data);
    }

    return nullptr;
}

void buildPolygonExtrusion(const Polygon& _polygon, double _minHeight, double _height,
    std::vector<PolygonVertex>& _vertices, std::vector<unsigned int>& _indices)
{

}

void buildPolygon(const Polygon& _polygon, double _height, std::vector<PolygonVertex>& _vertices,
    std::vector<unsigned int>& _indices)
{
    mapbox::Earcut<float, unsigned int> earcut;

    earcut(_polygon);

    unsigned int vertexDataOffset = _vertices.size();

    if (vertexDataOffset == 0) {
        _indices = std::move(earcut.indices);
    } else {
        _indices.reserve(_indices.size() + earcut.indices.size());
        for (auto i : earcut.indices) {
            _indices.push_back(vertexDataOffset + i);
        }
    }
    static glm::vec3 normal(0.0, 0.0, 1.0);

    for (auto& p : earcut.vertices) {
        glm::vec3 coord(p[0], p[1], _height);
        _vertices.push_back({coord, normal});
    }
}

int main() {
    int tileX = 19294;
    int tileY = 24642;
    int tileZ = 16;

    std::string apiKey = "vector-tiles-qVaBcRA";
    std::string url = "http://vector.mapzen.com/osm/all/"
        + std::to_string(tileZ) + "/"
        + std::to_string(tileX) + "/"
        + std::to_string(tileY) + ".json?api_key=" + apiKey;

    curl_global_init(CURL_GLOBAL_DEFAULT);

    Tile tile = {tileX, tileY, tileZ};
    auto data = downloadTile(url, tile);

    glm::dvec4 bounds = tileBounds(tile, 256.0);
    double scale = 0.5 * glm::abs(bounds.x - bounds.z);
    double invScale = 1.0 / scale;

    const static std::string key_height("height");
    const static std::string key_min_height("min_height");

    std::vector<PolygonMesh> meshes;
    for (auto layer : data->layers) {
        if (layer.name == "buildings" || layer.name == "landuse") {
            for (auto feature : layer.features) {
                auto itHeight = feature.props.numericProps.find(key_height);
                auto itMinHeight = feature.props.numericProps.find(key_min_height);
                double height = 0.0;
                double minHeight = 0.0;
                if (itHeight != feature.props.numericProps.end()) {
                    height = itHeight->second * invScale;
                }
                if (itMinHeight != feature.props.numericProps.end()) {
                    minHeight = itMinHeight->second * invScale;
                }
                PolygonMesh mesh;
                for (auto polygon : feature.polygons) {
                    if (minHeight != height) {
                        buildPolygonExtrusion(polygon, minHeight, height,
                            mesh.vertices, mesh.indices);
                    }
                    buildPolygon(polygon, height, mesh.vertices, mesh.indices);
                }
                meshes.push_back(mesh);
            }
        }
    }

    std::ofstream file(std::to_string(tileX) + "." + std::to_string(tileY)
        + "." + std::to_string(tileZ) + ".obj");

    if (file.is_open()) {
        int i = 0;
        int indexOffset = 0;
        for (auto mesh : meshes) {
            file << "o [mesh" << i++ << "]\n";
            for (auto vertex : mesh.vertices) {
                file << "v " << vertex.position.x << " "
                    << vertex.position.y << " "
                    << vertex.position.z << "\n";
            }
            for (auto vertex : mesh.vertices) {
                file << "vn " << vertex.normal.x << " "
                    << vertex.normal.y << " "
                    << vertex.normal.z << "\n";
            }
            for (int i = 0; i < mesh.indices.size(); i += 3) {
                file << "f " << mesh.indices[i] + indexOffset + 1 << "/"
                    << mesh.indices[i] + indexOffset + 1 << "/"
                    << mesh.indices[i] + indexOffset + 1;
                file << " ";
                file << mesh.indices[i+1] + indexOffset + 1 << "/"
                    << mesh.indices[i+1] + indexOffset + 1 << "/"
                    << mesh.indices[i+1] + indexOffset + 1;
                file << " ";
                file << mesh.indices[i+2] + indexOffset + 1 << "/"
                    << mesh.indices[i+2] + indexOffset + 1 << "/"
                    << mesh.indices[i+2] + indexOffset + 1<< "\n";
            }
            indexOffset += mesh.vertices.size();
            file << "\n";
        }
        file.close();
    }
}
