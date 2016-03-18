#include <iostream>
#include <curl/curl.h>
#include <string>
#include <sstream>
#include <fstream>

#include "rapidjson/document.h"
#include "geojson.h"
#include "tileData.h"
#include "earcut.hpp"
#include "objexport.h"
#include "aobaker.h"

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
    curl_global_init(CURL_GLOBAL_DEFAULT);

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

    printf("Fetching URL with curl: %s\n", _url.c_str());

    CURLcode result = curl_easy_perform(curlHandle);

    if (result != CURLE_OK) {
        printf("curl_easy_perform failed: %s\n", curl_easy_strerror(result));
        success = false;
    } else {
        printf("Fetched tile: %s\n", _url.c_str());

        // parse written data into a JSON object
        rapidjson::Document doc;
        doc.Parse(out.str().c_str());

        if (doc.HasParseError()) {
            printf("Error parsing tile\n");
            return nullptr;
        }

        std::unique_ptr<TileData> data = std::unique_ptr<TileData>(new TileData());
        for (auto layer = doc.MemberBegin(); layer != doc.MemberEnd(); ++layer) {
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
    int vertexDataOffset = _vertices.size();
    glm::vec3 upVector(0.0f, 0.0f, 1.0f);
    glm::vec3 normalVector;

    for (auto& line : _polygon) {
        size_t lineSize = line.size();
        for (size_t i = 0; i < lineSize - 1; i++) {
            glm::vec3 a(line[i]);
            glm::vec3 b(line[i+1]);

            normalVector = glm::cross(upVector, b - a);
            normalVector = glm::normalize(normalVector);
            a.z = _height;
            _vertices.push_back({a, normalVector});
            b.z = _height;
            _vertices.push_back({b, normalVector});
            a.z = _minHeight;
            _vertices.push_back({a, normalVector});
            b.z = _minHeight;
            _vertices.push_back({b, normalVector});

            _indices.push_back(vertexDataOffset);
            _indices.push_back(vertexDataOffset + 1);
            _indices.push_back(vertexDataOffset + 2);
            _indices.push_back(vertexDataOffset + 1);
            _indices.push_back(vertexDataOffset + 3);
            _indices.push_back(vertexDataOffset + 2);

            vertexDataOffset += 4;
        }
    }
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

bool saveOBJ(std::string _outputOBJ,
    bool _splitMeshes,
    const std::vector<PolygonMesh>& _meshes,
    float _offsetX,
    float _offsetY,
    bool _append,
    Tile _tile)
{
    std::ofstream file;

    if (_append) {
        file = std::ofstream(_outputOBJ, std::ios_base::app);
    } else {
        file = std::ofstream(_outputOBJ);
    }

    if (file.is_open()) {
        int indexOffset = 0;

        if (_splitMeshes) {
            int meshCnt = 0;

            for (const PolygonMesh& mesh : _meshes) {
                if (mesh.vertices.size() == 0) { continue; }
                file << "# tile " << _tile.x << " " << _tile.y << " " << _tile.z << "\n";

                file << "o mesh" << meshCnt++ << "\n";
                for (auto vertex : mesh.vertices) {
                    file << "v " << vertex.position.x + _offsetX << " "
                         << vertex.position.y + _offsetY << " "
                         << vertex.position.z << "\n";
                }
                for (auto vertex : mesh.vertices) {
                    file << "vn " << vertex.normal.x << " "
                         << vertex.normal.y << " "
                         << vertex.normal.z << "\n";
                }
                for (int i = 0; i < mesh.indices.size(); i += 3) {
                    file << "f " << mesh.indices[i] + indexOffset + 1 << "//"
                         << mesh.indices[i] + indexOffset + 1;
                    file << " ";
                    file << mesh.indices[i+1] + indexOffset + 1 << "//"
                         << mesh.indices[i+1] + indexOffset + 1;
                    file << " ";
                    file << mesh.indices[i+2] + indexOffset + 1 << "//"
                         << mesh.indices[i+2] + indexOffset + 1 << "\n";
                }
                file << "\n";
                indexOffset += mesh.vertices.size();
            }
        } else {
            file << "o tile_" << _tile.x << "_" << _tile.y << "_" << _tile.z << "\n";

            for (const PolygonMesh& mesh : _meshes) {
                if (mesh.vertices.size() == 0) { continue; }

                for (auto vertex : mesh.vertices) {
                    file << "v " << vertex.position.x + _offsetX << " "
                         << vertex.position.y + _offsetY << " "
                         << vertex.position.z << "\n";
                }
            }
            for (const PolygonMesh& mesh : _meshes) {
                if (mesh.vertices.size() == 0) { continue; }

                for (auto vertex : mesh.vertices) {
                    file << "vn " << vertex.normal.x << " "
                         << vertex.normal.y << " "
                         << vertex.normal.z << "\n";
                }
            }
            for (const PolygonMesh& mesh : _meshes) {
                if (mesh.vertices.size() == 0) { continue; }

                for (int i = 0; i < mesh.indices.size(); i += 3) {
                    file << "f " << mesh.indices[i] + indexOffset + 1 << "//"
                         << mesh.indices[i] + indexOffset + 1;
                    file << " ";
                    file << mesh.indices[i+1] + indexOffset + 1 << "//"
                         << mesh.indices[i+1] + indexOffset + 1;
                    file << " ";
                    file << mesh.indices[i+2] + indexOffset + 1 << "//"
                         << mesh.indices[i+2] + indexOffset + 1 << "\n";
                }
                indexOffset += mesh.vertices.size();
            }
        }

        file.close();
        printf("Save %s\n", _outputOBJ.c_str());
        return true;
    } else {
        printf("Can't open file %s", _outputOBJ.c_str());
    }
    return false;
}

int objexport(int _tileX,
    int _tileY,
    int _tileZ,
    float _offsetX,
    float _offsetY,
    bool _splitMeshes,
    int _sizehint,
    int _nsamples,
    bool _bakeAO,
    bool _append)
{
    std::string apiKey = "vector-tiles-qVaBcRA";
    std::string url = "http://vector.mapzen.com/osm/all/"
        + std::to_string(_tileZ) + "/"
        + std::to_string(_tileX) + "/"
        + std::to_string(_tileY) + ".json?api_key=" + apiKey;

    Tile tile = {_tileX, _tileY, _tileZ};
    auto data = downloadTile(url, tile);

    if (!data) {
        printf("Failed to download tile data\n");
        return EXIT_FAILURE;
    }

    glm::dvec4 bounds = tileBounds(tile, 256.0);
    double scale = 0.5 * glm::abs(bounds.x - bounds.z);
    double invScale = 1.0 / scale;

    const static std::string key_height("height");
    const static std::string key_min_height("min_height");

    std::vector<PolygonMesh> meshes;
    for (auto layer : data->layers) {
        //if (layer.name == "buildings" || layer.name == "landuse") {
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
        //}
    }

    std::string outFile = std::to_string(_tileX) + "." + std::to_string(_tileY)
        + "." + std::to_string(_tileZ);
    std::string outputOBJ = outFile + ".obj";

    if (!saveOBJ(outputOBJ, _splitMeshes, meshes, _offsetX, _offsetY, _append, tile)) {
        return EXIT_FAILURE;
    }

    if (_bakeAO) {
        return aobaker_bake(outputOBJ.c_str(), (outFile + "-ao.obj").c_str(),
                (outFile + ".png").c_str(), _sizehint, _nsamples, false, false, 1.0);
    }

    return EXIT_SUCCESS;
}
