#include "flag.h"
#include "flag.c"
#include "vectiler.h"

int main(int argc, const char **argv) {
    // Default parameters
    const char* tileX = "19294";
    const char* tileY = "24642";
    int tileZ = 16;
    float offsetX = 0.f;
    float offsetY = 0.f;
    int splitMeshes = 0;
    int append = 0;
    int pedestal = 0;
    float pedestalHeight = 0.f;
    int terrain = 0;
    int terrainSubdivision = 64;
    float terrainExtrusionScale = 1.f;
    int buildings = 1;
    float buildingsExtrusionScale = 1.f;
    float buildingsHeight = 0.f;
    int roads = 0;
    float roadsHeight = 1.f;
    float roadsExtrusionWidth = 5.f;
    int normals = 0;
    const char* name = NULL;
    const char* apiKey = "xwlF66_oRKWWb058St_Q9Q";

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
    flag_int(&buildings, "buildings", "Whether to export building geometry");
    flag_float(&buildingsExtrusionScale, "buildingsExtrusionScale", "Building height scale factor");
    flag_float(&buildingsHeight, "buildingsHeight", "The height at which building should be extruded (if no height data is available)");
    flag_int(&pedestal, "pedestal", "Build a pedestal when running with terrain option (Useful for 3d printing)");
    flag_float(&pedestalHeight, "pedestalHeight", "Pedestal height, can be negative");
    flag_int(&terrain, "terrain", "Generate terrain elevation topography");
    flag_int(&terrainSubdivision, "terrainSubdivision", "Terrain mesh subdivision");
    flag_float(&terrainExtrusionScale, "terrainExtrusionScale", "Terrain mesh extrusion scale");
    flag_int(&roads, "roads", "Whether to export roads geometry");
    flag_float(&roadsHeight, "roadsHeight", "The roads height offset (z-axis)");
    flag_float(&roadsExtrusionWidth, "roadsExtrusionWidth", "The roads extrusion width");
    flag_int(&normals, "normals", "Export with normals");
    flag_parse(argc, argv, "v" "0.1.0", 0);

    struct Params parameters = {
        &name[0],
        &apiKey[0],
        tileX,
        tileY,
        tileZ,
        {offsetX, offsetY},
        (bool)splitMeshes,
        (bool)append,
        (bool)terrain,
        terrainSubdivision,
        terrainExtrusionScale,
        (bool)buildings,
        buildingsExtrusionScale,
        (bool)roads,
        roadsHeight,
        roadsExtrusionWidth,
        (bool)normals,
        buildingsHeight,
        pedestal,
        pedestalHeight
    };

    if (!parameters.terrain && parameters.pedestal) {
        printf("Pedestal parameters can only be given when exporting with terrain (--terrain)\n");
        return EXIT_FAILURE;
    }

    return vectiler(parameters);
}
