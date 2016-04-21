#pragma once

struct Params {
    const char* filename;
    const char* apiKey;
    const char* tilex;
    const char* tiley;
    int tilez;
    float offset[2];
    bool splitMesh;
    int aoSizeHint;
    int aoSamples;
    bool aoBaking;
    bool append;
    bool terrain;
    int terrainSubdivision;
    float terrainExtrusionScale;
    bool buildings;
    float buildingsExtrusionScale;
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
void subdivideLine(Line& line, float subdivision);
#endif
