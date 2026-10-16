#include "../ui_common.h"
#include "../wirecanvas.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

static lv_obj_t* scr_radar;
static lv_obj_t* radar_obj;
static lv_obj_t* card_radar;
static lv_obj_t* lbl_radar_info;
static lv_timer_t* radar_timer;

// =========================================================================
// Radar simulation shown as a holographic wireframe sphere: contacts fly in
// 3D around the datapad (the centre); a luminous meridian sweeps round the
// globe, lighting up each contact it crosses; contacts then fade out.
// Drag to turn the globe, like the hologram projector.
// =========================================================================
// Viewport geometry: 240x166 on the portrait layout, the left part of the
// screen on the wide one (globe radius and markers scale with it)
static int VW = 240, VH = 166;
static int RCX = 120, RCY = 84;      // globe centre inside the viewport
static float RADAR_S = 66.0f;        // globe radius in px
static float K = 1.0f;               // scale relative to the portrait design
#define KI(v) ((int)((v) * K + 0.5f))
#define RADAR_D          4.0f        // camera distance in globe radii (perspective)
#define DEG              (3.14159265f / 180.0f)
#define RADAR_SWEEP_DPS  80.0f       // beam speed, degrees per second
#define RADAR_TRAIL      6           // trail meridians behind the beam
#define RADAR_TRAIL_DEG  7.0f        // spacing between trail meridians
#define RADAR_FADE_MS    3500        // contact fade-out after being swept
#define RADAR_PING_MS    700         // expanding ring when a contact is detected
#define RADAR_KM         30.0f       // globe radius expressed in km
#define N_CONTACTS       5           // first 2 are allies, the rest hostile
#define SEG              20          // segments per drawn circle

struct RadarContact {
    float x, y, z;     // -1..1 inside the globe: x east, y south, z up
    float vx, vy, vz;  // units per second
    bool hostile;
    bool revealed;
    uint32_t hit_ms;   // last time the beam swept it
};

static RadarContact contacts[N_CONTACTS];
static float sweep_deg = 0;
static float yaw = 15, pitch = 22;       // view orientation (drag to change)
static bool pressing = false;
static uint32_t radar_last_tick = 0, last_touch = 0;
static int flicker = 255, scan_off = 0;
static char radar_card_text[96] = "";
static bool radar_card_alert = false;

static float frand(void) { return lv_rand(0, 10000) / 10000.0f; }

static void contact_spawn(RadarContact& c, bool at_edge) {
    float az = frand() * 2.0f * (float)M_PI;
    float el = (frand() - 0.5f) * 1.8f;                        // about +-50 deg
    float r = at_edge ? 0.97f : 0.2f + frand() * 0.7f;
    c.x = r * cosf(el) * sinf(az);
    c.y = -r * cosf(el) * cosf(az);
    c.z = r * sinf(el);
    // Entering contacts head roughly towards the centre
    float speed = 0.04f + frand() * 0.05f;
    float dx, dy, dz;
    if (at_edge) {
        dx = -c.x + (frand() - 0.5f) * 0.9f;
        dy = -c.y + (frand() - 0.5f) * 0.9f;
        dz = -c.z + (frand() - 0.5f) * 0.9f;
    } else {
        dx = frand() - 0.5f; dy = frand() - 0.5f; dz = (frand() - 0.5f) * 0.6f;
    }
    float n = sqrtf(dx * dx + dy * dy + dz * dz) + 0.0001f;
    c.vx = dx / n * speed;
    c.vy = dy / n * speed;
    c.vz = dz / n * speed;
    c.revealed = false;
    c.hit_ms = 0;
}

static float contact_bearing(const RadarContact& c) {
    float b = atan2f(c.x, -c.y) * 180.0f / (float)M_PI;
    return b < 0 ? b + 360.0f : b;
}

