#include <iostream>
#include <curl/curl.h>
#include <string>
#include <sstream>
#include <fstream>
#include <limits>

#include "rapidjson/document.h"
#include "geojson.h"
#include "tileData.h"
#include "objexport.h"
#include "aobaker.h"
#include "earcut.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

struct PolygonVertex {
    glm::vec3 position;
    glm::vec3 normal;
};

struct HeightData {
    std::vector<std::vector<float>> elevation;
    int width;
    int height;
};

struct PolygonMesh {
    std::vector<unsigned int> indices;
    std::vector<PolygonVertex> vertices;
};

static size_t write_data(void *_ptr, size_t _size, size_t _nmemb, void *_stream) {
    ((std::stringstream*) _stream)->write(reinterpret_cast<char *>(_ptr), _size * _nmemb);
    return _size * _nmemb;
}

bool downloadData(std::stringstream& _out, const std::string& _url) {
    static bool curlInitialized = false;
    if (!curlInitialized) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curlInitialized = true;
    }

    CURL* curlHandle = curl_easy_init();

    // set up curl to perform fetch
    curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &_out);
    curl_easy_setopt(curlHandle, CURLOPT_URL, _url.c_str());
    curl_easy_setopt(curlHandle, CURLOPT_HEADER, 0L);
    curl_easy_setopt(curlHandle, CURLOPT_VERBOSE, 0L);
    curl_easy_setopt(curlHandle, CURLOPT_ACCEPT_ENCODING, "gzip");
    curl_easy_setopt(curlHandle, CURLOPT_NOSIGNAL, 1L);

    printf("Fetching URL with curl: %s\n", _url.c_str());

    CURLcode result = curl_easy_perform(curlHandle);

    if (result != CURLE_OK) {
        printf("Curl download failure: %s\n", curl_easy_strerror(result));
    } else {
        printf("Downloaded data from url: %s\n", _url.c_str());
    }

    return result == CURLE_OK;
}

std::unique_ptr<HeightData> downloadHeightmapTile(const std::string& _url, const Tile& _tile) {
    std::stringstream out;

    if (downloadData(out, _url)) {
        unsigned char* pixels;
        int width, height, comp;

        pixels = stbi_load_from_memory(reinterpret_cast<const unsigned char*>(out.str().c_str()),
            out.str().length(), &width, &height, &comp, STBI_rgb_alpha);

        printf("Texture data %d width, %d height, %d comp\n", width, height, comp);

        assert(comp == STBI_rgb_alpha);

        std::unique_ptr<HeightData> data = std::unique_ptr<HeightData>(new HeightData());

        data->elevation.resize(height);
        for (int i = 0; i < height; ++i) {
            data->elevation[i].resize(width);
        }

        data->width = width;
        data->height = height;

        unsigned char* pixel = pixels;
        for (size_t i = 0; i < width * height; i++, pixel += 4) {
            float red =   *(pixel+0);
            float green = *(pixel+1);
            float blue =  *(pixel+2);

            // Decode the elevation packed data from color component
            float elevation = (red * 256 + green + blue / 256) - 32768;

            int x = i / height;
            int y = i % width;

            assert(x >= 0 && x <= height && y >= 0 && y <= width);

            data->elevation[x][y] = elevation;
        }

        return std::move(data);
    }

    return nullptr;
}

