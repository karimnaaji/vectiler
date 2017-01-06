#ifndef VECTILER_H
#define VECTILER_H

struct Params {
    const char* filename;
    const char* apiKey;
    const char* tilex;
    const char* tiley;
    int tilez;
    float offset[2];
    bool splitMesh;
    bool append;
    bool terrain;
    int terrainSubdivision;
    float terrainExtrusionScale;
    bool buildings;
    float buildingsExtrusionScale;
    bool roads;
    float roadsHeight;
    float roadsExtrusionWidth;
    bool normals;
    float buildingsHeight;
    int pedestal;
    float pedestalHeight;
};

#ifdef __cplusplus
extern "C" {
#endif

int vectiler(struct Params parameters);

#ifdef __cplusplus
}
#endif

#ifdef VECTILER_UNIT_TESTS
#include "tiledata.h"
#endif

#endif
