"""Synthesizes the game's sound effects and ambient music as 16-bit mono WAV files.

Run: python Tools/make_sounds.py <output dir>
The WAVs are imported into /Game/AquaticTowerDefense/Audio (see Tools/import_sounds.json).
"""
import math
import os
import random
import struct
import sys
import wave

RATE = 22050


def write(path, samples, rate=RATE, fade=True):
    if fade:
        # short fade-out so one-shots end on silence (no click)
        n = min(len(samples), int(0.012 * rate))
        samples = samples[:len(samples) - n] + [s * (1.0 - (i + 1) / n) for i, s in enumerate(samples[len(samples) - n:])]
    peak = max(1e-6, max(abs(s) for s in samples))
    scale = min(1.0, 0.95 / peak)
    with wave.open(path, 'wb') as w:
        w.setnchannels(1)
        w.setsampwidth(2)
        w.setframerate(rate)
        w.writeframes(b''.join(struct.pack('<h', int(max(-1.0, min(1.0, s * scale)) * 32767)) for s in samples))


def env(t, attack, decay):
    """Fast attack, exponential decay"""
    a = min(1.0, t / attack) if attack > 0 else 1.0
    return a * math.exp(-t / decay)


def lowpass(samples, cutoff):
    rc = 1.0 / (2 * math.pi * cutoff)
    alpha = (1.0 / RATE) / (rc + 1.0 / RATE)
    out, y = [], 0.0
    for s in samples:
        y += alpha * (s - y)
        out.append(y)
    return out


def tone(length, fn):
    n = int(length * RATE)
    return [fn(i / RATE) for i in range(n)]


def sweep(f0, f1, length, decay, wave_fn=math.sin, attack=0.004):
    phase = 0.0
    out = []
    n = int(length * RATE)
    for i in range(n):
        t = i / RATE
        f = f0 + (f1 - f0) * (t / length)
        phase += 2 * math.pi * f / RATE
        out.append(wave_fn(phase) * env(t, attack, decay))
    return out


def noise(length, decay, cutoff, attack=0.002, seed=1):
    rnd = random.Random(seed)
    raw = [rnd.uniform(-1, 1) * env(i / RATE, attack, decay) for i in range(int(length * RATE))]
    return lowpass(raw, cutoff)


def mix(*tracks):
    n = max(len(t) for t in tracks)
    return [sum(t[i] if i < len(t) else 0.0 for t in tracks) for i in range(n)]


def gain(samples, g):
    return [s * g for s in samples]


def delay(samples, offset):
    return [0.0] * int(offset * RATE) + samples


def bubble(f0, length=0.12, seed=0):
    # rising sine "bloop"
    return sweep(f0, f0 * 2.2, length, length * 0.35)


def square(p):
    return 1.0 if math.sin(p) >= 0 else -1.0


