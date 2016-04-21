#include <iostream>
#include <curl/curl.h>
#include <string>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <limits>

#include "rapidjson/document.h"
#include "geojson.h"
#include "tiledata.h"
#include "vectiler.h"
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
    glm::vec2 offset;
};

static size_t writeData(void* ptr, size_t size, size_t nmemb, void *stream) {
    ((std::stringstream*) stream)->write(reinterpret_cast<char*>(ptr), size * nmemb);
    return size * nmemb;
}

bool downloadData(std::stringstream& out, const std::string& url) {
    static bool curlInitialized = false;
    static CURL* curlHandle = nullptr;

    if (!curlInitialized) {
        curl_global_init(CURL_GLOBAL_DEFAULT);
        curlHandle = curl_easy_init();
        curlInitialized = true;

        // set up curl to perform fetch
        curl_easy_setopt(curlHandle, CURLOPT_WRITEFUNCTION, writeData);
        curl_easy_setopt(curlHandle, CURLOPT_HEADER, 0L);
        curl_easy_setopt(curlHandle, CURLOPT_VERBOSE, 0L);
        curl_easy_setopt(curlHandle, CURLOPT_ACCEPT_ENCODING, "gzip");
        curl_easy_setopt(curlHandle, CURLOPT_NOSIGNAL, 1L);
        curl_easy_setopt(curlHandle, CURLOPT_DNS_CACHE_TIMEOUT, -1);
    }

    curl_easy_setopt(curlHandle, CURLOPT_WRITEDATA, &out);
    curl_easy_setopt(curlHandle, CURLOPT_URL, url.c_str());

    printf("URL request: %s", url.c_str());

    CURLcode result = curl_easy_perform(curlHandle);

    if (result != CURLE_OK) {
        printf(" -- Failure: %s\n", curl_easy_strerror(result));
    } else {
        printf(" -- OK\n");
    }

    return result == CURLE_OK;
}

std::unique_ptr<HeightData> downloadHeightmapTile(const std::string& url,
    const Tile& tile,
    float extrusionScale)
{
    std::stringstream out;

    if (downloadData(out, url)) {
        unsigned char* pixels;
        int width, height, comp;

        // Decode texture PNG
        const unsigned char* pngData = reinterpret_cast<const unsigned char*>(out.str().c_str());
        pixels = stbi_load_from_memory(pngData, out.str().length(), &width, &height, &comp,
            STBI_rgb_alpha);

        // printf("Texture data %d width, %d height, %d comp\n", width, height, comp);

        if (comp != STBI_rgb_alpha) {
            printf("Failed to decompress PNG file\n");
            return nullptr;
        }

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

            int y = i / height;
            int x = i % width;

            assert(x >= 0 && x <= width && y >= 0 && y <= height);

            data->elevation[x][y] = elevation * extrusionScale;
        }

        return std::move(data);
    }

    return nullptr;
}

std::unique_ptr<TileData> downloadTile(const std::string& url, const Tile& tile) {
    std::stringstream out;

    if (downloadData(out, url)) {
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
            GeoJson::extractLayer(layer->value, data->layers.back(), tile);
        }

        return std::move(data);
    }

    return nullptr;
}

bool withinTileRange(const glm::vec2& pos) {
    return pos.x >= -1.0 && pos.x <= 1.0
        && pos.y >= -1.0 && pos.y <= 1.0;
}

/*
 * Sample elevation using bilinear texture sampling
 * - position: must lie within tile range [-1.0, 1.0]
 * - textureData: the elevation tile data, may be null
 */
float sampleElevation(glm::vec2 position, const std::unique_ptr<HeightData>& textureData) {
    if (!textureData) {
        return 0.0;
    }

    if (!withinTileRange(position)) {
        position = glm::clamp(position, glm::vec2(-1.0), glm::vec2(1.0));
    }

    // Normalize vertex coordinates into the texture coordinates range
    float u = (position.x * 0.5f + 0.5f) * textureData->width;
    float v = (position.y * 0.5f + 0.5f) * textureData->height;

    // Flip v coordinate according to tile coordinates
    v = textureData->height - v;

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

    return bilinearHeight;
}