std::unique_ptr<TileData> downloadTile(const std::string& _url, const Tile& _tile) {
    std::stringstream out;

    if (downloadData(out, _url)) {
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

void buildPlane(std::vector<PolygonVertex>& _vertices,
    std::vector<unsigned int>& _indices,
    float _width,       // Total plane width (x-axis)
    float _height,      // Total plane height (y-axis)
    unsigned int _nw,   // Split on width
    unsigned int _nh)   // Split on height
{
    std::vector<glm::vec4> vertices;
    std::vector<int> indices;

    int indexOffset = 0;

    float ow = _width / _nw;
    float oh = _height / _nh;

    for (float w = -_width / 2.0; w <= _width / 2.0 - ow; w += ow) {
        for (float h = -_height / 2.0; h <= _height / 2.0 - oh; h += oh) {
            glm::vec3 v0(w, h + oh, 0.0);
            glm::vec3 v1(w, h, 0.0);
            glm::vec3 v2(w + ow, h, 0.0);
            glm::vec3 v3(w + ow, h + oh, 0.0);

            static const glm::vec3 up(0.0, 0.0, 1.0);

            _vertices.push_back({v0, up});
            _vertices.push_back({v1, up});
            _vertices.push_back({v2, up});
            _vertices.push_back({v3, up});

            _indices.push_back(indexOffset+0);
            _indices.push_back(indexOffset+1);
            _indices.push_back(indexOffset+2);
            _indices.push_back(indexOffset+0);
            _indices.push_back(indexOffset+2);
            _indices.push_back(indexOffset+3);

            indexOffset += 4;
        }
    }
}

void buildPolygonExtrusion(const Polygon& _polygon,
    double _minHeight,
    double _height,
    std::vector<PolygonVertex>& _vertices,
    std::vector<unsigned int>& _indices)
{
    int vertexDataOffset = _vertices.size();
    glm::vec3 upVector(0.0f, 0.0f, 1.0f);
    glm::vec3 normalVector;

    for (auto& line : _polygon) {
        size_t lineSize = line.size();

        _vertices.reserve(_vertices.size() + lineSize * 4);
        _indices.reserve(_indices.size() + lineSize * 6);

        for (size_t i = 0; i < lineSize - 1; i++) {
            glm::vec3 a(line[i]);
            glm::vec3 b(line[i+1]);

            if (a == b) { continue; }

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

            _indices.push_back(vertexDataOffset+0);
            _indices.push_back(vertexDataOffset+1);
            _indices.push_back(vertexDataOffset+2);
            _indices.push_back(vertexDataOffset+1);
            _indices.push_back(vertexDataOffset+3);
            _indices.push_back(vertexDataOffset+2);

            vertexDataOffset += 4;
        }
    }
}

void buildPolygon(const Polygon& _polygon,
    double _height,
    std::vector<PolygonVertex>& _vertices,
    std::vector<unsigned int>& _indices)
{
    mapbox::Earcut<float, unsigned int> earcut;

    earcut(_polygon);

    unsigned int vertexDataOffset = _vertices.size();

    if (earcut.indices.size() == 0) return;

    if (vertexDataOffset == 0) {
        _indices = std::move(earcut.indices);
    } else {
        _indices.reserve(_indices.size() + earcut.indices.size());
        for (auto i : earcut.indices) {
            _indices.push_back(vertexDataOffset + i);
        }
    }

    static glm::vec3 normal(0.0, 0.0, 1.0);

    _vertices.reserve(_vertices.size() + earcut.vertices.size());

    for (auto& p : earcut.vertices) {
        glm::vec3 coord(p[0], p[1], _height);
        _vertices.push_back({coord, normal});
    }
}

bool saveOBJ(std::string _outputOBJ,
    bool _splitMeshes,
    std::vector<PolygonMesh>& _meshes,
    float _offsetX,
    float _offsetY,
    bool _append,
    Tile _tile)
{

    /// Cleanup mesh from degenerate points
    {
        for (auto& mesh : _meshes) {
            if (mesh.indices.size() == 0) continue;

            int i = 0;
            for (auto it = mesh.indices.begin(); it < mesh.indices.end() - 2;) {
                glm::vec3 p0 = mesh.vertices[mesh.indices[i+0]].position;
                glm::vec3 p1 = mesh.vertices[mesh.indices[i+1]].position;
                glm::vec3 p2 = mesh.vertices[mesh.indices[i+2]].position;

                if (p0 == p1 || p0 == p2) {
                    for (int j = 0; j < 3; ++j) {
                        it = mesh.indices.erase(it);
                    }
                } else {
                    it += 3;
                }

                i += 3;
            }
        }
    }

    size_t maxindex = 0;

    /// Find max index from previously existing wavefront vertices
    {
        std::ifstream filein(_outputOBJ.c_str(), std::ios::in);
        std::string token;

        if (filein.good() && _append) {
            // TODO: optimize this
            while (!filein.eof()) {
                filein >> token;
                if (token == "f") {
                    std::string faceLine;
                    getline(filein, faceLine);

                    for (unsigned int i = 0; i < faceLine.length(); ++i) {
                        if (faceLine[i] == '/') {
                            faceLine[i] = ' ';
                        }
                    }

                    std::stringstream ss(faceLine);
                    std::string faceToken;

                    for (int i = 0; i < 6; ++i) {
                        ss >> faceToken;
                        if (faceToken.find_first_not_of("\t\n ") != std::string::npos) {
                            size_t index = atoi(faceToken.c_str());
                            maxindex = index > maxindex ? index : maxindex;
                        }
                    }
                }
            }

            filein.close();
        }
    }

    /// Save obj file
    {
        std::ofstream file;
        if (_append) {
            file = std::ofstream(_outputOBJ, std::ios_base::app);
        } else {
            file = std::ofstream(_outputOBJ);
        }

        if (file.is_open()) {
            file << "# exported with vectiler: https://github.com/karimnaaji/vectiler" << "\n";
            file << "\n";

            int indexOffset = maxindex;

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
    }
    return false;
}

int objexport(const char* _filename,
    const char* _apiKey,
    int _tileX,
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
    std::string apiKey;

    if (!_apiKey) {
        apiKey = "vector-tiles-qVaBcRA";
    } else {
        apiKey = std::string(_apiKey);
    }

    bool terrain = true;
    //Tile tile = {_tileX, _tileY, _tileZ};
    Tile tile = {664, 1583, 12};
    std::string url;

    if (!terrain) {
        url = "http://vector.mapzen.com/osm/all/"
            + std::to_string(tile.z) + "/"
            + std::to_string(tile.x) + "/"
            + std::to_string(tile.y) + ".json?api_key=" + apiKey;
    } else {
        url = "https://terrain-preview.mapzen.com/terrarium/"
            + std::to_string(tile.z) + "/"
            + std::to_string(tile.x) + "/"
            + std::to_string(tile.y) + ".png";
    }

    std::unique_ptr<HeightData> textureData = downloadHeightmapTile(url, tile);

    assert(textureData != nullptr);

    PolygonMesh m;

    /// Build terrain mesh
    {
        // Extract a plane geometry, vertices in [-1.0,1.0]
        // With a resolution of 512 * 512 vertices
        buildPlane(m.vertices, m.indices, 2.0, 2.0, 512, 512);

        // Build terrain mesh extrusion, with bilinear height sampling
        for (auto& vertex : m.vertices) {
            // Normalize vertex coordinates into the texture coordinates range
            float u = (vertex.position.x * 0.5f + 0.5f) * textureData->width;
            float v = (vertex.position.y * 0.5f + 0.5f) * textureData->height;

            float alpha = u - floor(u);
            float beta  = v - floor(v);

            int ii0 = floor(u);
            int jj0 = floor(v);
            int ii1 = ii0 + 1;
            int jj1 = jj0 + 1;

            // Clamp on borders
            ii0 = std::min(ii0, textureData->width - 1);
            jj0 = std::min(jj0, textureData->height -1);
            ii1 = std::min(ii1, textureData->width - 1);
            jj1 = std::min(jj1, textureData->height - 1);

            // Sample four corners of the current texel
            float s0 = textureData->elevation[ii0][jj0];
            float s1 = textureData->elevation[ii0][jj1];
            float s2 = textureData->elevation[ii1][jj0];
            float s3 = textureData->elevation[ii1][jj1];

            // Sample the bilinear height from the elevation texture
            float bilinearHeight = (1 - beta) * (1 - alpha) * s0
                                 + (1 - beta) * alpha       * s1
                                 + beta       * (1 - alpha) * s2
                                 + alpha      * beta        * s3;

            // Scale the height within the tile scale
            vertex.position.z = bilinearHeight * tile.invScale;
        }

        // Compute faces normals
        for (size_t i = 0; i < m.indices.size(); i += 3) {
            int i1 = m.indices[i+0];
            int i2 = m.indices[i+1];
            int i3 = m.indices[i+2];

            const glm::vec3& v1 = m.vertices[i1].position;
            const glm::vec3& v2 = m.vertices[i2].position;
            const glm::vec3& v3 = m.vertices[i3].position;

            glm::vec3 d = glm::normalize(glm::cross(v2 - v1, v3 - v1));

            m.vertices[i1].normal += d;
            m.vertices[i2].normal += d;
            m.vertices[i3].normal += d;
        }

        for (auto& v : m.vertices) {
            v.normal = glm::normalize(v.normal);
        }
    }

    std::vector<PolygonMesh> mes = {m};
    saveOBJ("meshtest.obj", false, mes, 0, 0, false, tile);

    return 0;

    auto data = downloadTile(url, tile);

    if (!data) {
        printf("Failed to download tile data\n");
        return EXIT_FAILURE;
    }

    const static std::string keyHeight("height");
    const static std::string keyMinHeight("min_height");

    std::vector<PolygonMesh> meshes;
    for (auto layer : data->layers) {
        // TODO: give layer as parameter, to filter
        for (auto feature : layer.features) {
            auto itHeight = feature.props.numericProps.find(keyHeight);
            auto itMinHeight = feature.props.numericProps.find(keyMinHeight);
            double height = 0.0;
            double minHeight = 0.0;
            if (itHeight != feature.props.numericProps.end()) {
                height = itHeight->second * tile.invScale;
            }
            if (itMinHeight != feature.props.numericProps.end()) {
                minHeight = itMinHeight->second * tile.invScale;
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

    std::string outFile;

    if (_filename) {
        outFile = std::string(_filename);
    } else {
        outFile = std::to_string(_tileX) + "." + std::to_string(_tileY) + "." + std::to_string(_tileZ);
    }

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
