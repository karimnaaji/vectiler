#include "flag.h"
#include "flag.c"
#include "objexport.h"

int main(int argc, const char **argv) {
    int tileX = 19294;
    int tileY = 24642;
    int tileZ = 16;
    int sizehint = 512;
    int nsamples = 256;
    bool splitMeshes = false;
    flag_usage("[options]");
    flag_bool(&splitMeshes, "splitMeshes", "Generate one mesh per feature in wavefront file");
    flag_int(&tileX, "tilex", "Tile X");
    flag_int(&tileY, "tiley", "Tile Y");
    flag_int(&tileZ, "tilez", "Tile Z");
    flag_int(&sizehint, "sizehint", "Controls resolution of atlas");
    flag_int(&nsamples, "nsamples", "Quality of ambient occlusion");
    flag_parse(argc, argv, "v" "0.1.0", 0);
    return objexport(tileX, tileY, tileZ, splitMeshes, sizehint, nsamples);
}
