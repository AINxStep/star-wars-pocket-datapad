#include "holo_models.h"
#include "holo_crest.h"
#include "i18n.h"
#include <math.h>
#include <string.h>

static HoloModel* M;

// -------------------------------------------------------------------------
// Primitives
// -------------------------------------------------------------------------
static int V(float x, float y, float z) {
    if (M->nv >= HOLO_MAXV) return M->nv - 1;     // full: degenerate, harmless
    M->v[M->nv][0] = x; M->v[M->nv][1] = y; M->v[M->nv][2] = z;
    return M->nv++;
}

static void E(int a, int b, uint8_t w = 1) {
    if (M->ne >= HOLO_MAXE) return;
    M->e[M->ne][0] = a; M->e[M->ne][1] = b; M->w[M->ne] = w;
    M->ne++;
}

static const float AX_X[3] = {1, 0, 0}, AX_Y[3] = {0, 1, 0}, AX_Z[3] = {0, 0, 1};

// Circle centered at c in the plane spanned by unit vectors u and w; returns its first vertex
static int ring(const float c[3], const float u[3], const float w[3], float r, int segs, uint8_t wd = 1) {
    int first = M->nv, prev = -1;
    for (int i = 0; i < segs; i++) {
        float a = 2.0f * (float)M_PI * i / segs, cs = cosf(a) * r, sn = sinf(a) * r;
        int idx = V(c[0] + u[0] * cs + w[0] * sn, c[1] + u[1] * cs + w[1] * sn, c[2] + u[2] * cs + w[2] * sn);
        if (prev >= 0) E(prev, idx, wd);
        prev = idx;
    }
    E(prev, first, wd);
    return first;
}

static int ring_y(float cx, float cy, float cz, float r, int segs, uint8_t wd = 1) {   // horizontal
    float c[3] = {cx, cy, cz};
    return ring(c, AX_X, AX_Z, r, segs, wd);
}
static int ring_x(float cx, float cy, float cz, float r, int segs, uint8_t wd = 1) {   // normal = x
    float c[3] = {cx, cy, cz};
    return ring(c, AX_Y, AX_Z, r, segs, wd);
}
static int ring_z(float cx, float cy, float cz, float r, int segs, uint8_t wd = 1) {   // normal = z
    float c[3] = {cx, cy, cz};
    return ring(c, AX_X, AX_Y, r, segs, wd);
}

// Latitude rings (equator-ish ones are emphasized) plus meridians
static void sphere(float cx, float cy, float cz, float R, const float* lats, int nlat, int rsegs,
                   int nmer, int msegs) {
    for (int i = 0; i < nlat; i++) {
        float phi = lats[i] * (float)M_PI / 180.0f;
        ring_y(cx, cy + R * sinf(phi), cz, R * cosf(phi), rsegs, fabsf(lats[i]) < 6 ? 2 : 1);
    }
    for (int m = 0; m < nmer; m++) {
        float th = 2.0f * (float)M_PI * m / nmer;
        int prev = -1;
        for (int k = 0; k <= msegs; k++) {
            float phi = -(float)M_PI / 2 + (float)M_PI * k / msegs;
            int idx = V(cx + R * cosf(phi) * cosf(th), cy + R * sinf(phi), cz + R * cosf(phi) * sinf(th));
            if (prev >= 0) E(prev, idx);
            prev = idx;
        }
    }
}

