#include "link.h"
#include <lvgl.h>
#include <emscripten.h>
#include <string.h>

// =========================================================================
// Web simulator link: two simulator tabs of the same browser talk through a
// BroadcastChannel, standing in for ESP-NOW. Same behavior as src/link.cpp:
// a 1 Hz beacon while the link is on, a peer is "seen" if a beacon arrived in
// the last 3 s, and predefined messages are only delivered while active.
// =========================================================================
EM_JS(void, js_link_open, (), { if (window.simLink) window.simLink.open(); });
EM_JS(void, js_link_send, (int type, int arg), { if (window.simLink) window.simLink.send(type, arg); });
EM_JS(int, js_link_age, (), { return window.simLink ? window.simLink.age() : -1; });
EM_JS(int, js_link_pop, (), { return window.simLink ? window.simLink.pop() : 0; });
EM_JS(int, js_link_rx, (), { return window.simLink ? window.simLink.rx() : 0; });
EM_JS(void, js_link_peer, (char* out, int n), {
    const s = window.simLink ? window.simLink.peerMac() : "";
    const b = new TextEncoder().encode(s.slice(0, n - 1));
    HEAPU8.set(b, out);
    HEAPU8[out + b.length] = 0;
});

static bool active = false;
static lv_timer_t* beacon_timer = NULL;
static lv_timer_t* poll_timer = NULL;
static LinkMsgHandler msg_handler = NULL;

static void beacon_cb(lv_timer_t*) { js_link_send(0, 0); }

static void poll_cb(lv_timer_t*) {
    int id = js_link_pop();
    if (!id || !active || !msg_handler) return;
    char mac[18];
    js_link_peer(mac, sizeof mac);
    msg_handler((uint8_t)id, mac);
}

void link_wifi_init() { js_link_open(); }

void link_start() {
    if (active) return;
    if (!beacon_timer) beacon_timer = lv_timer_create(beacon_cb, 1000, NULL);
    lv_timer_resume(beacon_timer);
    active = true;
}

void link_stop() {
    if (!active) return;
    if (beacon_timer) lv_timer_pause(beacon_timer);
    active = false;
}

bool link_active() { return active; }

bool link_peer_seen() {
    int age = js_link_age();
    return active && age >= 0 && age < 3000;
}

void link_peer_mac(char out[18]) { js_link_peer(out, 18); }
uint32_t link_rx_count() { return (uint32_t)js_link_rx(); }

void link_send_msg(uint8_t id) {
    if (!active || id < 1 || id > 4) return;
    js_link_send(1, id);
}

void link_set_msg_handler(LinkMsgHandler handler) {
    msg_handler = handler;
    if (!poll_timer) poll_timer = lv_timer_create(poll_cb, 100, NULL);
}
