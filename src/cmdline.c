#include "flag.h"
#include "flag.c"
#include "objexport.h"

int main(int argc, const char **argv) {
    int tileX = 19294;
    int tileY = 24642;
    int tileZ = 16;
    float offsetX = 0.f;
    float offsetY = 0.f;
    int sizehint = 512;
    int nsamples = 256;
    int splitMeshes = 0;
    int bakeAO = 0;
    int append = 0;
    const char* name = NULL;

    flag_usage("[options]");
    flag_string(&name, "name", "File name");
    flag_int(&splitMeshes, "splitMeshes", "Generate one mesh per feature in wavefront file");
    flag_int(&tileX, "tilex", "Tile X");
    flag_int(&tileY, "tiley", "Tile Y");
    flag_int(&tileZ, "tilez", "Tile Z");
    flag_float(&offsetX, "offsetx", "Tile Offset on X coordinate");
    flag_float(&offsetY, "offsety", "Tile Offset on Y coordinate");
    flag_int(&bakeAO, "bakeAO", "Generate ambiant occlusion baked atlas");
    flag_int(&append, "append", "Append the obj to an existing obj file");
    flag_int(&sizehint, "sizehint", "Controls resolution of atlas");
    flag_int(&nsamples, "nsamples", "Quality of ambient occlusion");
    flag_parse(argc, argv, "v" "0.1.0", 0);

    if (append && bakeAO) {
        printf("Can't use options --append and --bakeAO altogether");
        printf("Those option are currently exclusive");
        return EXIT_FAILURE;
    }

    return objexport(&name[0],
            tileX,
            tileY,
            tileZ,
            offsetX,
            offsetY,
            (bool)splitMeshes,
            sizehint,
            nsamples,
            (bool)bakeAO,
            (bool)append);
}