def make_all(out):
    os.makedirs(out, exist_ok=True)
    S = {}

    # tower shots
    S['Shot_Harpoon'] = mix(noise(0.25, 0.06, 2500, seed=2), gain(sweep(520, 140, 0.25, 0.07), 0.6))
    S['Shot_Mantis'] = mix(gain(noise(0.05, 0.008, 4500, seed=3), 0.8), gain(sweep(260, 70, 0.14, 0.04), 1.0))  # sharp snap and a low thump
    S['Shot_Rock'] = mix(gain(noise(0.08, 0.015, 5000, seed=4), 0.8), gain(sweep(900, 600, 0.08, 0.02), 0.4))
    S['Shot_Toxin'] = gain(lowpass(sweep(220, 300, 0.1, 0.035, attack=0.012), 1000), 0.8)  # soft low plip: it fires constantly
    S['Shot_Razor'] = mix(gain(lowpass(sweep(620, 500, 0.12, 0.04, attack=0.012), 1400), 0.6), gain(noise(0.12, 0.03, 1500, attack=0.012, seed=10), 0.25))  # soft muted swish: it fires constantly
    S['Shot_Puffer'] = mix(gain(sweep(220, 90, 0.3, 0.09), 0.9), noise(0.3, 0.05, 1200, seed=5))
    S['Shot_Jelly'] = gain(tone(0.5, lambda t: math.sin(2 * math.pi * (180 + 40 * math.sin(2 * math.pi * 7 * t)) * t) * env(t, 0.03, 0.15)), 0.7)
    S['Shot_Eel'] = gain(lowpass(tone(0.35, lambda t: square(2 * math.pi * (95 + 30 * math.sin(2 * math.pi * 31 * t)) * t) * random.uniform(0.6, 1.0) * env(t, 0.003, 0.11)), 3500), 0.45)

    # impacts and creatures
    S['Hit'] = mix(gain(noise(0.09, 0.02, 1800, seed=6), 0.7), gain(sweep(160, 90, 0.09, 0.03), 0.6))
    S['Kill'] = mix(noise(0.35, 0.07, 1400, seed=7), delay(bubble(350, 0.12), 0.02), delay(gain(bubble(520, 0.1), 0.7), 0.08), delay(gain(bubble(700, 0.09), 0.5), 0.15))
    S['ShieldPop'] = mix(gain(sweep(1400, 400, 0.25, 0.07), 0.6), noise(0.25, 0.05, 4000, seed=8))
    S['Leak'] = mix(gain(sweep(300, 180, 0.6, 0.3, square), 0.25), gain(sweep(150, 90, 0.6, 0.35), 0.6))

    # progression and UI
    notes = [523.25, 659.25, 783.99, 1046.5]
    S['LevelUp'] = mix(*[delay(gain(tone(0.45, lambda t, f=f: (math.sin(2 * math.pi * f * t) + 0.3 * math.sin(4 * math.pi * f * t)) * env(t, 0.005, 0.18)), 0.5), i * 0.07) for i, f in enumerate(notes)])
    horn = lambda t: sum(math.sin(2 * math.pi * f * t) / (k + 1) for k, f in enumerate([110, 220, 330, 440])) * env(t, 0.12, 0.9)
    S['WaveStart'] = mix(gain(tone(1.4, horn), 0.5), delay(gain(tone(1.2, lambda t: math.sin(2 * math.pi * 164.8 * t) * env(t, 0.1, 0.7)), 0.3), 0.15))
    S['Build'] = mix(gain(noise(0.2, 0.04, 900, seed=9), 0.9), gain(sweep(140, 70, 0.2, 0.06), 0.8))
    S['Sell'] = mix(*[delay(gain(tone(0.25, lambda t, f=f: math.sin(2 * math.pi * f * t) * env(t, 0.002, 0.08)), 0.5), i * 0.06) for i, f in enumerate([1318.5, 1760.0])])
    S['ItemDrop'] = mix(*[delay(gain(tone(0.4, lambda t, f=f: math.sin(2 * math.pi * f * t) * env(t, 0.002, 0.12)), 0.35), i * 0.05) for i, f in enumerate([1568, 2093, 2637, 3136])])
    S['Click'] = gain(sweep(1200, 900, 0.05, 0.012), 0.5)
    S['Upgrade'] = mix(*[delay(gain(tone(0.3, lambda t, f=f: math.sin(2 * math.pi * f * t) * env(t, 0.003, 0.1)), 0.45), i * 0.05) for i, f in enumerate([880, 1174.7, 1568])])

    for name, samples in S.items():
        write(os.path.join(out, 'S_%s.wav' % name), samples)

    # ambient music: slow pad chords with a gentle bubble layer, seamless 48 s loop
    length = 48.0
    n = int(length * RATE)
    chords = [[110.0, 164.81, 220.0, 261.63], [98.0, 146.83, 196.0, 246.94], [87.31, 130.81, 174.61, 220.0], [98.0, 146.83, 196.0, 233.08]]
    per = length / len(chords)
    music = [0.0] * n
    for ci, chord in enumerate(chords):
        start = ci * per
        for f in chord:
            for det in (-0.6, 0.6):
                ph = random.uniform(0, 6.28)
                for i in range(int(start * RATE), int(min(length, start + per + 3.0) * RATE)):
                    t = i / RATE - start
                    # long swell in and out, overlapping the next chord for a smooth change
                    a = min(1.0, t / 3.0) * min(1.0, max(0.0, (per + 3.0 - t) / 3.0))
                    music[i % n] += 0.05 * a * math.sin(2 * math.pi * (f + det) * t + ph) * (1.0 + 0.15 * math.sin(2 * math.pi * 0.2 * t))
    music = lowpass(music, 1400)
    rnd = random.Random(11)
    for k in range(70):
        at = rnd.uniform(0, length - 0.2)
        b = gain(bubble(rnd.uniform(400, 900), rnd.uniform(0.06, 0.12)), rnd.uniform(0.02, 0.05))
        base = int(at * RATE)
        for j, s in enumerate(b):
            music[(base + j) % n] += s
    write(os.path.join(out, 'S_Music.wav'), music, fade=False)