static uint8_t contact_intensity(const RadarContact& c, uint32_t now) {
    if (!c.revealed) return 0;
    uint32_t age = now - c.hit_ms;
    if (age >= RADAR_FADE_MS) return 0;
    return (uint8_t)(255 - (255UL * age) / RADAR_FADE_MS);
}

static void radar_update_card(uint32_t now) {
    int hostiles = 0, allies = 0, bearing = 0;
    float best_r = 9.0f;
    for (int i = 0; i < N_CONTACTS; i++) {
        if (contact_intensity(contacts[i], now) == 0) continue;
        if (contacts[i].hostile) {
            hostiles++;
            float r = sqrtf(contacts[i].x * contacts[i].x + contacts[i].y * contacts[i].y + contacts[i].z * contacts[i].z);
            if (r < best_r) { best_r = r; bearing = (int)contact_bearing(contacts[i]); }
        } else {
            allies++;
        }
    }

    char buf[sizeof(radar_card_text)];
    if (hostiles) {
        snprintf(buf, sizeof(buf), tr(S_RADAR_HOSTILE_FMT), best_r * RADAR_KM, bearing, allies);
    } else {
        snprintf(buf, sizeof(buf), tr(S_RADAR_CLEAR_FMT), allies);
    }
    if (strcmp(buf, radar_card_text) != 0) {
        strcpy(radar_card_text, buf);
        lv_label_set_text(lbl_radar_info, buf);
    }

    bool alert = hostiles > 0;
    if (alert != radar_card_alert) {
        radar_card_alert = alert;
        lv_obj_set_style_border_color(card_radar, alert ? COLOR_ALERT : COLOR_DATA, 0);
    }
}

static void radar_build_wide(void);

static void radar_timer_cb(lv_timer_t* timer) {
    uint32_t now = lv_tick_get();
    if (lv_scr_act() != scr_radar) {   // paused off-screen; avoid a time jump on return
        radar_last_tick = now;
        return;
    }

    float dt = (now - radar_last_tick) / 1000.0f;
    radar_last_tick = now;
    if (dt > 0.2f) dt = 0.2f;

    if (!pressing && now - last_touch > 1500) yaw = fmodf(yaw + 9.0f * dt, 360.0f);
    flicker = 225 + lv_rand(0, 30);
    scan_off = (now / 70) % 5;

    float step = RADAR_SWEEP_DPS * dt;
    float prev = sweep_deg;
    sweep_deg = fmodf(sweep_deg + step, 360.0f);

    for (int i = 0; i < N_CONTACTS; i++) {
        RadarContact& c = contacts[i];
        c.x += c.vx * dt;
        c.y += c.vy * dt;
        c.z += c.vz * dt;

        // Left the globe heading outwards: a new contact enters from the edge
        if (c.x * c.x + c.y * c.y + c.z * c.z > 1.0f && (c.x * c.vx + c.y * c.vy + c.z * c.vz) > 0) {
            contact_spawn(c, true);
            continue;
        }

        // Did the beam cross this contact since the last frame?
        float d = fmodf(contact_bearing(c) - prev + 360.0f, 360.0f);
        if (d > 0 && d <= step) {
            c.revealed = true;
            c.hit_ms = now;
        }
    }

    radar_update_card(now);
    if (UI_WIDE) radar_build_wide();
    lv_obj_invalidate(radar_obj);
}

// -------------------------------------------------------------------------
// Drawing (same projection style as the hologram projector)
// -------------------------------------------------------------------------
static float m_cy, m_sy, m_cp, m_sp;     // view rotation, refreshed per frame

// Globe-space point (x east, y up, z south) -> viewport pixels + depth (1 = near)
static void proj(float x, float y, float z, int* sx, int* sy, float* depth, float* k) {
    float x1 = x * m_cy + z * m_sy, z1 = -x * m_sy + z * m_cy;
    float y2 = y * m_cp - z1 * m_sp, z2 = y * m_sp + z1 * m_cp;
    float kk = RADAR_D / (RADAR_D - z2);
    *sx = (int)(RCX + x1 * kk * RADAR_S);
    *sy = (int)(RCY - y2 * kk * RADAR_S);
    *depth = z2;
    if (k) *k = kk;
}

