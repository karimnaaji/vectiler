#pragma once

#ifdef __cplusplus
extern "C" {
#endif

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
        bool _append);

#ifdef __cplusplus
}
#endif
