#pragma once
#include <stdint.h>

// =========================================================================
// Wireframe models for the hologram screen, generated procedurally.
// Coordinates are in model units (roughly a unit sphere); `radius` is the
// farthest vertex so the renderer can scale each model to the same size.
// =========================================================================

#define HOLO_MAXV 320
#define HOLO_MAXE 340

struct HoloModel {
    float    v[HOLO_MAXV][3];
    uint16_t e[HOLO_MAXE][2];
    uint8_t  w[HOLO_MAXE];      // line width: 1 normal, 2 emphasized
    int      nv, ne;
    float    radius;
};

enum HoloId {
    HOLO_DEATHSTAR,
    HOLO_XWING,
    HOLO_TIE,
    HOLO_CREST,
    HOLO_TATOOINE,
    HOLO_CORUSCANT,
    HOLO_ENDOR,
    HOLO_STARDESTROYER,
    HOLO_R2D2,
    HOLO_HOTH,
    HOLO_COUNT
};

const char* holo_model_name(int id);
const char* holo_model_desc(int id);
void holo_model_build(int id, HoloModel* out);