static lv_opa_t fl(float opa) {
    int o = (int)(opa * flicker / 255);
    return (lv_opa_t)(o > 255 ? 255 : (o < 0 ? 0 : o));
}

// Polyline of a circle/arc given by a function of the angle, with depth-faded opacity
typedef void (*CurveFn)(float t, float param, float* x, float* y, float* z);
static void curve_lat(float t, float lat, float* x, float* y, float* z) {   // parallel at latitude `lat`
    *x = cosf(lat) * sinf(t); *y = sinf(lat); *z = -cosf(lat) * cosf(t);
}
static void curve_mer(float t, float az, float* x, float* y, float* z) {    // meridian at azimuth `az`, t = latitude
    *x = cosf(t) * sinf(az); *y = sinf(t); *z = -cosf(t) * cosf(az);
}

static void draw_curve(lv_draw_ctx_t* ctx, lv_draw_line_dsc_t* ln, lv_coord_t ox, lv_coord_t oy,
                       CurveFn fn, float param, float t0, float t1, int segs, float base_opa, float depth_gain) {
    int px_ = 0, py_ = 0;
    float pd = 0;
    for (int i = 0; i <= segs; i++) {
        float x, y, z, d;
        int sx, sy;
        fn(t0 + (t1 - t0) * i / segs, param, &x, &y, &z);
        proj(x, y, z, &sx, &sy, &d, NULL);
        if (i > 0) {
            float dm = (d + pd) * 0.5f;                         // -1 far .. 1 near
            ln->opa = fl(base_opa * (1.0f - depth_gain + depth_gain * (dm + 1.0f) * 0.5f));
            lv_point_t p1 = {(lv_coord_t)(ox + px_), (lv_coord_t)(oy + py_)};
            lv_point_t p2 = {(lv_coord_t)(ox + sx), (lv_coord_t)(oy + sy)};
            if (!ui_line_clipped(ctx, p1.x, p1.y, p2.x, p2.y, 3)) lv_draw_line(ctx, ln, &p1, &p2);
        }
        px_ = sx; py_ = sy; pd = d;
    }
}

