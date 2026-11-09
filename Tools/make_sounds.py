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


def make_boss_music(out):
    """Boss layer: the ambient track's chords and 48 s loop length (so the two crossfade in step), made heavier with
    a low saw drone an octave under each chord root, a slow war-drum pulse, a tremolo tritone for tension and a
    darker filter. Seeded, so running it again gives the same file."""
    length = 48.0
    n = int(length * RATE)
    rnd = random.Random(23)
    chords = [[110.0, 164.81, 220.0, 261.63], [98.0, 146.83, 196.0, 246.94], [87.31, 130.81, 174.61, 220.0], [98.0, 146.83, 196.0, 233.08]]
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
            for k, f in enumerate(chord):
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
    write(os.path.join(out, 'S_MusicBoss.wav'), music, fade=False)


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


if __name__ == '__main__':
    # --boss-only <dir> / --tier-unlock-only <dir>: write only that file (a full run rerolls the unseeded Eel shot and ambient music)
    if len(sys.argv) > 1 and sys.argv[1] == '--boss-only':
        make_boss_music(sys.argv[2] if len(sys.argv) > 2 else 'Saved/SoundSources')
    elif len(sys.argv) > 1 and sys.argv[1] == '--tier-unlock-only':
        make_tier_unlock(sys.argv[2] if len(sys.argv) > 2 else 'Saved/SoundSources')
    else:
        make_all(sys.argv[1] if len(sys.argv) > 1 else 'Saved/SoundSources')
        make_boss_music(sys.argv[1] if len(sys.argv) > 1 else 'Saved/SoundSources')
        make_tier_unlock(sys.argv[1] if len(sys.argv) > 1 else 'Saved/SoundSources')