glm::vec2 centroid(const std::vector<std::vector<glm::vec3>>& polygon) {
    glm::vec2 centroid;
    int n = 0;

    for (auto& l : polygon) {
        for (auto& p : l) {
            centroid.x += p.x;
            centroid.y += p.y;
            n++;
        }
    }

    if (n == 0) {
        return centroid;
    }

    centroid /= n;
    return centroid;
}

void buildPlane(std::vector<PolygonVertex>& outVertices,
    std::vector<unsigned int>& outIndices,
    float width,       // Total plane width (x-axis)
    float height,      // Total plane height (y-axis)
    unsigned int nw,   // Split on width
    unsigned int nh)   // Split on height
{
    // TODO: add offsets
    std::vector<glm::vec4> vertices;
    std::vector<int> indices;

    int indexOffset = 0;

    float ow = width / nw;
    float oh = height / nh;

    for (float w = -width / 2.0; w <= width / 2.0 - ow; w += ow) {
        for (float h = -height / 2.0; h <= height / 2.0 - oh; h += oh) {
            glm::vec3 v0(w, h + oh, 0.0);
            glm::vec3 v1(w, h, 0.0);
            glm::vec3 v2(w + ow, h, 0.0);
            glm::vec3 v3(w + ow, h + oh, 0.0);

            static const glm::vec3 up(0.0, 0.0, 1.0);

            outVertices.push_back({v0, up});
            outVertices.push_back({v1, up});
            outVertices.push_back({v2, up});
            outVertices.push_back({v3, up});

            // FIXME: probably a bad index order
            outIndices.push_back(indexOffset+0);
            outIndices.push_back(indexOffset+1);
            outIndices.push_back(indexOffset+2);
            outIndices.push_back(indexOffset+0);
            outIndices.push_back(indexOffset+2);
            outIndices.push_back(indexOffset+3);

            indexOffset += 4;
        }
    }
}

float buildPolygonExtrusion(const Polygon& polygon,
    double minHeight,
    double height,
    std::vector<PolygonVertex>& outVertices,
    std::vector<unsigned int>& outIndices,
    const std::unique_ptr<HeightData>& elevation,
    float inverseTileScale)
{
    int vertexDataOffset = outVertices.size();
    glm::vec3 upVector(0.0f, 0.0f, 1.0f);
    glm::vec3 normalVector;
    float minz = 0.f;
    float cz = 0.f;

    // Compute min and max height of the polygon
    if (elevation) {
        // The polygon centroid height
        cz = sampleElevation(centroid(polygon), elevation);
        minz = std::numeric_limits<float>::max();

        for (auto& line : polygon) {
            for (size_t i = 0; i < line.size(); i++) {
                glm::vec3 p(line[i]);

                float pz = sampleElevation(glm::vec2(p.x, p.y), elevation);

                minz = std::min(minz, pz);
            }
        }
    }

    for (auto& line : polygon) {
        size_t lineSize = line.size();

        outVertices.reserve(outVertices.size() + lineSize * 4);
        outIndices.reserve(outIndices.size() + lineSize * 6);

        for (size_t i = 0; i < lineSize - 1; i++) {
            glm::vec3 a(line[i]);
            glm::vec3 b(line[i+1]);

            if (a == b) { continue; }

            normalVector = glm::cross(upVector, b - a);
            normalVector = glm::normalize(normalVector);

            a.z = height + cz * inverseTileScale;
            outVertices.push_back({a, normalVector});
            b.z = height + cz * inverseTileScale;
            outVertices.push_back({b, normalVector});
            a.z = minHeight + minz * inverseTileScale;
            outVertices.push_back({a, normalVector});
            b.z = minHeight + minz * inverseTileScale;
            outVertices.push_back({b, normalVector});

            outIndices.push_back(vertexDataOffset+0);
            outIndices.push_back(vertexDataOffset+1);
            outIndices.push_back(vertexDataOffset+2);
            outIndices.push_back(vertexDataOffset+1);
            outIndices.push_back(vertexDataOffset+3);
            outIndices.push_back(vertexDataOffset+2);

            vertexDataOffset += 4;
        }
    }

    return cz;
}