REEF_CHORDS = [[110.0, 164.81, 220.0, 261.63], [98.0, 146.83, 196.0, 246.94], [87.31, 130.81, 174.61, 220.0], [98.0, 146.83, 196.0, 233.08]]
# Kelp Crossing: D minor drift (Dm9, Bbmaj7, C6, Am7)
KELP_CHORDS = [[73.42, 110.0, 174.61, 220.0, 329.63], [58.27, 116.54, 174.61, 220.0, 293.66], [65.41, 130.81, 196.0, 220.0, 329.63], [55.0, 110.0, 164.81, 196.0, 261.63]]
# Sunken Ruins: E minor, modal and still (Em(add9), Cmaj7, Am7, Bsus4)
RUINS_CHORDS = [[82.41, 123.47, 164.81, 185.0, 246.94], [65.41, 130.81, 164.81, 196.0, 246.94], [55.0, 110.0, 164.81, 196.0, 261.63], [61.74, 123.47, 164.81, 185.0, 246.94]]


def make_boss_music(out, chords=None, name='S_MusicBoss.wav', seed=23):
    """Boss layer: the ambient track's chords and 48 s loop length (so the two crossfade in step), made heavier with
    a low saw drone an octave under each chord root, a slow war-drum pulse, a tremolo tritone for tension and a
    darker filter. Seeded, so running it again gives the same file."""
    length = 48.0
    n = int(length * RATE)
    rnd = random.Random(seed)
    chords = chords or REEF_CHORDS
    per = length / len(chords)
    music = [0.0] * n

    def saw(p):
        return 2.0 * ((p / (2 * math.pi)) % 1.0) - 1.0

    for ci, chord in enumerate(chords):
        start = ci * per
        root = chord[0]
        lo, hi = int(start * RATE), int(min(length, start + per + 2.0) * RATE)
        ph = [rnd.uniform(0, 6.28) for _ in range(8)]
        for i in range(lo, hi):
            t = i / RATE - start
            a = min(1.0, t / 1.5) * min(1.0, max(0.0, (per + 2.0 - t) / 2.0))
            s = 0.0
            # the same pad chord, a little quieter than in the ambient loop
            for k, f in enumerate(chord[:4]):
                s += 0.035 * math.sin(2 * math.pi * f * t + ph[k])
            # drone: two detuned saws an octave below the root, plus a sub sine
            s += 0.06 * (saw(2 * math.pi * (root * 0.5 - 0.4) * t + ph[4]) + saw(2 * math.pi * (root * 0.5 + 0.4) * t + ph[5]))
            s += 0.10 * math.sin(2 * math.pi * root * 0.25 * t)
            # tension: a tritone above the root, trembling at 6 Hz
            s += 0.022 * math.sin(2 * math.pi * root * 2.0 * 1.4142 * t + ph[6]) * (0.5 + 0.5 * math.sin(2 * math.pi * 6.0 * t))
            music[i % n] += a * s
    music = lowpass(music, 900)

    # war drums: a low thump every 0.75 s (64 per loop), the first of every four louder, with a soft off-beat tap
    beat = 0.75
    for k in range(int(length / beat)):
        accent = 1.0 if k % 4 == 0 else 0.6
        kick = mix(gain(sweep(95, 42, 0.45, 0.16), 0.55 * accent), gain(noise(0.12, 0.03, 500, seed=31 + k % 4), 0.25 * accent))
        base = int(k * beat * RATE)
        for j, s in enumerate(kick):
            music[(base + j) % n] += s
        tap = gain(noise(0.08, 0.02, 2200, seed=57 + k % 3), 0.07)
        base = int((k * beat + beat * 0.5) * RATE)
        for j, s in enumerate(tap):
            music[(base + j) % n] += s
    write(os.path.join(out, name), music, fade=False)


