#include "starwars_audio.h"
#include <emscripten.h>

// =========================================================================
// Web simulator sound engine: the page loads sounds/<name>.wav with the Web
// Audio API (src/../web/sim.js). Volume follows the firmware's curve (v^2).
// =========================================================================
EM_JS(void, js_audio_init, (), { if (window.simAudio) window.simAudio.init(); });
EM_JS(int, js_audio_play, (int id), { return window.simAudio ? window.simAudio.play(id) : 0; });
EM_JS(void, js_audio_volume, (int pct), { if (window.simAudio) window.simAudio.setVolume(pct); });
EM_JS(int, js_audio_busy, (), { return window.simAudio && window.simAudio.busy() ? 1 : 0; });

void audio_init(SPIClass&) { js_audio_init(); }
void audio_start_output() {}
void audio_set_volume(uint8_t pct) { js_audio_volume(pct > 100 ? 100 : pct); }
bool audio_busy() { return js_audio_busy(); }
uint32_t audio_play(SoundId id) { return (uint32_t)js_audio_play((int)id); }
void audio_print_load_report() { Serial.println("[AUDIO] sim: clips served from /sounds"); }