static void radar_draw_cb(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target(e);
    lv_draw_ctx_t* ctx = lv_event_get_draw_ctx(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    lv_coord_t ox = a.x1, oy = a.y1;
    uint32_t now = lv_tick_get();
    const float PI_ = 3.14159265f;

    m_cy = cosf(yaw * DEG); m_sy = sinf(yaw * DEG);
    m_cp = cosf(pitch * DEG); m_sp = sinf(pitch * DEG);

    // Projection chamber
    lv_draw_rect_dsc_t bg;
    lv_draw_rect_dsc_init(&bg);
    bg.bg_color = COLOR_CHAMBER;
    bg.bg_opa = LV_OPA_COVER;
    lv_draw_rect(ctx, &bg, &a);

    lv_draw_line_dsc_t ln;
    lv_draw_line_dsc_init(&ln);
    ln.color = COLOR_HOLO;
    ln.width = 1;

    // Globe wireframe: equator and two parallels, four great circles through the poles
    ln.width = K > 1.5f ? 3 : 2;
    draw_curve(ctx, &ln, ox, oy, curve_lat, 0.0f, 0, 2 * PI_, SEG, 150, 0.7f);
    ln.width = 1;
    draw_curve(ctx, &ln, ox, oy, curve_lat, 40.0f * DEG, 0, 2 * PI_, SEG, 90, 0.7f);
    draw_curve(ctx, &ln, ox, oy, curve_lat, -40.0f * DEG, 0, 2 * PI_, SEG, 90, 0.7f);
    for (int i = 0; i < 4; i++) {
        float az = i * 45.0f * DEG;
        draw_curve(ctx, &ln, ox, oy, curve_mer, az, -PI_ / 2, PI_ / 2, SEG / 2, 90, 0.7f);
        draw_curve(ctx, &ln, ox, oy, curve_mer, az + PI_, -PI_ / 2, PI_ / 2, SEG / 2, 90, 0.7f);
    }

    // Sweep: a bright meridian turning round the globe, with a fading trail
    for (int k = RADAR_TRAIL; k >= 0; k--) {
        float az = (sweep_deg - k * RADAR_TRAIL_DEG) * DEG;
        if (k == 0) {   // soft glow under the beam's bright line
            ln.color = COLOR_HILITE;
            ln.width = 4;
            draw_curve(ctx, &ln, ox, oy, curve_mer, az, -PI_ / 2, PI_ / 2, SEG / 2, 50, 0.4f);
        }
        ln.color = k == 0 ? COLOR_HILITE : COLOR_HOLO;
        ln.width = k == 0 ? 2 : 1;
        float op = k == 0 ? 255 : 150.0f * (RADAR_TRAIL - k + 1) / (RADAR_TRAIL + 1);
        draw_curve(ctx, &ln, ox, oy, curve_mer, az, -PI_ / 2, PI_ / 2, SEG / 2, op, 0.4f);
    }

    // North marker (amber) at the pole, and our own position at the centre
    int nx, ny, cxp, cyp;
    float nd, cd, kk;
    proj(0, 0, -1.0f, &nx, &ny, &nd, &kk);
    lv_draw_rect_dsc_t dot;
    lv_draw_rect_dsc_init(&dot);
    dot.bg_color = COLOR_ACCENT;
    dot.bg_opa = LV_OPA_COVER;
    dot.radius = LV_RADIUS_CIRCLE;
    lv_area_t na = {(lv_coord_t)(ox + nx - KI(3)), (lv_coord_t)(oy + ny - KI(3)), (lv_coord_t)(ox + nx + KI(3)), (lv_coord_t)(oy + ny + KI(3))};
    lv_draw_rect(ctx, &dot, &na);
    proj(0, 0, 0, &cxp, &cyp, &cd, NULL);
    dot.bg_color = COLOR_HILITE;
    dot.bg_opa = LV_OPA_80;
    lv_area_t ca = {(lv_coord_t)(ox + cxp - KI(3)), (lv_coord_t)(oy + cyp - KI(3)), (lv_coord_t)(ox + cxp + KI(3)), (lv_coord_t)(oy + cyp + KI(3))};
    lv_draw_rect(ctx, &dot, &ca);

    // Contacts, farthest first so nearer ones overdraw
    int order[N_CONTACTS];
    float cdepth[N_CONTACTS];
    int csx[N_CONTACTS], csy[N_CONTACTS];
    float ck[N_CONTACTS];
    for (int i = 0; i < N_CONTACTS; i++) {
        order[i] = i;
        proj(contacts[i].x, contacts[i].z, contacts[i].y, &csx[i], &csy[i], &cdepth[i], &ck[i]);
    }
    for (int i = 1; i < N_CONTACTS; i++) {
        int k = order[i], j = i - 1;
        while (j >= 0 && cdepth[order[j]] > cdepth[k]) { order[j + 1] = order[j]; j--; }
        order[j + 1] = k;
    }
    for (int n = 0; n < N_CONTACTS; n++) {
        int i = order[n];
        const RadarContact& c = contacts[i];
        uint8_t inten = contact_intensity(c, now);
        if (inten == 0) continue;
        lv_color_t col = c.hostile ? COLOR_ALERT : COLOR_FRIEND;
        float near_k = 0.6f + 0.4f * (cdepth[i] + 1.0f) * 0.5f;      // depth cue
        lv_opa_t op = (lv_opa_t)(inten * near_k);
        int px = ox + csx[i], py = oy + csy[i];

        // Line of sight back to the centre
        ln.color = col;
        ln.width = 1;
        ln.opa = op / 3;
        lv_point_t p1 = {(lv_coord_t)px, (lv_coord_t)py};
        lv_point_t p2 = {(lv_coord_t)(ox + cxp), (lv_coord_t)(oy + cyp)};
        lv_draw_line(ctx, &ln, &p1, &p2);

        uint32_t age = now - c.hit_ms;
        if (age < RADAR_PING_MS) {             // detection ring expanding around the contact
            lv_draw_rect_dsc_t ping;
            lv_draw_rect_dsc_init(&ping);
            ping.bg_opa = LV_OPA_TRANSP;
            ping.border_color = col;
            ping.border_width = 1;
            ping.border_opa = (lv_opa_t)(220 - (220UL * age) / RADAR_PING_MS);
            ping.radius = LV_RADIUS_CIRCLE;
            int r = KI(4) + (int)((KI(16) * age) / RADAR_PING_MS);
            lv_area_t pa = {(lv_coord_t)(px - r), (lv_coord_t)(py - r), (lv_coord_t)(px + r), (lv_coord_t)(py + r)};
            lv_draw_rect(ctx, &ping, &pa);
        }

        // Marker: bigger when nearer; hostiles get a halo
        lv_draw_rect_dsc_t mk;
        lv_draw_rect_dsc_init(&mk);
        mk.bg_color = col;
        mk.bg_opa = op;
        mk.radius = LV_RADIUS_CIRCLE;
        int r = (int)((c.hostile ? 4.0f : 3.0f) * K * (0.8f + 0.4f * (ck[i] - 0.8f)) + 0.5f);
        if (r < 2) r = 2;
        if (c.hostile) {
            mk.border_color = col;
            mk.border_width = 1;
            mk.border_opa = op / 2;
            r += 1;
        }
        lv_area_t ba = {(lv_coord_t)(px - r), (lv_coord_t)(py - r), (lv_coord_t)(px + r), (lv_coord_t)(py + r)};
        lv_draw_rect(ctx, &mk, &ba);
    }

    // Scanlines on top of everything
    ln.color = COLOR_HOLO;
    ln.width = 1;
    ln.opa = LV_OPA_10;
    for (int y = scan_off; y < VH; y += KI(5)) {
        lv_point_t p1 = {ox, (lv_coord_t)(oy + y)};
        lv_point_t p2 = {(lv_coord_t)(ox + VW), (lv_coord_t)(oy + y)};
        if (ui_line_clipped(ctx, p1.x, p1.y, p2.x, p2.y, 1)) continue;
        lv_draw_line(ctx, &ln, &p1, &p2);
    }
}

// -------------------------------------------------------------------------
// Wide layout, per-frame geometry: the globe's curves only depend on the
// frame state, so they are projected once per frame (here) into a segment
// list. Each render band then just rasterizes the segments, instead of
// redoing ~600 sin/cos calls per band.
// -------------------------------------------------------------------------
struct WSeg { int16_t x0, y0, x1, y1; uint8_t opa, hilite, w; };
static WSeg wsegs[640];
static int nwsegs = 0;

static void collect_curve(CurveFn fn, float param, float t0, float t1, int segs, float base_opa, float depth_gain,
                          bool hilite, int width) {
    int px_ = 0, py_ = 0;
    float pd = 0;
    for (int i = 0; i <= segs; i++) {
        float x, y, z, d;
        int sx, sy;
        fn(t0 + (t1 - t0) * i / segs, param, &x, &y, &z);
        proj(x, y, z, &sx, &sy, &d, NULL);
        if (i > 0 && nwsegs < (int)(sizeof(wsegs) / sizeof(wsegs[0]))) {
            float dm = (d + pd) * 0.5f;
            WSeg& g = wsegs[nwsegs++];
            g.x0 = px_; g.y0 = py_; g.x1 = sx; g.y1 = sy;
            g.opa = fl(base_opa * (1.0f - depth_gain + depth_gain * (dm + 1.0f) * 0.5f));
            g.hilite = hilite;
            g.w = width;
        }
        px_ = sx; py_ = sy; pd = d;
    }
}

static void radar_build_wide(void) {
    const float PI_ = 3.14159265f;
    m_cy = cosf(yaw * DEG); m_sy = sinf(yaw * DEG);
    m_cp = cosf(pitch * DEG); m_sp = sinf(pitch * DEG);
    nwsegs = 0;

    // Globe wireframe: equator and two parallels, four great circles through the poles
    collect_curve(curve_lat, 0.0f, 0, 2 * PI_, SEG, 150, 0.7f, false, 2);
    collect_curve(curve_lat, 40.0f * DEG, 0, 2 * PI_, SEG, 90, 0.7f, false, 1);
    collect_curve(curve_lat, -40.0f * DEG, 0, 2 * PI_, SEG, 90, 0.7f, false, 1);
    for (int i = 0; i < 4; i++) {
        float az = i * 45.0f * DEG;
        collect_curve(curve_mer, az, -PI_ / 2, PI_ / 2, SEG / 2, 90, 0.7f, false, 1);
        collect_curve(curve_mer, az + PI_, -PI_ / 2, PI_ / 2, SEG / 2, 90, 0.7f, false, 1);
    }

    // Sweep: a bright meridian turning round the globe, with a fading trail
    for (int k = RADAR_TRAIL; k >= 0; k--) {
        float az = (sweep_deg - k * RADAR_TRAIL_DEG) * DEG;
        if (k == 0)   // soft glow under the beam's bright line
            collect_curve(curve_mer, az, -PI_ / 2, PI_ / 2, SEG / 2, 50, 0.4f, true, 5);
        float op = k == 0 ? 255 : 150.0f * (RADAR_TRAIL - k + 1) / (RADAR_TRAIL + 1);
        collect_curve(curve_mer, az, -PI_ / 2, PI_ / 2, SEG / 2, op, 0.4f, k == 0, k == 0 ? 3 : 1);
    }
}

// -------------------------------------------------------------------------
// Wide layout: same scene, rasterized by hand into LVGL's render band (see
// wirecanvas.h) because LVGL's diagonal translucent lines are too slow here
// -------------------------------------------------------------------------
static void radar_draw_cb_wide(lv_event_t* e) {
    lv_obj_t* obj = lv_event_get_target(e);
    lv_draw_ctx_t* ctx = lv_event_get_draw_ctx(e);
    lv_area_t a;
    lv_obj_get_coords(obj, &a);
    const int ox = a.x1, oy = a.y1;
    const WireTarget t = wt_begin(ctx);
    uint32_t now = lv_tick_get();
    const float PI_ = 3.14159265f;

    wt_fill_rect(t, a.x1, a.y1, a.x2, a.y2, COLOR_CHAMBER, 255);

    // Limb: the globe's silhouette. Without it the tilted parallels read as
    // flat ellipses and the sphere looks squashed. With the perspective camera
    // the visible outline has radius S * D / sqrt(D^2 - 1).
    {
        int lr = (int)(RADAR_S * RADAR_D / sqrtf(RADAR_D * RADAR_D - 1.0f) + 0.5f);
        wt_ellipse(t, ox + RCX, oy + RCY, lr, lr, COLOR_HOLO, fl(150), false);
        wt_ellipse(t, ox + RCX, oy + RCY, lr + 1, lr + 1, COLOR_HOLO, fl(90), false);
    }

    // Globe and sweep: segments projected once per frame (radar_build_wide)
    for (int i = 0; i < nwsegs; i++) {
        const WSeg& g = wsegs[i];
        wt_line(t, ox + g.x0, oy + g.y0, ox + g.x1, oy + g.y1, g.hilite ? COLOR_HILITE : COLOR_HOLO, g.opa, g.w);
    }

    // North marker (amber) at the pole, and our own position at the centre
    int nx, ny, cxp, cyp;
    float nd, cd, kk;
    proj(0, 0, -1.0f, &nx, &ny, &nd, &kk);
    wt_disc(t, ox + nx, oy + ny, KI(3), COLOR_ACCENT, 255);
    proj(0, 0, 0, &cxp, &cyp, &cd, NULL);
    wt_disc(t, ox + cxp, oy + cyp, KI(3), COLOR_HILITE, LV_OPA_80);

    // Contacts, farthest first so nearer ones overdraw
    int order[N_CONTACTS];
    float cdepth[N_CONTACTS];
    int csx[N_CONTACTS], csy[N_CONTACTS];
    float ck[N_CONTACTS];
    for (int i = 0; i < N_CONTACTS; i++) {
        order[i] = i;
        proj(contacts[i].x, contacts[i].z, contacts[i].y, &csx[i], &csy[i], &cdepth[i], &ck[i]);
    }
    for (int i = 1; i < N_CONTACTS; i++) {
        int k = order[i], j = i - 1;
        while (j >= 0 && cdepth[order[j]] > cdepth[k]) { order[j + 1] = order[j]; j--; }
        order[j + 1] = k;
    }
    for (int n = 0; n < N_CONTACTS; n++) {
        int i = order[n];
        const RadarContact& c = contacts[i];
        uint8_t inten = contact_intensity(c, now);
        if (inten == 0) continue;
        lv_color_t col = c.hostile ? COLOR_ALERT : COLOR_FRIEND;
        float near_k = 0.6f + 0.4f * (cdepth[i] + 1.0f) * 0.5f;      // depth cue
        uint8_t op = (uint8_t)(inten * near_k);
        int px = ox + csx[i], py = oy + csy[i];

        wt_line(t, px, py, ox + cxp, oy + cyp, col, op / 3, 1);       // line of sight to the centre

        uint32_t age = now - c.hit_ms;
        if (age < RADAR_PING_MS) {                                     // detection ring expanding around the contact
            int r = KI(4) + (int)((KI(16) * age) / RADAR_PING_MS);
            wt_ellipse(t, px, py, r, r, col, (uint8_t)(220 - (220UL * age) / RADAR_PING_MS), false);
        }

        // Marker: bigger when nearer; hostiles get a halo
        int r = (int)((c.hostile ? 4.0f : 3.0f) * K * (0.8f + 0.4f * (ck[i] - 0.8f)) + 0.5f);
        if (r < 2) r = 2;
        if (c.hostile) {
            r += 1;
            wt_ellipse(t, px, py, r + 1, r + 1, col, op / 2, false);
        }
        wt_disc(t, px, py, r, col, op);
    }

    // Scanlines on top of everything
    for (int y = scan_off; y < VH; y += KI(5)) wt_hline(t, oy + y, ox, ox + VW - 1, COLOR_HOLO, LV_OPA_10);
}

// Drag to turn the globe (same feel as the hologram projector)
static void radar_touch_cb(lv_event_t* e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSING) {
        lv_point_t v;
        lv_indev_get_vect(lv_indev_get_act(), &v);
        yaw += v.x * 0.9f;
        pitch += v.y * 0.9f;
        if (pitch > 75) pitch = 75;
        if (pitch < -75) pitch = -75;
        pressing = true;
        last_touch = lv_tick_get();
    } else if (code == LV_EVENT_RELEASED || code == LV_EVENT_PRESS_LOST) {
        pressing = false;
        last_touch = lv_tick_get();
    }
}