void buildPolygon(const Polygon& polygon,
    double height,
    std::vector<PolygonVertex>& outVertices,
    std::vector<unsigned int>& outIndices,
    const std::unique_ptr<HeightData>& elevation,
    float centroidHeight,
    float inverseTileScale)
{
    mapbox::Earcut<float, unsigned int> earcut;

    earcut(polygon);

    unsigned int vertexDataOffset = outVertices.size();

    if (earcut.indices.size() == 0) {
        return;
    }

    if (vertexDataOffset == 0) {
        outIndices = std::move(earcut.indices);
    } else {
        outIndices.reserve(outIndices.size() + earcut.indices.size());

        for (auto i : earcut.indices) {
            outIndices.push_back(vertexDataOffset + i);
        }
    }

    static glm::vec3 normal(0.0, 0.0, 1.0);

    outVertices.reserve(outVertices.size() + earcut.vertices.size());

    centroidHeight *= inverseTileScale;

    for (auto& p : earcut.vertices) {
        glm::vec2 position(p[0], p[1]);
        glm::vec3 coord(position.x, position.y, height + centroidHeight);
        outVertices.push_back({coord, normal});
    }
}

glm::vec3 perp(const glm::vec3& v1, const glm::vec3& v2) {
    return glm::normalize(glm::vec3(v2.y - v1.y, v1.x - v2.x, 0.0));
}

void addPolylineIndices(std::vector<unsigned int>& outIndices, size_t vertexDataOffset) {
    outIndices.push_back(vertexDataOffset+0);
    outIndices.push_back(vertexDataOffset+2);
    outIndices.push_back(vertexDataOffset+1);
    outIndices.push_back(vertexDataOffset+1);
    outIndices.push_back(vertexDataOffset+2);
    outIndices.push_back(vertexDataOffset+3);
}

void addPolylineMitterIndices(std::vector<unsigned int>& outIndices, size_t vertexDataOffset) {
    outIndices.push_back(vertexDataOffset+0);
    outIndices.push_back(vertexDataOffset+1);
    outIndices.push_back(vertexDataOffset+2);
}

void subdivideLine(Line& line, float subdivision) {
    float subdivisionStep = 1.0 / subdivision;

    auto it = line.begin();

    for (int i = 0; i < line.size() - 1; ++i) {
        glm::vec3 p0 = line[i];
        glm::vec3 p1 = line[i+1];
        glm::vec3 dir = glm::normalize(p1 - p0);
        float distance = glm::distance(p0, p1);

        if (distance > subdivisionStep) {
            int steps = distance / subdivisionStep;
            float step = distance / steps;

            for (int s = 1; s < steps; ++s) {
                glm::vec3 np = p0 + dir * step * (float)s;
                it = line.insert(it + 1, np);
                i++;
            }
        }
    }
}

