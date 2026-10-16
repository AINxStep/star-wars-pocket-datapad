#include "link.h"
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <lvgl.h>
#include <string.h>
#include <stdio.h>

// =========================================================================
// ESP-NOW beacon link
// ESP-NOW callbacks run on the Wi-Fi task (core 0); the audio task lives on
// core 1 at priority 5, so playback keeps priority over the radio.
// =========================================================================
static const uint8_t MAGIC[4] = { 0xDA, 0x7A, 'D', 'P' };   // ~"DATAPAD"
static const uint8_t BCAST[6] = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF };

enum { PKT_BEACON = 0, PKT_MSG = 1 };
static const int PKT_LEN = 7;

// A message handed from the radio task (core 0) to the LVGL thread
static volatile uint8_t  rx_msg_id = 0;         // 0 = nothing pending
static volatile uint8_t  rx_msg_seq = 0;
static uint8_t  rx_msg_mac[6] = { 0 };
static uint8_t  last_msg_seq = 0xFF;
static bool     last_msg_valid = false;
static uint8_t  msg_seq = 0;
static LinkMsgHandler msg_handler = NULL;
static lv_timer_t* msg_timer = NULL;

static bool active = false;
static bool radio_ready = false;
static uint8_t seq = 0;
static uint32_t last_rx_ms = 0;
static uint32_t rx_n = 0;
static uint8_t last_mac[6] = { 0 };
static bool have_mac = false;
static lv_timer_t* beacon_timer = NULL;

// arduino-esp32 >= 3.0 (IDF 5) changed the receive callback signature
#if defined(ESP_ARDUINO_VERSION_MAJOR) && (ESP_ARDUINO_VERSION_MAJOR >= 3)
static void on_recv(const esp_now_recv_info_t* info, const uint8_t* data, int len) {
    const uint8_t* mac = info->src_addr;
#else
static void on_recv(const uint8_t* mac, const uint8_t* data, int len) {
#endif
    if (len < 5 || memcmp(data, MAGIC, 4)) return;

    // Messages (type byte present only in 7-byte packets)
    if (len >= PKT_LEN && data[4] == PKT_MSG) {
        if (!active) return;
        uint8_t id = data[5], seq = data[6];
        if (id < 1 || id > 4) return;
        if (last_msg_valid && seq == last_msg_seq && !memcmp(mac, rx_msg_mac, 6)) return;   // duplicate
        memcpy(rx_msg_mac, mac, 6);
        last_msg_seq = seq;
        last_msg_valid = true;
        rx_msg_seq = seq;
        rx_msg_id = id;               // published last: the LVGL timer polls this
        return;
    }

    if (!have_mac || memcmp(mac, last_mac, 6)) {
        Serial.printf("[LINK] peer %02X:%02X:%02X:%02X:%02X:%02X\n",
                      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
    }
    last_rx_ms = millis();
    rx_n++;
    memcpy(last_mac, mac, 6);
    have_mac = true;
}

static void beacon_cb(lv_timer_t* t) {
    uint8_t pkt[PKT_LEN] = { MAGIC[0], MAGIC[1], MAGIC[2], MAGIC[3], PKT_BEACON, 0, seq++ };
    esp_now_send(BCAST, pkt, sizeof(pkt));
}

// Wi-Fi + ESP-NOW must be brought up at boot, while the heap still has big
// contiguous blocks — starting the radio later (after LVGL + audio clips)
// fails inside phy_init with "failed to allocate memory for RF calibration
// data" and aborts. The radio stays initialized; the LINK toggle only runs
// or pauses the beacon timer.
void link_wifi_init() {
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(false, true);   // erase creds but keep the STA interface up
    if (esp_now_init() != ESP_OK) {
        Serial.println("[LINK] esp_now_init failed");
        return;
    }
    esp_now_register_recv_cb(on_recv);

    esp_now_peer_info_t peer;
    memset(&peer, 0, sizeof(peer));
    memcpy(peer.peer_addr, BCAST, 6);
    peer.channel = 0;          // current Wi-Fi channel (1 while unassociated)
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("[LINK] add broadcast peer failed");
        esp_now_deinit();
        return;
    }
    radio_ready = true;
    Serial.printf("[LINK] ESP-NOW ready; heap %u free\n", (unsigned)ESP.getFreeHeap());
}

void link_start() {
    if (active || !radio_ready) return;
    if (!beacon_timer) {
        beacon_timer = lv_timer_create(beacon_cb, 1000, NULL);
        lv_timer_pause(beacon_timer);
    }
    lv_timer_resume(beacon_timer);
    active = true;
    Serial.printf("[LINK] ESP-NOW beacon on; heap %u free\n", (unsigned)ESP.getFreeHeap());
}

void link_stop() {
    if (!active) return;
    lv_timer_pause(beacon_timer);
    active = false;
    have_mac = false;
    rx_n = 0;
    last_rx_ms = 0;
    Serial.printf("[LINK] ESP-NOW off; heap %u free\n", (unsigned)ESP.getFreeHeap());
}

bool link_active() { return active; }

bool link_peer_seen() {
    return active && have_mac && (millis() - last_rx_ms < 3000);
}

void link_peer_mac(char out[18]) {
    if (!have_mac) { out[0] = '\0'; return; }
    snprintf(out, 18, "%02X:%02X:%02X:%02X:%02X:%02X",
             last_mac[0], last_mac[1], last_mac[2], last_mac[3], last_mac[4], last_mac[5]);
}

uint32_t link_rx_count() { return rx_n; }

// -------------------------------------------------------------------------
// Predefined messages
// -------------------------------------------------------------------------
static uint8_t resend_id = 0, resend_seq = 0;

static void send_pkt(uint8_t id, uint8_t seq_) {
    uint8_t pkt[PKT_LEN] = { MAGIC[0], MAGIC[1], MAGIC[2], MAGIC[3], PKT_MSG, id, seq_ };
    esp_now_send(BCAST, pkt, sizeof(pkt));
}

static void resend_cb(lv_timer_t* t) { send_pkt(resend_id, resend_seq); }

void link_send_msg(uint8_t id) {
    if (!active || !radio_ready || id < 1 || id > 4) return;
    uint8_t seq_ = msg_seq++;
    send_pkt(id, seq_);
    resend_id = id;
    resend_seq = seq_;
    lv_timer_t* t = lv_timer_create(resend_cb, 40, NULL);   // second copy, then self-deletes
    lv_timer_set_repeat_count(t, 1);
    Serial.printf("[LINK] msg %u sent (seq %u)\n", id, seq_);
}

static void msg_poll_cb(lv_timer_t* t) {
    uint8_t id = rx_msg_id;
    if (!id) return;
    rx_msg_id = 0;
    if (!active || !msg_handler) return;
    char mac[18];
    snprintf(mac, sizeof(mac), "%02X:%02X:%02X:%02X:%02X:%02X",
             rx_msg_mac[0], rx_msg_mac[1], rx_msg_mac[2], rx_msg_mac[3], rx_msg_mac[4], rx_msg_mac[5]);
    Serial.printf("[LINK] msg %u received from %s\n", id, mac);
    msg_handler(id, mac);
}

void link_set_msg_handler(LinkMsgHandler handler) {
    msg_handler = handler;
    if (!msg_timer) msg_timer = lv_timer_create(msg_poll_cb, 100, NULL);
}
