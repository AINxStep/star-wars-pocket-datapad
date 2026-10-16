// =========================================================================
// Web simulator glue: loads datapad.wasm (the firmware's real UI code built
// with Emscripten) and provides what the board layer asks of the browser:
// the screen (<canvas>), the pointer, the speaker (Web Audio), the status LED,
// the brightness, and the P2P link (BroadcastChannel between two tabs).
// =========================================================================
(function () {
  'use strict';
  const $ = (id) => document.getElementById(id);
  // The project targets a single board: the LCDWIKI ES3C28P (240x320 portrait).
  const W = 240, H = 320;

  $('zoom').value = localStorage.getItem('sim/zoom') || '2';

  // ---- serial log ------------------------------------------------------
  const logEl = $('log');
  window.simLog = function (line) {
    logEl.textContent += line + '\n';
    const lines = logEl.textContent.split('\n');
    if (lines.length > 400) logEl.textContent = lines.slice(-300).join('\n');
    logEl.scrollTop = logEl.scrollHeight;
  };

  // ---- screen ----------------------------------------------------------
  const canvas = $('screen');
  canvas.width = W; canvas.height = H;
  const ctx2d = canvas.getContext('2d');
  function applyZoom() {
    const z = parseFloat($('zoom').value);
    canvas.style.width = Math.round(W * z) + 'px';
    canvas.style.height = Math.round(H * z) + 'px';
    localStorage.setItem('sim/zoom', $('zoom').value);
  }
  applyZoom();
  $('zoom').addEventListener('change', applyZoom);

  // Brightness (and the screen-off sleep, which is brightness 0)
  window.simBrightness = function (pct) {
    canvas.style.filter = 'brightness(' + (pct / 100).toFixed(2) + ')';
  };
  window.simLed = function (r, g, b) {
    const led = $('led');
    if (r || g || b) {
      const c = 'rgb(' + (r ? 255 : 0) + ',' + (g ? 255 : 0) + ',' + (b ? 255 : 0) + ')';
      led.style.background = c;
      led.style.boxShadow = '0 0 10px 3px ' + c;
    } else {
      led.style.background = '#222';
      led.style.boxShadow = 'inset 0 0 2px #000';
    }
  };

  // ---- pointer = touch controller -------------------------------------
  let started = false;
  function toDevice(e) {
    const r = canvas.getBoundingClientRect();
    return [Math.round((e.clientX - r.left) * W / r.width), Math.round((e.clientY - r.top) * H / r.height)];
  }
  // The firmware samples the touch controller once per loop iteration, so a
  // press must last a few frames to be seen (a synthetic click can be shorter)
  const MIN_PRESS_MS = 40;
  let pressedAt = 0, releaseTimer = null;
  canvas.addEventListener('pointerdown', (e) => {
    canvas.setPointerCapture(e.pointerId);
    if (simAudio.ctx && simAudio.ctx.state === 'suspended') simAudio.ctx.resume();
    if (!started) return;
    clearTimeout(releaseTimer);
    pressedAt = performance.now();
    const [x, y] = toDevice(e);
    Module._sim_pointer(x, y, 1);
    e.preventDefault();
  });
  canvas.addEventListener('pointermove', (e) => {
    if (!started || !(e.buttons & 1 || e.pointerType === 'touch')) return;
    const [x, y] = toDevice(e);
    Module._sim_pointer(x, y, 1);
  });
  const up = (e) => {
    if (!started) return;
    const [x, y] = toDevice(e);
    const wait = Math.max(0, MIN_PRESS_MS - (performance.now() - pressedAt));
    releaseTimer = setTimeout(() => Module._sim_pointer(x, y, 0), wait);
  };
  canvas.addEventListener('pointerup', up);
  canvas.addEventListener('pointercancel', up);
  canvas.addEventListener('contextmenu', (e) => e.preventDefault());

  // ---- sound: Web Audio, same clips as the firmware ---------------------
  const SOUNDS = ['click', 'r2d2', 'blaster', 'comlink', 'alarm', 'holo', 'coms1', 'coms2', 'coms3'];
  const simAudio = window.simAudio = {
    ctx: null, gain: null, buffers: [], vol: 0.36, muted: false, endAt: 0,
    init() {
      const AC = window.AudioContext || window.webkitAudioContext;
      if (!AC || this.ctx) return;
      this.ctx = new AC();
      this.gain = this.ctx.createGain();
      this.gain.gain.value = this.muted ? 0 : this.vol;
      this.gain.connect(this.ctx.destination);
      SOUNDS.forEach((n, i) => {
        fetch('sounds/' + n + '.wav').then((r) => r.arrayBuffer())
          .then((b) => this.ctx.decodeAudioData(b)).then((buf) => { this.buffers[i] = buf; })
          .catch((err) => window.simLog('[AUDIO] ' + n + '.wav: ' + err));
      });
    },
    play(id) {
      const buf = this.buffers[id];
      if (!this.ctx || !buf) return 0;
      if (this.ctx.state === 'suspended') this.ctx.resume();
      const src = this.ctx.createBufferSource();
      src.buffer = buf;
      src.connect(this.gain);
      src.start();
      const ms = Math.round(buf.duration * 1000);
      this.endAt = performance.now() + ms;
      return ms;
    },
    setVolume(pct) {                       // the firmware's curve: volume squared
      this.vol = Math.pow(pct / 100, 2);
      if (this.gain) this.gain.gain.value = this.muted ? 0 : this.vol;
    },
    busy() { return performance.now() < this.endAt; },
  };
  $('sound').addEventListener('click', () => {
    simAudio.muted = !simAudio.muted;
    $('sound').classList.toggle('on', !simAudio.muted);
    $('sound').textContent = simAudio.muted ? '🔇 Silencio' : '🔊 Sonido';
    if (simAudio.gain) simAudio.gain.gain.value = simAudio.muted ? 0 : simAudio.vol;
  });

  // ---- P2P link: two tabs through a BroadcastChannel --------------------
  const simLink = window.simLink = (function () {
    let bc = null, lastBeacon = 0, peer = '', pending = 0, rx = 0;
    const id = Math.floor(Math.random() * 0xFFFFFF);
    const hex = (n) => n.toString(16).padStart(2, '0').toUpperCase();
    const mac = 'DA:7A:' + hex(id >> 16 & 255) + ':' + hex(id >> 8 & 255) + ':' + hex(id & 255) + ':' + hex((id * 7) & 255);
    return {
      open() {
        if (bc || !('BroadcastChannel' in window)) return;
        bc = new BroadcastChannel('datapad-link');
        bc.onmessage = (e) => {
          const m = e.data;
          if (!m || m.id === id) return;
          peer = m.mac;
          if (m.k === 'b') { lastBeacon = performance.now(); rx++; }
          else if (m.k === 'm') pending = m.arg;
        };
      },
      send(type, arg) {
        if (!bc) return;
        bc.postMessage(type === 0 ? { k: 'b', id, mac } : { k: 'm', id, mac, arg });
      },
      age() { return lastBeacon ? Math.round(performance.now() - lastBeacon) : -1; },
      pop() { const p = pending; pending = 0; return p; },
      rx() { return rx; },
      peerMac() { return peer; },
    };
  })();
  $('second').addEventListener('click', () => window.open(location.href, '_blank'));

  // ---- controls ---------------------------------------------------------
  $('logbtn').addEventListener('click', () => {
    $('logbox').classList.toggle('hidden');
    $('logbtn').classList.toggle('on');
  });
  $('reset').addEventListener('click', () => {
    if (!confirm('¿Borrar los ajustes guardados del simulador?')) return;
    Object.keys(localStorage).filter((k) => k.startsWith('datapad/')).forEach((k) => localStorage.removeItem(k));
    location.reload();
  });

  // ---- load the firmware ------------------------------------------------
  let lastFrame = -1;
  function paint() {
    const f = Module._sim_frames();
    if (f !== lastFrame) {
      lastFrame = f;
      const ptr = Module._sim_fb();
      const px = new Uint8ClampedArray(Module.HEAPU8.buffer, ptr, W * H * 4);
      ctx2d.putImageData(new ImageData(px, W, H), 0, 0);
    }
    requestAnimationFrame(paint);
  }

  window.Module = {
    print: (t) => window.simLog(t),
    printErr: (t) => console.warn(t),
    onRuntimeInitialized() {
      Module._sim_configure(W, H);
      started = true;
      Module._sim_start();
      setInterval(Module._sim_tick, 6);          // the firmware's loop(), decoupled from painting
      $('status').textContent = 'ES3C28P 240×320';
      requestAnimationFrame(paint);
    },
  };
  const s = document.createElement('script');
  s.src = 'datapad.js';
  s.onerror = () => { $('status').textContent = 'falta datapad.js: ejecuta ./build.sh'; };
  document.body.appendChild(s);
})();
