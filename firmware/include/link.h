#pragma once
#include <Arduino.h>

// =========================================================================
// ESP-NOW peer link ("comlink" between two datapads)
// A 1 Hz beacon on the broadcast address. When a packet with the datapad
// magic arrives, a peer is reported on the Comlink screen. On top of that the
// link carries predefined messages: a sound id that the receiving datapad
// plays and shows as an alert.
//
// Packet: { 0xDA, 0x7A, 'D', 'P', type, arg, seq }
//   type 0 = beacon (arg unused), type 1 = message (arg = sound id 1..4).
//   A bare 5-byte packet is a legacy beacon.
// The radio initializes at boot (Wi-Fi needs a clean heap) and stays on;
// the Comlink toggle only runs or pauses the beacon.
// =========================================================================

void link_wifi_init();                  // call early in setup(), before audio_init
void link_start();
void link_stop();
bool link_active();
bool link_peer_seen();                  // a beacon arrived in the last 3 s
void link_peer_mac(char out[18]);       // last peer "AA:BB:CC:DD:EE:FF", "" if none
uint32_t link_rx_count();               // beacons received so far

// Predefined messages. `id` is the sound-board effect (1 R2-D2, 2 blaster,
// 3 comlink channel, 4 red alert). Sent twice (same sequence number) because
// broadcast frames are not acknowledged; receivers drop the duplicate.
void link_send_msg(uint8_t id);

// Called from the LVGL thread when a message from the peer arrives (only
// while the link is active). `mac` is "AA:BB:CC:DD:EE:FF".
typedef void (*LinkMsgHandler)(uint8_t id, const char* mac);
void link_set_msg_handler(LinkMsgHandler handler);