def make_tier_unlock(out):
    """Abyss tier unlocked: a short rising bell arpeggio (C5 E5 G5 C6) over a soft low swell, about 1.1 s. No randomness."""
    notes = [523.25, 659.25, 783.99, 1046.5]
    parts = []
    for i, f in enumerate(notes):
        last = i == len(notes) - 1
        decay = 0.45 if last else 0.2
        bell = tone(0.7 if last else 0.4, lambda t, f=f, d=decay: (math.sin(2 * math.pi * f * t) + 0.35 * math.sin(2 * math.pi * f * 2.0 * t) * math.exp(-t / 0.08)) * env(t, 0.004, d))
        parts.append(delay(gain(bell, 0.42 if last else 0.34), i * 0.11))
    swell = tone(1.1, lambda t: math.sin(2 * math.pi * 130.81 * t) * min(1.0, t / 0.25) * math.exp(-max(0.0, t - 0.25) / 0.3))
    parts.append(gain(swell, 0.18))
    write(os.path.join(out, 'S_TierUnlock.wav'), mix(*parts))


def add_into(music, samples, at):
    """Adds samples into a loop at a time in seconds, wrapping round the end so the loop stays seamless"""
    n = len(music)
    base = int(at * RATE)
    for j, v in enumerate(samples):
        music[(base + j) % n] += v


def pad(chords, length, level, seed, swell=3.0, cutoff=1400):
    """Slow detuned pad chords, each overlapping the next, as a seamless loop"""
    rnd = random.Random(seed)
    n = int(length * RATE)
    per = length / len(chords)
    music = [0.0] * n
    for ci, chord in enumerate(chords):
        start = ci * per
        for f in chord:
            for det in (-0.5, 0.5):
                ph = rnd.uniform(0, 6.28)
                w = 2 * math.pi * (f + det)
                for i in range(int(start * RATE), int(min(length + start, start + per + swell) * RATE)):
                    t = i / RATE - start
                    a = min(1.0, t / swell) * min(1.0, max(0.0, (per + swell - t) / swell))
                    music[i % n] += level * a * math.sin(w * t + ph) * (1.0 + 0.12 * math.sin(2 * math.pi * 0.15 * t))
    return lowpass(music, cutoff)


def mallet(f, length=0.5, decay=0.22):
    """Soft marimba-like note: fundamental plus a quickly fading fourth harmonic"""
    return tone(length, lambda t: (math.sin(2 * math.pi * f * t) + 0.35 * math.sin(2 * math.pi * f * 4.0 * t) * math.exp(-t / 0.03)) * env(t, 0.003, decay))


def bell(f, length=2.4, decay=0.9):
    """Struck bell: inharmonic partials that fade at different rates"""
    return tone(length, lambda t: (math.sin(2 * math.pi * f * t) * env(t, 0.004, decay)
                                   + 0.45 * math.sin(2 * math.pi * f * 2.76 * t) * env(t, 0.004, decay * 0.45)
                                   + 0.2 * math.sin(2 * math.pi * f * 5.4 * t) * env(t, 0.004, decay * 0.2)))


def make_kelp_music(out):
    """Kelp Crossing: D minor pads with a drifting marimba arpeggio in eighths (80 bpm, the boss drums' tempo) and a
    soft bass on each bar. Seeded, 48 s loop; S_MusicBoss2 shares its chords."""
    length = 48.0
    rnd = random.Random(41)
    chords = KELP_CHORDS
    per = length / len(chords)
    music = pad(chords, length, 0.035, seed=42, cutoff=1200)
    eighth = 0.375
    for ci, chord in enumerate(chords):
        tones = sorted(set([f * 2 for f in chord[1:]] + [f * 4 for f in chord[2:4]]))
        steps = int(per / eighth)
        idx = rnd.randrange(len(tones))
        for k in range(steps):
            # a gentle wandering line: mostly steps through the chord, some rests
            if rnd.random() < 0.22:
                continue
            idx = max(0, min(len(tones) - 1, idx + rnd.choice([-1, 1, 1, -1, 2, -2])))
            accent = 1.0 if k % 4 == 0 else 0.7
            add_into(music, gain(mallet(tones[idx]), 0.09 * accent * rnd.uniform(0.85, 1.0)), ci * per + k * eighth)
        for bar in range(int(per / 3.0)):
            root = chord[0]
            add_into(music, gain(tone(2.6, lambda t, f=root: math.sin(2 * math.pi * f * t) * env(t, 0.02, 0.9)), 0.16), ci * per + bar * 3.0)
    for k in range(40):
        add_into(music, gain(bubble(rnd.uniform(500, 1000), rnd.uniform(0.05, 0.1)), rnd.uniform(0.015, 0.035)), rnd.uniform(0, length))
    write(os.path.join(out, 'S_Music2.wav'), music, fade=False)
    make_boss_music(out, KELP_CHORDS, 'S_MusicBoss2.wav', seed=43)


