#!/usr/bin/env python3
"""Generates the datapad sound effects as WAV files (original synthesized
sounds, no dependencies). Copy the resulting sounds/ folder to the root of the
microSD card: /sounds/click.wav, r2d2.wav, blaster.wav, comlink.wav, alarm.wav,
holo.wav. It also writes coms1.wav, coms2.wav and coms3.wav, the ambient-chatter
slots, as one second of silence: the project ships no third-party audio, so
those slots stay silent until you put your own clips there (do not commit or
redistribute audio you do not have the rights to).

Any of them can be replaced with your own PCM WAV (8/16-bit, mono/stereo,
4-48 kHz, up to ~6 s); the firmware converts it on boot.

Usage: python3 tools/gen_sounds.py
"""
import math, os, random, wave

RATE = 16000
OUT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "sounds")
TAU = 2 * math.pi


def write(name, samples, rate=RATE):
    peak = max(1e-9, max(abs(s) for s in samples))
    data = bytes(max(0, min(255, int(128 + 120 * s / peak))) for s in samples)
    os.makedirs(OUT, exist_ok=True)
    path = os.path.join(OUT, name + ".wav")
    with wave.open(path, "wb") as w:
        w.setnchannels(1); w.setsampwidth(1); w.setframerate(rate)
        w.writeframes(data)
    print(f"{name}.wav  {len(samples) / rate:.2f} s  {len(data)} bytes")


def n(sec): return int(sec * RATE)


def tone(freqs, harm=((1, 1.0),), vib=(0, 0)):
    """freqs: per-sample frequency list. harm: (multiple, amplitude) pairs."""
    out, phase = [], 0.0
    for i, f in enumerate(freqs):
        f = f * (1 + vib[1] * math.sin(TAU * vib[0] * i / RATE))
        phase += TAU * f / RATE
        out.append(sum(a * math.sin(m * phase) for m, a in harm))
    return out


def env_ad(length, attack=0.004, release=0.02):
    a, r = n(attack), n(release)
    return [min(1.0, i / max(1, a), (length - i) / max(1, r)) for i in range(length)]


def lowpass_noise(length, k=0.35, seed=1):
    rnd, y, out = random.Random(seed), 0.0, []
    for _ in range(length):
        y += k * (rnd.uniform(-1, 1) - y)
        out.append(y)
    return out


def lerp(a, b, t): return a + (b - a) * t


# ---- click: short, soft UI tick ------------------------------------------
def click():
    L = n(0.03)
    t = tone([2400] * L, harm=((1, 1.0), (2, 0.3)))
    return [s * math.exp(-i / (L * 0.25)) for i, s in enumerate(t)]


# ---- comlink: static squelch + three-note chirp --------------------------
def comlink():
    squelch = [s * 0.35 * e for s, e in zip(lowpass_noise(n(0.07), 0.6, 7), env_ad(n(0.07), 0.005, 0.03))]
    notes = [(1850, 0.05), (2450, 0.06), (1250, 0.05)]
    out = list(squelch)
    for f, d in notes:
        L = n(d)
        seg = tone([f] * L, harm=((1, 1.0), (3, 0.15)), vib=(35, 0.012))
        out += [s * e for s, e in zip(seg, env_ad(L, 0.004, 0.012))]
    out += [0.0] * n(0.02)
    return out


# ---- blaster: noise crack + falling exponential sweep --------------------
def blaster():
    L = n(0.38)
    freqs = [2900 * (260 / 2900) ** (i / L) for i in range(L)]
    t = tone(freqs, harm=((1, 1.0), (2, 0.5), (3, 0.3)))
    crack = lowpass_noise(L, 0.8, 3)
    out = []
    for i in range(L):
        e = math.exp(-i / (L * 0.38))
        c = math.exp(-i / (RATE * 0.012))
        out.append(t[i] * e * min(1.0, i / 40) + crack[i] * 0.9 * c)
    return out


# ---- r2d2: chattering glides with FM wobble ------------------------------
def r2d2():
    rnd = random.Random(2187)
    out, f_prev = [], 1800.0
    for _ in range(11):
        f_next = rnd.choice([900, 1300, 1700, 2100, 2600, 3000, 3300])
        d = rnd.choice([0.045, 0.06, 0.08, 0.11])
        L = n(d)
        glide = rnd.random() < 0.55
        freqs = [lerp(f_prev, f_next, i / L) if glide else f_next for i in range(L)]
        seg = tone(freqs, harm=((1, 1.0), (2, 0.35), (4, 0.12)), vib=(rnd.choice([18, 28, 40]), 0.04))
        out += [s * e for s, e in zip(seg, env_ad(L, 0.006, 0.02))]
        out += [0.0] * n(rnd.choice([0.008, 0.015, 0.025]))
        f_prev = f_next
    return out


# ---- alarm: rising/falling siren with some grit --------------------------
def alarm():
    out = []
    for _ in range(2):
        L = n(0.42)
        up = [lerp(550, 1500, i / L) for i in range(L)]
        down = [lerp(1500, 550, i / L) for i in range(L)]
        out += tone(up + down, harm=((1, 1.0), (2, 0.4), (3, 0.25), (5, 0.1)))[:2 * L]
    L = len(out)
    return [s * min(1.0, i / 60, (L - i) / 400) for i, s in enumerate(out)]


# ---- holo: rising shimmer with tremolo over a low hum ---------------------
def holo():
    L = n(0.6)
    sweep = tone([180 * (2400 / 180) ** (i / L) for i in range(L)], harm=((1, 1.0), (2, 0.3)))
    hum = tone([62] * L, harm=((1, 1.0), (2, 0.5)))
    shimmer = lowpass_noise(L, 0.5, 11)
    env = env_ad(L, 0.06, 0.25)
    return [(sweep[i] * (0.65 + 0.35 * math.sin(TAU * 28 * i / RATE)) * 0.7 + hum[i] * 0.3 + shimmer[i] * 0.2) * env[i]
            for i in range(L)]


for name, fn in [("holo", holo), ("click", click), ("comlink", comlink), ("blaster", blaster),
                 ("r2d2", r2d2), ("alarm", alarm)]:
    write(name, fn())

# Ambient chatter slots: silence (8 kHz like the original slots), see the header
for name in ("coms1", "coms2", "coms3"):
    write(name, [0.0] * 8000, rate=8000)
