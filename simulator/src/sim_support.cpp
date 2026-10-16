#include <Arduino.h>
#include <Preferences.h>
#include <emscripten.h>

SerialClass Serial;
EspClass ESP;

uint32_t millis(void) { return (uint32_t)emscripten_get_now(); }
uint32_t micros(void) { return (uint32_t)(emscripten_get_now() * 1000.0); }

// Serial output goes to the page's log panel and the browser console
EM_JS(void, js_log, (const char* s), {
    const t = UTF8ToString(s);
    if (window.simLog) window.simLog(t); else console.log(t);
});

void SerialClass::println(const char* s) { char b[512]; snprintf(b, sizeof b, "%s", s); js_log(b); }
void SerialClass::print(const char* s)   { js_log(s); }
int SerialClass::printf(const char* fmt, ...) {
    char b[512];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    // Trim the trailing newline: the log panel adds its own line break
    for (int i = (int)strlen(b) - 1; i >= 0 && (b[i] == '\n' || b[i] == '\r'); i--) b[i] = 0;
    js_log(b);
    return n;
}

// Settings "reset" reboots the board on device; the closest sim equivalent is
// reloading the page (localStorage prefs survive, same as NVS on real flash).
void EspClass::restart() { emscripten_run_script("location.reload()"); }

// Preferences -> localStorage ("datapad/<namespace>/<key>")
EM_JS(int, sim_pref_get, (const char* ns, const char* key, int def), {
    try {
        const v = window.localStorage.getItem('datapad/' + UTF8ToString(ns) + '/' + UTF8ToString(key));
        return v === null ? def : parseInt(v, 10);
    } catch (e) { return def; }
});
EM_JS(void, sim_pref_put, (const char* ns, const char* key, int value), {
    try { window.localStorage.setItem('datapad/' + UTF8ToString(ns) + '/' + UTF8ToString(key), String(value)); } catch (e) {}
});