void buildPolyline(Line& line,
    std::vector<PolygonVertex>& outVertices,
    std::vector<unsigned int>& outIndices,
    float inverseTileScale)
{
    // TODO: give as param
    const static float scale = 5.0;
    const static glm::vec3 normal = glm::vec3(0.0, 0.0, 1.0);

    float extrude = scale * inverseTileScale;

    glm::vec3 currentVertex(line[0]);
    glm::vec3 nextVertex(line[1]);

    if (currentVertex == nextVertex) {
        return;
    }

    glm::vec3 n0 = perp(currentVertex, nextVertex);

    size_t vertexDataOffset = outVertices.size();

    currentVertex.z = 0.02;

    outVertices.push_back({currentVertex + n0 * extrude, normal});
    outVertices.push_back({currentVertex - n0 * extrude, normal});

    glm::vec3 lastVertex;
    glm::vec3 lastNormal;

    for (size_t i = 1; i < line.size() - 1; ++i) {
        lastVertex = currentVertex;
        lastNormal = n0;

        currentVertex = line[i];
        nextVertex = line[i+1];

        // TODO: make configurable
        currentVertex.z = 0.02;
        nextVertex.z = 0.02;

        if (currentVertex == nextVertex) {
            continue;
        }

        n0 = perp(lastVertex, currentVertex);
        glm::vec3 n1 = perp(currentVertex, nextVertex);

        bool rightTurn = glm::cross(n1, n0).z > 0.0;

        glm::vec3 mitter = n0 + n1;

        mitter *= sqrtf(2.0f / (1.0f + glm::dot(n0, n1)) / glm::dot(mitter, mitter));

        if (rightTurn) {
            outVertices.push_back({currentVertex + mitter * extrude, normal});
            outVertices.push_back({currentVertex - lastNormal * extrude, normal});

            addPolylineIndices(outIndices, vertexDataOffset);

            vertexDataOffset += 4;

            // Add mitter triangle
            outVertices.push_back({currentVertex + mitter * extrude, normal});
            outVertices.push_back({currentVertex - n1 * extrude, normal});
            outVertices.push_back({currentVertex - lastNormal * extrude, normal});

            addPolylineMitterIndices(outIndices, vertexDataOffset);

            vertexDataOffset += 3;

            outVertices.push_back({currentVertex + mitter * extrude, normal});
            outVertices.push_back({currentVertex - n1 * extrude, normal});
        } else {
            outVertices.push_back({currentVertex + lastNormal * extrude, normal});
            outVertices.push_back({currentVertex - mitter * extrude, normal});

            addPolylineIndices(outIndices, vertexDataOffset);

            vertexDataOffset += 4;

            outVertices.push_back({currentVertex - mitter * extrude, normal});
            outVertices.push_back({currentVertex + lastNormal * extrude, normal});
            outVertices.push_back({currentVertex + n1 * extrude, normal});

            addPolylineMitterIndices(outIndices, vertexDataOffset);

            vertexDataOffset += 3;

            outVertices.push_back({currentVertex + n1 * extrude, normal});
            outVertices.push_back({currentVertex - mitter * extrude, normal});
        }
    }

    lastVertex = currentVertex;
    currentVertex = nextVertex;

    if (currentVertex == lastVertex) {
        outVertices.pop_back();
        outVertices.pop_back();
        return;
    }

    currentVertex.z = 0.02;

    n0 = perp(lastVertex, currentVertex);

    outVertices.push_back({currentVertex + n0 * extrude, normal});
    outVertices.push_back({currentVertex - n0 * extrude, normal});

    addPolylineIndices(outIndices, vertexDataOffset);
}

void adjustTerrainEdges(std::unordered_map<Tile, std::unique_ptr<HeightData>>& heightData) {
    for (auto& tileData0 : heightData) {
        auto& tileHeight0 = tileData0.second;

        for (auto& tileData1 : heightData) {
            if (tileData0.first == tileData1.first) {
                continue;
            }

            auto& tileHeight1 = tileData1.second;

            if (tileData0.first.x + 1 == tileData1.first.x
             && tileData0.first.y == tileData1.first.y) {
                for (size_t y = 0; y < tileHeight0->height; ++y) {
                    float h0 = tileHeight0->elevation[tileHeight0->width - 1][y];
                    float h1 = tileHeight1->elevation[0][y];
                    float h = (h0 + h1) * 0.5f;
                    tileHeight0->elevation[tileHeight0->width - 1][y] = h;
                    tileHeight1->elevation[0][y] = h;
                }
            }

            if (tileData0.first.y + 1 == tileData1.first.y
             && tileData0.first.x == tileData1.first.x) {
                for (size_t x = 0; x < tileHeight0->width; ++x) {
                    float h0 = tileHeight0->elevation[x][tileHeight0->height - 1];
                    float h1 = tileHeight1->elevation[x][0];
                    float h = (h0 + h1) * 0.5f;
                    tileHeight0->elevation[x][tileHeight0->height - 1] = h;
                    tileHeight1->elevation[x][0] = h;
                }
            }

            // TODO: Corner point
        }
    }
}

/*
 * Save an obj file for the set of meshes
 * - outputOBJ: the output filename of the wavefront object file
 * - splitMeshes: will enable exporting meshes as single objects within
 *   the wavefront file
 * - offsetx/y: are global offset, additional to the inner mesh offset
 * - append: option will append meshes to an existing obj file
 *   (filename should be the same)
 */