def make_ruins_music(out):
    """Sunken Ruins: still E minor pads over a low drone, sparse bells from the E minor pentatonic and distant
    low knocks. Seeded, 48 s loop; S_MusicBoss3 shares its chords."""
    length = 48.0
    rnd = random.Random(61)
    chords = RUINS_CHORDS
    music = pad(chords, length, 0.032, seed=62, swell=4.0, cutoff=900)
    n = len(music)
    for i in range(n):
        t = i / RATE
        # drone on low E, breathing over 12 s (4 cycles per loop, so it joins up)
        music[i] += 0.05 * math.sin(2 * math.pi * 41.2 * t) * (0.7 + 0.3 * math.sin(2 * math.pi * t / 12.0))
    scale = [329.63, 392.0, 440.0, 493.88, 587.33, 659.25, 783.99]
    t = 0.75
    while t < length - 0.5:
        f = rnd.choice(scale)
        add_into(music, gain(bell(f), 0.07 * rnd.uniform(0.7, 1.0)), t)
        if rnd.random() < 0.3:
            # an answering note a fifth or octave below, a beat later
            add_into(music, gain(bell(f * rnd.choice([0.5, 0.667])), 0.05), t + 0.75)
        t += rnd.choice([1.5, 2.25, 3.0, 3.0, 4.5])
    for k in range(12):
        knock = mix(gain(sweep(140, 60, 0.3, 0.07), 0.5), gain(noise(0.15, 0.03, 700, seed=70 + k), 0.4))
        add_into(music, gain(lowpass(knock, 600), 0.12), rnd.uniform(0, length))
    write(os.path.join(out, 'S_Music3.wav'), music, fade=False)
    make_boss_music(out, RUINS_CHORDS, 'S_MusicBoss3.wav', seed=63)


def saw_wave(p):
    return 2.0 * ((p / (2 * math.pi)) % 1.0) - 1.0