// Box centered at (cx,cy,cz) with half sizes, rotated about z
static void box(float cx, float cy, float cz, float hx, float hy, float hz, float rz = 0, uint8_t wd = 1) {
    int b = M->nv;
    float c = cosf(rz), s = sinf(rz);
    for (int i = 0; i < 8; i++) {
        float x = (i & 1) ? hx : -hx, y = (i & 2) ? hy : -hy, z = (i & 4) ? hz : -hz;
        V(cx + x * c - y * s, cy + x * s + y * c, cz + z);
    }
    static const uint8_t ed[12][2] = {{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    for (int i = 0; i < 12; i++) E(b + ed[i][0], b + ed[i][1], wd);
}

static void line(float x1, float y1, float z1, float x2, float y2, float z2, uint8_t wd = 1) {
    E(V(x1, y1, z1), V(x2, y2, z2), wd);
}

// Small three-axis cross (stars, tiny ships)
static void cross(float cx, float cy, float cz, float r, uint8_t wd = 1) {
    line(cx - r, cy, cz, cx + r, cy, cz, wd);
    line(cx, cy - r, cz, cx, cy + r, cz, wd);
    line(cx, cy, cz - r, cx, cy, cz + r, wd);
}

// Ring tilted about the x axis by `tilt` radians
static void tilted_ring(float R, float tilt, int segs, uint8_t wd = 1) {
    float w[3] = {0, sinf(tilt), cosf(tilt)}, c[3] = {0, 0, 0};
    ring(c, AX_X, w, R, segs, wd);
}

// -------------------------------------------------------------------------
// Models
// -------------------------------------------------------------------------
static void build_deathstar() {
    static const float lats[] = {-60, -30, -4, 4, 30, 60};
    sphere(0, 0, 0, 1.0f, lats, 6, 16, 8, 12);

    // Superlaser dish: rings on the surface around direction n
    float la = 25.0f * (float)M_PI / 180.0f, lo = 40.0f * (float)M_PI / 180.0f;
    float n[3] = {cosf(la) * cosf(lo), sinf(la), cosf(la) * sinf(lo)};
    float ux = n[2], uy = 0, uz = -n[0];                       // n x up, normalized
    float ul = sqrtf(ux * ux + uz * uz);
    float u[3] = {ux / ul, uy, uz / ul};
    float w[3] = {n[1] * u[2] - n[2] * u[1], n[2] * u[0] - n[0] * u[2], n[0] * u[1] - n[1] * u[0]};
    float c[3] = {n[0] * 0.99f, n[1] * 0.99f, n[2] * 0.99f};
    ring(c, u, w, 0.27f, 14, 2);
    int inner = ring(c, u, w, 0.12f, 8, 1);
    int hub = V(c[0], c[1], c[2]);
    for (int k = 0; k < 8; k += 2) E(hub, inner + k);
}

static void build_xwing() {
    // Fuselage and tapered nose (forward = +z)
    box(0, 0, -0.325f, 0.12f, 0.10f, 0.575f);
    int f = M->nv;
    V(-0.12f, -0.10f, 0.25f); V(0.12f, -0.10f, 0.25f); V(-0.12f, 0.10f, 0.25f); V(0.12f, 0.10f, 0.25f);
    V(-0.04f, -0.03f, 0.95f); V(0.04f, -0.03f, 0.95f); V(-0.04f, 0.03f, 0.95f); V(0.04f, 0.03f, 0.95f);
    static const uint8_t nose[8][2] = {{4,5},{6,7},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    for (int i = 0; i < 8; i++) E(f + nose[i][0], f + nose[i][1]);

    // Four S-foils in an X, an engine at each tip and a cannon along each wing
    for (int sx = -1; sx <= 1; sx += 2) {
        for (int sy = -1; sy <= 1; sy += 2) {
            float a = sx * sy * 14.0f * (float)M_PI / 180.0f;
            float cx = sx * 0.60f, cy = sy * 0.10f;
            box(cx, cy, -0.55f, 0.52f, 0.015f, 0.28f, a);
            float tx = cx + sx * 0.52f * cosf(a), ty = cy + sx * 0.52f * sinf(a);
            box(tx, ty, -0.45f, 0.07f, 0.07f, 0.22f, 0, 2);
            line(tx, ty, -0.27f, tx, ty, 0.62f);
        }
    }
    box(0, 0.15f, -0.05f, 0.07f, 0.05f, 0.18f);            // cockpit
    box(0, 0.16f, -0.45f, 0.05f, 0.05f, 0.07f);            // astromech
}

static void build_tie() {
    ring_x(0, 0, 0, 0.28f, 12);
    ring_y(0, 0, 0, 0.28f, 12, 2);
    ring_z(0, 0, 0, 0.28f, 12);

    for (int sx = -1; sx <= 1; sx += 2) {
        float x = sx * 0.78f;
        line(sx * 0.28f, 0, 0, x, 0, 0, 2);                 // strut
        int outer = M->nv;
        for (int k = 0; k < 6; k++) {                       // hexagonal panel
            float th = (float)M_PI / 2 + k * (float)M_PI / 3;
            V(x, 0.85f * sinf(th), 0.85f * cosf(th));
        }
        int inner = M->nv;
        for (int k = 0; k < 6; k++) {
            float th = (float)M_PI / 2 + k * (float)M_PI / 3;
            V(x, 0.42f * sinf(th), 0.42f * cosf(th));
        }
        int hub = V(x, 0, 0);
        for (int k = 0; k < 6; k++) {
            E(outer + k, outer + (k + 1) % 6, 2);
            E(inner + k, inner + (k + 1) % 6);
            E(outer + k, inner + k);
            E(hub, inner + k);
        }
    }
}

static void build_crest() {
    // Two thin layers of the outline joined by struts: a "thick" hologram
    const float depth = 0.07f, R = 0.95f;
    int start = 0;
    for (int p = 0; p < HOLO_CREST_POLYS; p++) {
        int end = HOLO_CREST_END[p], n = end - start;
        int front = M->nv;
        for (int i = start; i < end; i++) V(HOLO_CREST_PTS[i][0] / 1000.0f * R, HOLO_CREST_PTS[i][1] / 1000.0f * R, depth);
        int back = M->nv;
        for (int i = start; i < end; i++) V(HOLO_CREST_PTS[i][0] / 1000.0f * R, HOLO_CREST_PTS[i][1] / 1000.0f * R, -depth);
        for (int i = 0; i < n; i++) {
            E(front + i, front + (i + 1) % n, 2);
            E(back + i, back + (i + 1) % n);
            E(front + i, back + i);
        }
        start = end;
    }
}

static void build_tatooine() {
    static const float lats[] = {-50, -25, 0, 25, 50};
    sphere(0, 0, 0, 0.8f, lats, 5, 18, 8, 12);
    cross(1.05f, 0.45f, 0.1f, 0.12f, 2);                    // twin suns
    cross(1.0f, 0.15f, -0.25f, 0.09f, 2);
    ring_y(0.45f, 0.62f, 0.55f, 0.05f, 8);                  // moons
    ring_y(-0.55f, -0.7f, 0.5f, 0.04f, 8);
}

static void build_coruscant() {
    static const float lats[] = {-60, -40, -20, 0, 20, 40, 60};
    sphere(0, 0, 0, 0.75f, lats, 7, 16, 12, 10);            // dense grid: a city planet
    tilted_ring(1.0f, 0.5f, 24, 2);                          // orbital traffic lane
    cross(0.9f, 0.5f, 0.25f, 0.05f);
    cross(-0.8f, -0.55f, -0.5f, 0.05f);
}

static void build_endor() {
    static const float lats[] = {-40, 0, 40};
    sphere(0, 0, 0, 0.55f, lats, 3, 16, 6, 10);
    tilted_ring(0.9f, -0.35f, 24);
    // Second Death Star under construction
    static const float dl[] = {-20, 20};
    sphere(0.95f, 0.25f, 0.15f, 0.2f, dl, 2, 10, 4, 6);
    ring_y(0.95f, 0.25f, 0.15f, 0.22f, 12, 2);
}

static void build_stardestroyer() {
    // Dagger hull: pointed bow (+z), wide flat stern (-z)
    int tip = V(0, 0, 0.95f);
    int rtl = V(-0.58f, 0.12f, -0.70f), rtr = V(0.58f, 0.12f, -0.70f);
    int rbl = V(-0.58f, -0.10f, -0.70f), rbr = V(0.58f, -0.10f, -0.70f);
    E(tip, rtl, 2); E(tip, rtr, 2);                 // dorsal silhouette
    E(tip, rbl);    E(tip, rbr);                    // ventral silhouette
    E(rtl, rtr, 2); E(rbl, rbr, 2);                 // stern edges
    E(rtl, rbl, 2); E(rtr, rbr, 2);

    // Keel line and dorsal ridge
    line(0, -0.10f, -0.70f, 0, -0.04f, 0.60f);
    line(0, 0.12f, -0.70f, 0, 0.06f, 0.60f);

    // Command superstructure and bridge tower
    box(0, 0.17f, -0.56f, 0.20f, 0.05f, 0.12f);
    box(0, 0.26f, -0.62f, 0.05f, 0.04f, 0.04f);
    cross(-0.09f, 0.31f, -0.62f, 0.035f, 2);        // shield globes
    cross( 0.09f, 0.31f, -0.62f, 0.035f, 2);

    // Three main engines on the stern
    cross(-0.30f, 0.01f, -0.73f, 0.05f, 2);
    cross( 0.00f, 0.01f, -0.73f, 0.05f, 2);
    cross( 0.30f, 0.01f, -0.73f, 0.05f, 2);
}

static void build_r2d2() {
    // Barrel body: stacked rings joined by verticals; the top ring doubles as
    // the dome's equator. R2 is taller than wide, so keep the radius slim.
    int low  = ring_y(0, -0.60f, 0, 0.30f, 12);
    int mid  = ring_y(0, -0.22f, 0, 0.30f, 12);
    int top  = ring_y(0,  0.12f, 0, 0.30f, 12, 2);
    for (int k = 0; k < 12; k++) { E(low + k, mid + k); E(mid + k, top + k); }

    // Hemisphere dome: latitude rings of a sphere centered on the top ring
    // (base at lat 0deg, so the dome reads as a cap, not a cone)
    int d1   = ring_y(0, 0.27f, 0, 0.26f, 12);                // lat +30deg
    int d2   = ring_y(0, 0.38f, 0, 0.15f, 8);                 // lat +60deg
    int apex = V(0, 0.42f, 0);
    for (int k = 0; k < 12; k++) E(top + k, d1 + k);
    for (int k = 0; k < 8;  k++) E(d1 + (k * 12) / 8, d2 + k);
    for (int k = 0; k < 8;  k++) E(d2 + k, apex, 2);

    // Eye photoreceptor: small circle facing forward
    ring_z(0, 0.30f, 0.22f, 0.05f, 8, 2);

    // Side legs splayed outward with shoulder pivots and wide feet
    for (int sx = -1; sx <= 1; sx += 2) {
        box(sx * 0.33f,  0.05f, 0, 0.05f,  0.08f, 0.09f);               // shoulder pivot
        box(sx * 0.36f, -0.30f, 0, 0.075f, 0.30f, 0.10f, sx * 0.20f);   // angled leg
        box(sx * 0.42f, -0.72f, 0, 0.11f,  0.06f, 0.13f, 0, 2);         // foot
    }
    box(0, -0.70f, 0.02f, 0.06f, 0.09f, 0.09f);                         // retractable third leg
}

static void build_hoth() {
    static const float lats[] = {-55, -30, 0, 30, 55};
    sphere(0, 0, 0, 0.65f, lats, 5, 16, 8, 10);

    // Asteroid belt: tilted ring plus scattered rocks along it
    const float tilt = 0.35f, R = 1.0f;
    tilted_ring(R, tilt, 20, 2);
    for (int i = 0; i < 8; i++) {
        float a = i * 0.785f;
        cross(R * cosf(a), R * sinf(a) * sinf(tilt), R * sinf(a) * cosf(tilt),
              0.04f + (i % 3) * 0.015f);
    }
}

// -------------------------------------------------------------------------
// Catalogue
// -------------------------------------------------------------------------
struct HoloInfo { const char* name; const char* desc; void (*build)(void); };

static const HoloInfo infos[HOLO_COUNT] = {
    { "DEATH STAR",  "DS-1 ESTACION DE BATALLA", build_deathstar },
    { "X-WING",      "CAZA ESTELAR REBELDE",     build_xwing },
    { "TIE/LN",      "CAZA ESTELAR IMPERIAL",    build_tie },
    { "REBEL CREST", "SIMBOLO DE LA ALIANZA",    build_crest },
    { "TATOOINE",    "DESIERTO // DOS SOLES",    build_tatooine },
    { "CORUSCANT",   "PLANETA-CIUDAD",           build_coruscant },
    { "ENDOR",       "LUNA BOSCOSA",             build_endor },
    { "STAR DESTROYER", "DESTRUCTOR ESTELAR",    build_stardestroyer },
    { "R2-D2",       "DROIDE ASTROMEC",          build_r2d2 },
    { "HOTH",        "PLANETA HELADO",           build_hoth },
};

const char* holo_model_name(int id) { return infos[id % HOLO_COUNT].name; }
const char* holo_model_desc(int id) { return tr((StrId)(S_HOLO_DESC_0 + id % HOLO_COUNT)); }

void holo_model_build(int id, HoloModel* out) {
    M = out;
    M->nv = M->ne = 0;
    infos[id % HOLO_COUNT].build();

    float r = 0.01f;
    for (int i = 0; i < M->nv; i++) {
        float d = sqrtf(M->v[i][0] * M->v[i][0] + M->v[i][1] * M->v[i][1] + M->v[i][2] * M->v[i][2]);
        if (d > r) r = d;
    }
    M->radius = r;
}
