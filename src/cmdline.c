#include "flag.h"
#include "flag.c"
#include "objexport.h"

int main(int argc, const char **argv) {
    // Default parameters
    const char* tileX = "19294";
    const char* tileY = "24642";
    int tileZ = 16;
    float offsetX = 0.f;
    float offsetY = 0.f;
    int sizehint = 512;
    int nsamples = 256;
    int splitMeshes = 0;
    int bakeAO = 0;
    int append = 0;
    int terrain = 0;
    int terrainSubdivision = 64;
    float terrainExtrusionScale = 1.f;
    const char* name = NULL;
    const char* apiKey = "vector-tiles-qVaBcRA";

    // Parse params
    flag_usage("[options]");
    flag_string(&name, "name", "File name");
    flag_string(&apiKey, "apikey", "Developer API key (https://mapzen.com/developers/)");
    flag_int(&splitMeshes, "splitMeshes", "Generate one mesh per feature in wavefront file");
    flag_string(&tileX, "tilex", "Tile X (can be a tile range: 19294/19295)");
    flag_string(&tileY, "tiley", "Tile Y (can be a tile range: 24642/24643)");
    flag_int(&tileZ, "tilez", "Tile Z (zoom)");
    flag_float(&offsetX, "offsetx", "Global tile Offset on X coordinate");
    flag_float(&offsetY, "offsety", "Global tile Offset on Y coordinate");
    flag_int(&append, "append", "Append the obj to an existing obj file");
    flag_int(&terrain, "terrain", "Generate terrain elevation topography");
    flag_int(&terrainSubdivision, "terrainSubdivision", "Terrain mesh subdivision");
    flag_float(&terrainExtrusionScale, "terrainExtrusionScale", "Terrain mesh extrusion scale");
    flag_int(&bakeAO, "bakeAO", "Generate ambiant occlusion baked atlas");
    flag_int(&sizehint, "sizehint", "Controls resolution of atlas");
    flag_int(&nsamples, "nsamples", "Quality of ambient occlusion");
    flag_parse(argc, argv, "v" "0.1.0", 0);

    if (append && bakeAO) {
        printf("Can't use options --append and --bakeAO altogether");
        printf("Those option are currently exclusive");
        return EXIT_FAILURE;
    }

    struct Params parameters = {&name[0], &apiKey[0], tileX, tileY, tileZ, {offsetX, offsetY},
        (bool)splitMeshes, sizehint, nsamples, (bool)bakeAO, (bool)append, (bool)terrain,
        terrainSubdivision, terrainExtrusionScale};

    return objexport(parameters);
}