static void event_red_alert_handler(lv_event_t* e) {
    hw_led_pulse(true, false, false, sfx_red_alert(), 100); // Strobing red for the siren
}

// =========================================================================
// 2. Tactical Radar Screen
// =========================================================================
lv_obj_t* screen_radar_create(void) {
    const bool wide = UI_WIDE;
    VW = wide ? UI_W * 27 / 40 : 240;                  // wide: left two thirds
    VH = wide ? UI_H - TOP_BAR_H : 166;
    RCX = VW / 2;
    RCY = wide ? VH / 2 : 84;
    RADAR_S = wide ? VH * 0.38f : 66.0f;
    K = RADAR_S / 66.0f;

    radar_card_text[0] = '\0';      // force the info card to be refilled (also after a language rebuild)
    radar_card_alert = false;
    scr_radar = lv_obj_create(NULL);
    lv_obj_add_style(scr_radar, &style_base, 0);
    lv_obj_clear_flag(scr_radar, LV_OBJ_FLAG_SCROLLABLE);
    ui_attach_top_bar(scr_radar, tr(S_RADAR_TITLE), true);

    // Holographic 3D scope (custom drawn, see radar_draw_cb)
    radar_obj = lv_obj_create(scr_radar);
    lv_obj_remove_style_all(radar_obj);
    lv_obj_set_size(radar_obj, VW, VH);
    lv_obj_set_pos(radar_obj, 0, TOP_BAR_H);
    lv_obj_clear_flag(radar_obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(radar_obj, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(radar_obj, wide ? radar_draw_cb_wide : radar_draw_cb, LV_EVENT_DRAW_MAIN_END, NULL);
    lv_obj_add_event_cb(radar_obj, radar_touch_cb, LV_EVENT_ALL, NULL);

    for (int i = 0; i < N_CONTACTS; i++) {
        contacts[i].hostile = i >= 2;
        contact_spawn(contacts[i], false);
    }
    sweep_deg = 0;
    radar_last_tick = lv_tick_get();
    if (wide) radar_build_wide();

    // Target info card (updated from the simulation): under the scope on the
    // portrait layout, in the right-hand column when wide
    const int col_x = VW + 16, col_w = UI_W - VW - 32;
    card_radar = lv_obj_create(scr_radar);
    if (wide) {
        lv_obj_set_size(card_radar, col_w, 170);
        lv_obj_set_pos(card_radar, col_x, TOP_BAR_H + 16);
    } else {
        lv_obj_set_size(card_radar, 216, 48);
        lv_obj_align(card_radar, LV_ALIGN_TOP_MID, 0, TOP_BAR_H + VH + 4);
    }
    lv_obj_add_style(card_radar, &style_card, 0);
    lv_obj_clear_flag(card_radar, LV_OBJ_FLAG_SCROLLABLE);

    lbl_radar_info = lv_label_create(card_radar);
    lv_label_set_text(lbl_radar_info, "");
    lv_obj_set_width(lbl_radar_info, wide ? col_w - 24 : LV_SIZE_CONTENT);
    lv_obj_center(lbl_radar_info);
    lv_obj_set_style_text_align(lbl_radar_info, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(lbl_radar_info, COLOR_TEXT, 0);
    lv_obj_set_style_text_font(lbl_radar_info, ui_font(12), 0);
    radar_update_card(lv_tick_get());

    // Red Alert Alarm Button
    lv_obj_t* btn_alarm = lv_btn_create(scr_radar);
    if (wide) {
        lv_obj_set_size(btn_alarm, col_w, 90);
        lv_obj_align(btn_alarm, LV_ALIGN_BOTTOM_RIGHT, -16, -24);
    } else {
        lv_obj_set_size(btn_alarm, 216, 44);
        lv_obj_align(btn_alarm, LV_ALIGN_BOTTOM_MID, 0, -8);
    }
    ui_style_btn(btn_alarm, true);
    lv_obj_add_event_cb(btn_alarm, event_red_alert_handler, LV_EVENT_CLICKED, NULL);

    lv_obj_t* lbl_alarm = lv_label_create(btn_alarm);
    lv_label_set_text(lbl_alarm, tr(S_RADAR_ALARM));
    lv_obj_center(lbl_alarm);
    if (wide) lv_obj_set_style_text_font(lbl_alarm, ui_font(16), 0);
    if (!radar_timer) radar_timer = lv_timer_create(radar_timer_cb, 50, NULL);   // survives screen rebuilds
    return scr_radar;
}
