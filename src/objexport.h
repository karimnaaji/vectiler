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
    bool bakeAO;
    bool append;
    bool terrain;
    int terrainSubdivision;
    float terrainExtrusionScale;
};

#ifdef __cplusplus
extern "C" {
#endif

int objexport(struct Params parameters);

#ifdef __cplusplus
}
#endif