bool saveOBJ(std::string outputOBJ,
    bool splitMeshes,
    std::vector<std::unique_ptr<PolygonMesh>>& meshes,
    float offsetx,
    float offsety,
    bool append)
{
    /// Cleanup mesh from degenerate points
    {
        for (auto& mesh : meshes) {
            if (mesh->indices.size() == 0) continue;

            int i = 0;
            for (auto it = mesh->indices.begin(); it < mesh->indices.end() - 2;) {
                glm::vec3 p0 = mesh->vertices[mesh->indices[i+0]].position;
                glm::vec3 p1 = mesh->vertices[mesh->indices[i+1]].position;
                glm::vec3 p2 = mesh->vertices[mesh->indices[i+2]].position;

                if (p0 == p1 || p0 == p2) {
                    for (int j = 0; j < 3; ++j) {
                        it = mesh->indices.erase(it);
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
        std::ifstream filein(outputOBJ.c_str(), std::ios::in);
        std::string token;

        if (filein.good() && append) {
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
        if (append) {
            file = std::ofstream(outputOBJ, std::ios_base::app);
        } else {
            file = std::ofstream(outputOBJ);
        }

        if (file.is_open()) {
            file << "# exported with vectiler: https://github.com/karimnaaji/vectiler" << "\n";
            file << "\n";

            int indexOffset = maxindex;

            if (splitMeshes) {
                int meshCnt = 0;

                for (const auto& mesh : meshes) {
                    if (mesh->vertices.size() == 0) { continue; }

                    file << "o mesh" << meshCnt++ << "\n";

                    for (auto vertex : mesh->vertices) {
                        file << "v " << vertex.position.x + offsetx + mesh->offset.x << " "
                             << vertex.position.y + offsety + mesh->offset.y << " "
                             << vertex.position.z << "\n";
                    }

                    for (auto vertex : mesh->vertices) {
                        file << "vn " << vertex.normal.x << " "
                             << vertex.normal.y << " "
                             << vertex.normal.z << "\n";
                    }

                    for (int i = 0; i < mesh->indices.size(); i += 3) {
                        file << "f " << mesh->indices[i] + indexOffset + 1 << "//"
                             << mesh->indices[i] + indexOffset + 1;
                        file << " ";
                        file << mesh->indices[i+1] + indexOffset + 1 << "//"
                             << mesh->indices[i+1] + indexOffset + 1;
                        file << " ";
                        file << mesh->indices[i+2] + indexOffset + 1 << "//"
                             << mesh->indices[i+2] + indexOffset + 1 << "\n";
                    }

                    file << "\n";
                    indexOffset += mesh->vertices.size();
                }
            } else {
                file << "o " << outputOBJ << "\n";

                for (const auto& mesh : meshes) {
                    if (mesh->vertices.size() == 0) { continue; }

                    for (auto vertex : mesh->vertices) {
                        file << "v " << vertex.position.x + offsetx + mesh->offset.x << " "
                             << vertex.position.y + offsety + mesh->offset.y << " "
                             << vertex.position.z << "\n";
                    }
                }

                for (const auto& mesh : meshes) {
                    if (mesh->vertices.size() == 0) { continue; }

                    for (auto vertex : mesh->vertices) {
                        file << "vn " << vertex.normal.x << " "
                             << vertex.normal.y << " "
                             << vertex.normal.z << "\n";
                    }
                }

                for (const auto& mesh : meshes) {
                    if (mesh->vertices.size() == 0) { continue; }

                    for (int i = 0; i < mesh->indices.size(); i += 3) {
                        file << "f " << mesh->indices[i] + indexOffset + 1 << "//"
                             << mesh->indices[i] + indexOffset + 1;
                        file << " ";
                        file << mesh->indices[i+1] + indexOffset + 1 << "//"
                             << mesh->indices[i+1] + indexOffset + 1;
                        file << " ";
                        file << mesh->indices[i+2] + indexOffset + 1 << "//"
                             << mesh->indices[i+2] + indexOffset + 1 << "\n";
                    }

                    indexOffset += mesh->vertices.size();
                }
            }

            file.close();
            printf("Saved obj file %s\n", outputOBJ.c_str());
            return true;
        } else {
            printf("Can't open file %s\n", outputOBJ.c_str());
        }
    }

    return false;
}

std::vector<std::string> splitString(const std::string& s, char delim) {
    std::vector<std::string> elems;
    std::stringstream ss(s);
    std::string item;

    while (std::getline(ss, item, delim)) {
        elems.push_back(item);
    }

    return elems;
}

bool extractTileRange(int* start, int* end, const std::string& range) {
    std::vector<std::string> tilesRange = splitString(range, '/');

    if (tilesRange.size() > 2 || tilesRange.size() == 0) {
        return false;
    }

    if (tilesRange.size() == 2) {
        *start = std::stoi(tilesRange[0]);
        *end = std::stoi(tilesRange[1]);
    } else {
        *start = *end = std::stoi(tilesRange[0]);
    }

    if (*end < *start) {
        return false;
    }

    return true;
}

inline std::string vectorTileURL(const Tile& tile, const std::string& apiKey) {
    return "http://vector.mapzen.com/osm/all/"
        + std::to_string(tile.z) + "/"
        + std::to_string(tile.x) + "/"
        + std::to_string(tile.y) + ".json?api_key=" + apiKey;
}

inline std::string terrainURL(const Tile& tile) {
    return "https://terrain-preview.mapzen.com/terrarium/"
        + std::to_string(tile.z) + "/"
        + std::to_string(tile.x) + "/"
        + std::to_string(tile.y) + ".png";
}

int vectiler(Params exportParams) {
    std::string apiKey(exportParams.apiKey);

    printf("Using API key %s\n", exportParams.apiKey);

    std::vector<Tile> tiles;

    /// Parse tile params
    {
        int startx, starty, endx, endy;

        if (!extractTileRange(&startx, &endx, std::string(exportParams.tilex))) {
            printf("Bad param: %s\n", exportParams.tilex);
            return EXIT_FAILURE;
        }

        if (!extractTileRange(&starty, &endy, std::string(exportParams.tiley))) {
            printf("Bad param: %s\n", exportParams.tiley);
            return EXIT_FAILURE;
        }

        for (size_t x = startx; x <= endx; ++x) {
            for (size_t y = starty; y <= endy; ++y) {
                tiles.emplace_back(x, y, exportParams.tilez);
            }
        }
    }

    if (tiles.size() == 0) {
        printf("No tiles to download\n");
        return EXIT_FAILURE;
    }

    std::unordered_map<Tile, std::unique_ptr<HeightData>> heightData;
    std::unordered_map<Tile, std::unique_ptr<TileData>> vectorTileData;

    /// Download data
    {
        printf("---- Downloading tile data ----\n");

        for (auto tile : tiles) {
            if (exportParams.terrain) {
                std::string url = terrainURL(tile);

                auto textureData = downloadHeightmapTile(url, tile,
                    exportParams.terrainExtrusionScale);

                if (!textureData) {
                    printf("Failed to download heightmap texture data for tile %d %d %d\n",
                        tile.x, tile.y, tile.z);
                }

                heightData[tile] = std::move(textureData);
            }

            if (exportParams.buildings) {
               std::string url = vectorTileURL(tile, apiKey);

                auto tileData = downloadTile(url, tile);

                if (!tileData) {
                    printf("Failed to download vector tile data for tile %d %d %d\n",
                        tile.x, tile.y, tile.z);
                }

                vectorTileData[tile] = std::move(tileData);
            }
        }
    }

    /// Adjust terrain edges
    if (exportParams.terrain) {
        adjustTerrainEdges(heightData);
    }

    std::vector<std::unique_ptr<PolygonMesh>> meshes;

    glm::vec2 offset;
    Tile origin = tiles[0];

    printf("---- Building tile data ----\n");

    // Build meshes for each of the tiles
    for (auto tile : tiles) {
        std::string url;

        offset.x =  (tile.x - origin.x) * 2;
        offset.y = -(tile.y - origin.y) * 2;

        const auto& textureData = heightData[tile];

        /// Build terrain mesh
        if (exportParams.terrain) {
            url = terrainURL(tile);

            const auto& textureData = heightData[tile];

            if (!textureData) {
                printf("Failed to download heightmap texture data for tile %d %d %d\n",
                    tile.x, tile.y, tile.z);
            } else {
                auto mesh = std::unique_ptr<PolygonMesh>(new PolygonMesh);

                /// Extract a plane geometry, vertices in [-1.0,1.0]
                {
                    buildPlane(mesh->vertices, mesh->indices, 2.0, 2.0,
                        exportParams.terrainSubdivision, exportParams.terrainSubdivision);

                    // Build terrain mesh extrusion, with bilinear height sampling
                    for (auto& vertex : mesh->vertices) {
                        glm::vec2 tilePosition = glm::vec2(vertex.position.x, vertex.position.y);
                        float extrusion = sampleElevation(tilePosition, textureData);

                        // Scale the height within the tile scale
                        vertex.position.z = extrusion * tile.invScale;
                    }
                }

                /// Compute faces normals
                {
                    for (size_t i = 0; i < mesh->indices.size(); i += 3) {
                        int i1 = mesh->indices[i+0];
                        int i2 = mesh->indices[i+1];
                        int i3 = mesh->indices[i+2];

                        const glm::vec3& v1 = mesh->vertices[i1].position;
                        const glm::vec3& v2 = mesh->vertices[i2].position;
                        const glm::vec3& v3 = mesh->vertices[i3].position;

                        glm::vec3 d = glm::normalize(glm::cross(v2 - v1, v3 - v1));

                        mesh->vertices[i1].normal += d;
                        mesh->vertices[i2].normal += d;
                        mesh->vertices[i3].normal += d;
                    }

                    for (auto& v : mesh->vertices) {
                        v.normal = glm::normalize(v.normal);
                    }
                }

                mesh->offset = offset;
                meshes.push_back(std::move(mesh));
            }
        }

        /// Build vector tile mesh
        if (exportParams.buildings) {
            const auto& data = vectorTileData[tile];

            if (data) {
                const static std::string keyHeight("height");
                const static std::string keyMinHeight("min_height");

                for (auto layer : data->layers) {
                    for (auto feature : layer.features) {
                        // Coupled with terrain data, only export layer buildings for now
                        if (textureData && layer.name != "buildings") {
                            continue;
                        }

                        auto itHeight = feature.props.numericProps.find(keyHeight);
                        auto itMinHeight = feature.props.numericProps.find(keyMinHeight);
                        float scale = tile.invScale * exportParams.buildingsExtrusionScale;
                        double height = 0.0;
                        double minHeight = 0.0;

                        if (itHeight != feature.props.numericProps.end()) {
                            height = itHeight->second * scale;
                        }

                        if (textureData && height == 0.0) {
                            continue;
                        }

                        if (itMinHeight != feature.props.numericProps.end()) {
                            minHeight = itMinHeight->second * scale;
                        }

                        auto mesh = std::unique_ptr<PolygonMesh>(new PolygonMesh);

                        for (const Polygon& polygon : feature.polygons) {
                            float centroidHeight = 0.f;
                            if (minHeight != height) {
                                centroidHeight = buildPolygonExtrusion(polygon, minHeight, height,
                                    mesh->vertices, mesh->indices, textureData, tile.invScale);
                            }

                            buildPolygon(polygon, height, mesh->vertices, mesh->indices,
                                textureData, centroidHeight, tile.invScale);
                        }

                        if (layer.name == "roads" ) {
                            for (const Line& line : feature.lines) {
                                buildPolyline(line, mesh->vertices, mesh->indices, tile.invScale);
                            }
                        }

                        // Add local mesh offset
                        mesh->offset = offset;
                        meshes.push_back(std::move(mesh));
                    }
                }
            }
        }
    }

    std::string outFile;

    if (exportParams.filename) {
        outFile = std::string(exportParams.filename);
    } else {
        outFile = std::to_string(origin.x) + "."
                + std::to_string(origin.y) + "."
                + std::to_string(origin.z);
    }

    std::string outputOBJ = outFile + ".obj";

    // Save output OBJ file
    bool saved = saveOBJ(outputOBJ,
        exportParams.splitMesh, meshes,
        exportParams.offset[0],
        exportParams.offset[1],
        exportParams.append);

    if (!saved) {
        return EXIT_FAILURE;
    }

    // Bake ambiant occlusion using AO Baker
    if (exportParams.aoBaking) {
        bool aoBaked = aobaker_bake(outputOBJ.c_str(),
            (outFile + "-ao.obj").c_str(),
            (outFile + ".png").c_str(),
            exportParams.aoSizeHint,
            exportParams.aoSamples,
            false,  // g-buffers
            false,  // charinfo
            1.0);   // multiply

        return aoBaked;
    }

    return EXIT_SUCCESS;
}
