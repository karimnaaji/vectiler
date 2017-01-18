#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <unordered_map>
#include <limits>
#include <memory>

#include "rapidjson/document.h"
#include "geojson.h"
#include "tiledata.h"
#include "vectiler.h"
#include "earcut.h"
#include "platform.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define EPSILON 1e-5f

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

void computeNormals(PolygonMesh& mesh) {
    for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        int i1 = mesh.indices[i+0];
        int i2 = mesh.indices[i+1];
        int i3 = mesh.indices[i+2];

        const glm::vec3& v1 = mesh.vertices[i1].position;
        const glm::vec3& v2 = mesh.vertices[i2].position;
        const glm::vec3& v3 = mesh.vertices[i3].position;

        glm::vec3 d = glm::normalize(glm::cross(v2 - v1, v3 - v1));

        mesh.vertices[i1].normal += d;
        mesh.vertices[i2].normal += d;
        mesh.vertices[i3].normal += d;
    }

    for (auto& v : mesh.vertices) {
        v.normal = glm::normalize(v.normal);
    }
}

std::unique_ptr<HeightData> downloadHeightmapTile(const std::string& url,
    const Tile& tile,
    float extrusionScale)
{
    std::string out;

    if (downloadData(out, url)) {
        unsigned char* pixels;
        int width, height, comp;

        // Decode texture PNG
        const unsigned char* pngData = reinterpret_cast<const unsigned char*>(out.c_str());
        pixels = stbi_load_from_memory(pngData, out.length(), &width, &height, &comp,
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

        return data;
    }

    return nullptr;
}

std::unique_ptr<TileData> downloadTile(const std::string& url, const Tile& tile) {
    std::string out;

    if (downloadData(out, url)) {
        // parse written data into a JSON object
        rapidjson::Document doc;
        doc.Parse(out.c_str());

        if (doc.HasParseError()) {
            printf("Error parsing tile\n");
            return nullptr;
        }

        std::unique_ptr<TileData> data = std::unique_ptr<TileData>(new TileData());
        for (auto layer = doc.MemberBegin(); layer != doc.MemberEnd(); ++layer) {
            data->layers.emplace_back(std::string(layer->name.GetString()));
            GeoJson::extractLayer(layer->value, data->layers.back(), tile);
        }

        return data;
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
    unsigned int nh,   // Split on height
    bool flip = false)
{
    // TODO: add offsets
    std::vector<glm::vec4> vertices;
    std::vector<int> indices;

    int indexOffset = 0;

    float ow = width / nw;
    float oh = height / nh;
    static const glm::vec3 up(0.0, 0.0, 1.0);

    glm::vec3 normal = up;

    if (flip) {
        normal *= -1.f;
    }

    for (float w = -width / 2.0; w <= width / 2.0 - ow; w += ow) {
        for (float h = -height / 2.0; h <= height / 2.0 - oh; h += oh) {
            glm::vec3 v0(w, h + oh, 0.0);
            glm::vec3 v1(w, h, 0.0);
            glm::vec3 v2(w + ow, h, 0.0);
            glm::vec3 v3(w + ow, h + oh, 0.0);

            outVertices.push_back({v0, normal});
            outVertices.push_back({v1, normal});
            outVertices.push_back({v2, normal});
            outVertices.push_back({v3, normal});

            if (!flip) {
                outIndices.push_back(indexOffset+0);
                outIndices.push_back(indexOffset+1);
                outIndices.push_back(indexOffset+2);
                outIndices.push_back(indexOffset+0);
                outIndices.push_back(indexOffset+2);
                outIndices.push_back(indexOffset+3);
            } else {
                outIndices.push_back(indexOffset+0);
                outIndices.push_back(indexOffset+2);
                outIndices.push_back(indexOffset+1);
                outIndices.push_back(indexOffset+0);
                outIndices.push_back(indexOffset+3);
                outIndices.push_back(indexOffset+2);
            }

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

void buildPedestalPlanes(const Tile& tile,
    std::vector<PolygonVertex>& outVertices,
    std::vector<unsigned int>& outIndices,
    const std::unique_ptr<HeightData>& elevation,
    unsigned int subdiv,
    float pedestalHeight)
{
    float offset = 1.0 / subdiv;
    int vertexDataOffset = outVertices.size();

    for (int i = 0; i < tile.borders.size(); ++i) {
        if (!tile.borders[i]) {
            continue;
        }

        for (float x = -1.0; x < 1.0; x += offset) {
            static const glm::vec3 upVector(0.0, 0.0, 1.0);
            glm::vec3 v0, v1;

            if (i == Border::right) {
                v0 = glm::vec3(1.0, x + offset, 0.0);
                v1 = glm::vec3(1.0, x, 0.0);
            }

            if (i == Border::left) {
                v0 = glm::vec3(-1.0, x + offset, 0.0);
                v1 = glm::vec3(-1.0, x, 0.0);
            }

            if (i == Border::top) {
                v0 = glm::vec3(x + offset, 1.0, 0.0);
                v1 = glm::vec3(x, 1.0, 0.0);
            }

            if (i == Border::bottom) {
                v0 = glm::vec3(x + offset, -1.0, 0.0);
                v1 = glm::vec3(x, -1.0, 0.0);
            }

            glm::vec3 normalVector;

            normalVector = glm::cross(upVector, v0 - v1);
            normalVector = glm::normalize(normalVector);

            float h0 = sampleElevation(glm::vec2(v0.x, v0.y), elevation);
            float h1 = sampleElevation(glm::vec2(v1.x, v1.y), elevation);

            v0.z = h0 * tile.invScale;
            outVertices.push_back({v0, normalVector});
            v1.z = h1 * tile.invScale;
            outVertices.push_back({v1, normalVector});
            v0.z = pedestalHeight * tile.invScale;
            outVertices.push_back({v0, normalVector});
            v1.z = pedestalHeight * tile.invScale;
            outVertices.push_back({v1, normalVector});

            if (i == Border::right || i == Border::bottom) {
                outIndices.push_back(vertexDataOffset+0);
                outIndices.push_back(vertexDataOffset+1);
                outIndices.push_back(vertexDataOffset+2);
                outIndices.push_back(vertexDataOffset+1);
                outIndices.push_back(vertexDataOffset+3);
                outIndices.push_back(vertexDataOffset+2);
            } else {
                outIndices.push_back(vertexDataOffset+0);
                outIndices.push_back(vertexDataOffset+2);
                outIndices.push_back(vertexDataOffset+1);
                outIndices.push_back(vertexDataOffset+1);
                outIndices.push_back(vertexDataOffset+2);
                outIndices.push_back(vertexDataOffset+3);
            }

            vertexDataOffset += 4;
        }
    }
}

glm::vec3 perp(const glm::vec3& v) {
    return glm::normalize(glm::vec3(-v.y, v.x, 0.0));
}

glm::vec3 computeMiterVector(const glm::vec3& d0,
    const glm::vec3& d1,
    const glm::vec3& n0,
    const glm::vec3& n1)
{
    glm::vec3 miter = glm::normalize(n0 + n1);
    float miterl2 = glm::dot(miter, miter);

    if (miterl2 < std::numeric_limits<float>::epsilon()) {
        miter = glm::vec3(n1.y - n0.y, n0.x - n1.x, 0.0);
    } else {
        float theta = atan2f(d1.y, d1.x) - atan2f(d0.y, d0.x);
        if (theta < 0.f) { theta += 2 * M_PI; }
        miter *= 1.f / std::max<float>(sin(theta * 0.5f), EPSILON);
    }

    return miter;
}

void addPolygonPolylinePoint(Line& line,
    const glm::vec3 curr,
    const glm::vec3 next,
    const glm::vec3 last,
    const float extrude,
    size_t lineDataSize,
    size_t i,
    bool forward)
{
    glm::vec3 n0 = perp(curr - last);
    glm::vec3 n1 = perp(next - curr);
    bool right = glm::cross(n1, n0).z > 0.0;

    if ((i == 1 && forward) || (i == lineDataSize - 2 && !forward)) {
        line.push_back(last + n0 * extrude);
        line.push_back(last - n0 * extrude);
    }

    if (right) {
        glm::vec3 d0 = glm::normalize(last - curr);
        glm::vec3 d1 = glm::normalize(next - curr);
        glm::vec3 miter = computeMiterVector(d0, d1, n0, n1);
        line.push_back(curr - miter * extrude);
    } else {
        line.push_back(curr - n0 * extrude);
        line.push_back(curr - n1 * extrude);
    }
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
        }
    }
}

void addFaces(std::ostream& file, const PolygonMesh& mesh, size_t indexOffset, bool normals) {
    for (int i = 0; i < mesh.indices.size(); i += 3) {
        file << "f " << mesh.indices[i] + indexOffset + 1
             << (normals ? "//" + std::to_string(mesh.indices[i] + indexOffset + 1) : "");
        file << " ";
        file << mesh.indices[i+1] + indexOffset + 1
             << (normals ? "//" + std::to_string(mesh.indices[i+1] + indexOffset + 1) : "");
        file << " ";
        file << mesh.indices[i+2] + indexOffset + 1
             << (normals ? "//" + std::to_string(mesh.indices[i+2] + indexOffset + 1) : "");
        file << "\n";
    }
}

void addNormals(std::ostream& file, const PolygonMesh& mesh) {
    for (auto vertex : mesh.vertices) {
        file << "vn " << vertex.normal.x << " "
             << vertex.normal.y << " "
             << vertex.normal.z << "\n";
    }
}

void addPositions(std::ostream& file, const PolygonMesh& mesh, float offsetx, float offsety) {
    for (auto vertex : mesh.vertices) {
        file << "v " << vertex.position.x + offsetx + mesh.offset.x << " "
             << vertex.position.y + offsety + mesh.offset.y << " "
             << vertex.position.z << "\n";
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
    bool append,
    bool normals)
{
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
        std::ofstream file(outputOBJ);

        if (append) {
            file.seekp(std::ios_base::end);
        }

        if (file.is_open()) {
            size_t nVertex = 0;
            size_t nTriangles = 0;

            file << "# exported with vectiler: https://github.com/karimnaaji/vectiler" << "\n";
            file << "\n";

            int indexOffset = maxindex;

            if (splitMeshes) {
                int meshCnt = 0;

                for (const auto& mesh : meshes) {
                    if (mesh->vertices.size() == 0) { continue; }

                    file << "o mesh" << meshCnt++ << "\n";

                    addPositions(file, *mesh, offsetx, offsety);
                    nVertex += mesh->vertices.size();

                    if (normals) {
                        addNormals(file, *mesh);
                    }

                    addFaces(file, *mesh, indexOffset, normals);
                    nTriangles += mesh->indices.size() / 3;

                    file << "\n";

                    indexOffset += mesh->vertices.size();
                }
            } else {
                file << "o " << outputOBJ << "\n";

                for (const auto& mesh : meshes) {
                    if (mesh->vertices.size() == 0) { continue; }
                    addPositions(file, *mesh, offsetx, offsety);
                    nVertex += mesh->vertices.size();
                }

                if (normals) {
                    for (const auto& mesh : meshes) {
                        if (mesh->vertices.size() == 0) { continue; }
                        addNormals(file, *mesh);
                    }
                }

                for (const auto& mesh : meshes) {
                    if (mesh->vertices.size() == 0) { continue; }
                    addFaces(file, *mesh, indexOffset, normals);
                    indexOffset += mesh->vertices.size();
                    nTriangles += mesh->indices.size() / 3;
                }
            }

            file.close();

            // Print infos
            {
                printf("Saved obj file: %s\n", outputOBJ.c_str());
                printf("Triangles: %ld\n", nTriangles);
                printf("Vertices: %ld\n", nVertex);

                std::ifstream in(outputOBJ, std::ifstream::ate | std::ifstream::binary);
                if (in.is_open()) {
                    int size = (int)in.tellg();
                    printf("File size: %fmb\n", float(size) / (1024 * 1024));
                    in.close();
                }
            }

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
    return "https://tile.mapzen.com/mapzen/vector/v1/all/"
        + std::to_string(tile.z) + "/"
        + std::to_string(tile.x) + "/"
        + std::to_string(tile.y) + ".json?api_key=" + apiKey;
}

inline std::string terrainURL(const Tile& tile, const std::string& apiKey) {
    return "https://tile.mapzen.com/mapzen/terrain/v1/terrarium/"
        + std::to_string(tile.z) + "/"
        + std::to_string(tile.x) + "/"
        + std::to_string(tile.y) + ".png?api_key=" + apiKey;
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
                Tile t(x, y, exportParams.tilez);

                if (x == startx) {
                    t.borders.set(Border::left, 1);
                }
                if (x == endx) {
                    t.borders.set(Border::right, 1);
                }
                if (y == starty) {
                    t.borders.set(Border::top, 1);
                }
                if (y == endy) {
                    t.borders.set(Border::bottom, 1);
                }

                tiles.push_back(t);
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
                std::string url = terrainURL(tile, apiKey);

                auto textureData = downloadHeightmapTile(url, tile,
                    exportParams.terrainExtrusionScale);

                if (!textureData) {
                    printf("Failed to download heightmap texture data for tile %d %d %d\n",
                        tile.x, tile.y, tile.z);
                }

                heightData[tile] = std::move(textureData);
            }

            if (exportParams.buildings || exportParams.roads) {
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
            url = terrainURL(tile, apiKey);

            const auto& textureData = heightData[tile];

            if (!textureData) {
                printf("Failed to download heightmap texture data for tile %d %d %d\n",
                    tile.x, tile.y, tile.z);
            } else {

                /// Extract a plane geometry, vertices in [-1.0,1.0], for terrain mesh
                {
                    auto mesh = std::unique_ptr<PolygonMesh>(new PolygonMesh);

                    buildPlane(mesh->vertices, mesh->indices, 2.0, 2.0,
                        exportParams.terrainSubdivision, exportParams.terrainSubdivision);

                                    // Build terrain mesh extrusion, with bilinear height sampling
                    for (auto& vertex : mesh->vertices) {
                        glm::vec2 tilePosition = glm::vec2(vertex.position.x, vertex.position.y);
                        float extrusion = sampleElevation(tilePosition, textureData);

                        // Scale the height within the tile scale
                        vertex.position.z = extrusion * tile.invScale;
                    }

                    /// Compute faces normals
                    if (exportParams.normals) {
                        computeNormals(*mesh);
                    }

                    mesh->offset = offset;
                    meshes.push_back(std::move(mesh));
                }

                /// Build pedestal
                if (exportParams.pedestal) {
                    auto ground = std::unique_ptr<PolygonMesh>(new PolygonMesh);
                    auto wall = std::unique_ptr<PolygonMesh>(new PolygonMesh);

                    buildPlane(ground->vertices, ground->indices, 2.0, 2.0,
                        exportParams.terrainSubdivision, exportParams.terrainSubdivision, true);

                    for (auto& vertex : ground->vertices) {
                        vertex.position.z = exportParams.pedestalHeight * tile.invScale;
                    }

                    buildPedestalPlanes(tile, wall->vertices, wall->indices, textureData,
                        exportParams.terrainSubdivision, exportParams.pedestalHeight);

                    ground->offset = offset;
                    meshes.push_back(std::move(ground));
                    wall->offset = offset;
                    meshes.push_back(std::move(wall));
                }
            }
        }

        /// Build vector tile mesh
        if (exportParams.buildings || exportParams.roads) {
            const auto& data = vectorTileData[tile];

            if (data) {
                const static std::string keyHeight("height");
                const static std::string keyMinHeight("min_height");

                for (auto layer : data->layers) {
                    for (auto feature : layer.features) {
                        if (textureData && layer.name != "buildings" && layer.name != "roads") { continue; }
                        auto itHeight = feature.props.numericProps.find(keyHeight);
                        auto itMinHeight = feature.props.numericProps.find(keyMinHeight);
                        float scale = tile.invScale * exportParams.buildingsExtrusionScale;
                        double height = 0.0;
                        double minHeight = 0.0;

                        if (layer.name == "buildings") {
                            height = exportParams.buildingsHeight * tile.invScale;
                        }

                        if (itHeight != feature.props.numericProps.end()) {
                            height = itHeight->second * scale;
                        }

                        if (textureData && layer.name != "roads" && height == 0.0) {
                            continue;
                        }

                        if (itMinHeight != feature.props.numericProps.end()) {
                            minHeight = itMinHeight->second * scale;
                        }

                        auto mesh = std::unique_ptr<PolygonMesh>(new PolygonMesh);

                        if (exportParams.buildings) {
                            for (const Polygon& polygon : feature.polygons) {
                                float centroidHeight = 0.f;
                                if (minHeight != height) {
                                    centroidHeight = buildPolygonExtrusion(polygon, minHeight, height,
                                        mesh->vertices, mesh->indices, textureData, tile.invScale);
                                }

                                buildPolygon(polygon, height, mesh->vertices, mesh->indices,
                                    textureData, centroidHeight, tile.invScale);
                            }
                        }

                        if (exportParams.roads) {
                            for (Line& line : feature.lines) {
                                Polygon polygon;
                                float extrude = exportParams.roadsExtrusionWidth * tile.invScale;
                                polygon.emplace_back();
                                Line& polygonLine = polygon.back();

                                if (line.size() == 2) {
                                    glm::vec3 curr = line[0];
                                    glm::vec3 next = line[1];
                                    glm::vec3 n0 = perp(next - curr);

                                    polygonLine.push_back(curr - n0 * extrude);
                                    polygonLine.push_back(curr + n0 * extrude);
                                    polygonLine.push_back(next + n0 * extrude);
                                    polygonLine.push_back(next - n0 * extrude);
                                } else {
                                    glm::vec3 last = line[0];
                                    for (int i = 1; i < line.size() - 1; ++i) {
                                        glm::vec3 curr = line[i];
                                        glm::vec3 next = line[i+1];
                                        addPolygonPolylinePoint(polygonLine, curr, next, last, extrude,
                                            line.size(), i, true);
                                        last = curr;
                                    }

                                    last = line[line.size() - 1];
                                    for (int i = line.size() - 2; i > 0; --i) {
                                        glm::vec3 curr = line[i];
                                        glm::vec3 next = line[i-1];
                                        addPolygonPolylinePoint(polygonLine, curr, next, last, extrude,
                                            line.size(), i, false);
                                        last = curr;
                                    }
                                }

                                if (polygonLine.size() < 4) { continue; }

                                int count = 0;
                                for (int i = 0; i < polygonLine.size(); i++) {
                                    int j = (i + 1) % polygonLine.size();
                                    int k = (i + 2) % polygonLine.size();
                                    double z = (polygonLine[j].x - polygonLine[i].x)
                                             * (polygonLine[k].y - polygonLine[j].y)
                                             - (polygonLine[j].y - polygonLine[i].y)
                                             * (polygonLine[k].x - polygonLine[j].x);
                                    if (z < 0) { count--; }
                                    else if (z > 0) { count++; }
                                }

                                if (count > 0) { // CCW
                                    std::reverse(polygonLine.begin(), polygonLine.end());
                                }

                                // Close the polygon
                                polygonLine.push_back(polygonLine[0]);

                                size_t offset = mesh->vertices.size();

                                if (exportParams.roadsHeight > 0) {
                                    buildPolygonExtrusion(polygon, 0.0, exportParams.roadsHeight * tile.invScale,
                                        mesh->vertices, mesh->indices, nullptr, tile.invScale);
                                }

                                buildPolygon(polygon, exportParams.roadsHeight * tile.invScale, mesh->vertices,
                                    mesh->indices, nullptr, 0.f, tile.invScale);

                                if (textureData) {
                                    for (auto it = mesh->vertices.begin() + offset; it != mesh->vertices.end(); ++it) {
                                        it->position.z += sampleElevation(glm::vec2(it->position.x, it->position.y),
                                            textureData) * tile.invScale;
                                    }
                                }
                            }

                            if (exportParams.normals && exportParams.terrain) {
                                computeNormals(*mesh);
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
        exportParams.append,
        exportParams.normals);

    if (!saved) {
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