def make_creature_sounds(out):
    """Per-creature death sounds (S_Death_<kind>), the sea star split and the boss wave sting. Seeded: running it
    again gives the same files."""
    rnd = random.Random(81)
    S = {}
    # crab: a shell crack (three quick clicks) over a small thud
    clicks = [delay(gain(noise(0.03, 0.006, 6000, seed=82 + k), 0.8 - 0.2 * k), k * 0.035) for k in range(3)]
    S['Death_Crab'] = mix(*clicks, gain(sweep(180, 90, 0.18, 0.05), 0.6), delay(gain(bubble(420, 0.1), 0.35), 0.06))
    # piranha: a quick wet splat
    S['Death_Piranha'] = mix(gain(noise(0.14, 0.03, 2600, seed=85), 0.8), gain(sweep(900, 300, 0.1, 0.03), 0.4), delay(gain(bubble(600, 0.08), 0.4), 0.03))
    # sea turtle: a heavy hollow thunk and a slow bubble trail
    S['Death_Turtle'] = mix(gain(sweep(130, 48, 0.5, 0.14), 1.0), gain(noise(0.3, 0.06, 600, seed=86), 0.5),
                            delay(gain(bubble(300, 0.14), 0.35), 0.12), delay(gain(bubble(380, 0.12), 0.25), 0.24))
    # manta: a soft whoosh falling away
    S['Death_Manta'] = mix(gain(noise(0.45, 0.14, 1800, attack=0.06, seed=87), 0.8), gain(sweep(420, 180, 0.45, 0.16, attack=0.05), 0.4))
    # kraken: a long low roar with a tremble, then a burst of rising bubbles
    roar = tone(1.6, lambda t: (saw_wave(2 * math.pi * (72 - 26 * t / 1.6) * t) * 0.6 + math.sin(2 * math.pi * 36 * t))
                * (0.75 + 0.25 * math.sin(2 * math.pi * 9 * t)) * env(t, 0.12, 0.6))
    S['Death_Kraken'] = mix(gain(lowpass(roar, 700), 1.0), gain(noise(1.4, 0.45, 900, attack=0.1, seed=88), 0.45),
                            *[delay(gain(bubble(rnd.uniform(250, 700), rnd.uniform(0.08, 0.14)), 0.25), 0.5 + k * 0.09) for k in range(10)])
    # sea star: a bright sparkling pop
    S['Death_SeaStar'] = mix(gain(bubble(900, 0.1), 0.6), delay(gain(tone(0.35, lambda t: math.sin(2 * math.pi * 2093 * t) * env(t, 0.002, 0.09)), 0.3), 0.03),
                             delay(gain(tone(0.3, lambda t: math.sin(2 * math.pi * 2637 * t) * env(t, 0.002, 0.07)), 0.22), 0.08), gain(noise(0.1, 0.02, 5000, seed=89), 0.3))
    # cuttlefish: an ink squirt, a dark gush that swells and falls, and a bloop
    S['Death_Cuttlefish'] = mix(gain(lowpass(noise(0.4, 0.12, 1100, attack=0.04, seed=90), 1100), 0.9), gain(sweep(260, 140, 0.3, 0.09), 0.4), delay(gain(bubble(330, 0.13), 0.4), 0.15))
    # shock eel: a crackling electric zap that fizzles out
    zap_rnd = random.Random(91)
    zap = tone(0.35, lambda t: (square(2 * math.pi * (120 + 60 * math.sin(2 * math.pi * 23 * t)) * t) * zap_rnd.uniform(0.4, 1.0)) * env(t, 0.002, 0.08))
    S['Death_ShockEel'] = mix(gain(lowpass(zap, 4000), 0.5), gain(noise(0.25, 0.04, 7000, seed=92), 0.4))
    # sea star split: a soft squelch and pop
    S['Split'] = mix(gain(noise(0.1, 0.025, 1500, seed=93), 0.6), delay(gain(bubble(480, 0.1), 0.6), 0.02))
    # boss wave: a deep horn chord over a big drum hit, with a dark tritone edge
    horn = lambda t: (sum(math.sin(2 * math.pi * f * t) / (k + 1) for k, f in enumerate([55.0, 82.41, 110.0, 165.0]))
                      + 0.25 * math.sin(2 * math.pi * 155.56 * t) * min(1.0, t / 0.6)) * env(t, 0.08, 1.1)
    drum = mix(gain(sweep(90, 34, 0.9, 0.3), 1.0), gain(noise(0.4, 0.08, 500, seed=94), 0.5))
    S['BossSting'] = mix(gain(lowpass(tone(2.4, horn), 1200), 0.55), drum, delay(gain(drum, 0.6), 0.75))
    for name, samples in S.items():
        write(os.path.join(out, 'S_%s.wav' % name), samples)


if __name__ == '__main__':
    # --boss-only <dir> / --tier-unlock-only <dir>: write only that file (a full run rerolls the unseeded Eel shot and ambient music)
    if len(sys.argv) > 1 and sys.argv[1] == '--extras-only':
        # creature deaths, split, boss sting and the Kelp / Ruins music sets, without touching the older files
        d = sys.argv[2] if len(sys.argv) > 2 else 'Saved/SoundSources'
        os.makedirs(d, exist_ok=True)
        make_creature_sounds(d)
        make_kelp_music(d)
        make_ruins_music(d)
    elif len(sys.argv) > 1 and sys.argv[1] == '--boss-only':
        make_boss_music(sys.argv[2] if len(sys.argv) > 2 else 'Saved/SoundSources')
    elif len(sys.argv) > 1 and sys.argv[1] == '--tier-unlock-only':
        make_tier_unlock(sys.argv[2] if len(sys.argv) > 2 else 'Saved/SoundSources')
    else:
        make_all(sys.argv[1] if len(sys.argv) > 1 else 'Saved/SoundSources')
        make_boss_music(sys.argv[1] if len(sys.argv) > 1 else 'Saved/SoundSources')
        make_tier_unlock(sys.argv[1] if len(sys.argv) > 1 else 'Saved/SoundSources')
        make_creature_sounds(sys.argv[1] if len(sys.argv) > 1 else 'Saved/SoundSources')
        make_kelp_music(sys.argv[1] if len(sys.argv) > 1 else 'Saved/SoundSources')
        make_ruins_music(sys.argv[1] if len(sys.argv) > 1 else 'Saved/SoundSources')
