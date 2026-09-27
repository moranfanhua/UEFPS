"""Generate the original, deterministic gunshot WAVs used by UnderTide.

Run with Python 3; no third-party packages or external audio are required.
"""

from __future__ import annotations

import math
import random
import struct
import wave
from pathlib import Path


RATE = 48_000
OUTPUT = Path(__file__).resolve().parent

# The short snap, body, bass pulse, bolt and room tail differ by weapon.
PROFILES = {
    "AK": dict(seconds=.29, seed=101, crack=.92, crack_decay=.010,
               body=.72, body_decay=.060, bass=.42, bass_hz=145,
               bolt_at=.072, bolt=.23, tail=.13, echo=.12),
    "M4": dict(seconds=.24, seed=202, crack=1.00, crack_decay=.008,
               body=.52, body_decay=.048, bass=.27, bass_hz=170,
               bolt_at=.058, bolt=.17, tail=.09, echo=.09),
    "MP5": dict(seconds=.16, seed=303, crack=.63, crack_decay=.006,
                body=.31, body_decay=.032, bass=.15, bass_hz=190,
                bolt_at=.040, bolt=.13, tail=.045, echo=.04),
    "AA12": dict(seconds=.38, seed=404, crack=.78, crack_decay=.013,
                 body=.96, body_decay=.090, bass=.72, bass_hz=110,
                 bolt_at=.112, bolt=.29, tail=.19, echo=.16),
}


def lowpass(samples: list[float], cutoff: float) -> list[float]:
    alpha = 1.0 - math.exp(-2.0 * math.pi * cutoff / RATE)
    result = []
    value = 0.0
    for sample in samples:
        value += alpha * (sample - value)
        result.append(value)
    return result


def band_noise(length: int, seed: int, low: float, high: float) -> list[float]:
    rng = random.Random(seed)
    source = [rng.uniform(-1.0, 1.0) for _ in range(length)]
    upper = lowpass(source, high)
    lower = lowpass(source, low)
    band = [a - b for a, b in zip(upper, lower)]
    rms = math.sqrt(sum(value * value for value in band) / length)
    return [value / rms for value in band]


def generate(name: str, profile: dict[str, float | int]) -> None:
    count = round(profile["seconds"] * RATE)
    seed = int(profile["seed"])
    crack = band_noise(count, seed, 2_400, 15_000)
    body = band_noise(count, seed + 1, 130, 3_300)
    room = band_noise(count, seed + 2, 180, 1_700)
    bolt = band_noise(count, seed + 3, 1_300, 8_000)
    raw = []
    phase = 0.0
    for index in range(count):
        t = index / RATE
        phase += 2.0 * math.pi * profile["bass_hz"] * math.exp(-t * 11.0) / RATE
        attack = min(1.0, t / .0007)
        snap = profile["crack"] * crack[index] * attack * math.exp(-t / profile["crack_decay"])
        weight = profile["body"] * body[index] * attack * math.exp(-t / profile["body_decay"])
        thump = profile["bass"] * math.sin(phase) * attack * math.exp(-t / .055)
        bolt_time = t - profile["bolt_at"]
        action = profile["bolt"] * bolt[index] * math.exp(-bolt_time / .004) if bolt_time >= 0 else 0.0
        room_time = t - .018
        ambience = profile["tail"] * room[index] * math.exp(-room_time / .065) if room_time >= 0 else 0.0
        raw.append(snap + weight + thump + action + ambience)

    # Two quiet early reflections add space without turning a burst into a second shot.
    dry = raw[:]
    for delay, gain in ((.032, profile["echo"]), (.073, profile["echo"] * .42)):
        offset = round(delay * RATE)
        for index in range(offset, count):
            raw[index] += dry[index - offset] * gain

    peak = max(abs(value) for value in raw)
    soft = [math.tanh(1.25 * value / peak) for value in raw]
    peak = max(abs(value) for value in soft)
    fade = round(.014 * RATE)
    pcm = []
    for index, value in enumerate(soft):
        ending = min(1.0, (count - 1 - index) / fade)
        pcm.append(round(28_000 * value / peak * ending))

    path = OUTPUT / f"Fire_{name}.wav"
    with wave.open(str(path), "wb") as output:
        output.setnchannels(1)
        output.setsampwidth(2)
        output.setframerate(RATE)
        output.writeframes(struct.pack(f"<{count}h", *pcm))
    rms = math.sqrt(sum(value * value for value in pcm) / count) / 32_768
    print(f"{path.name}: {count / RATE:.2f}s, RMS {rms:.3f}")


if __name__ == "__main__":
    for weapon, settings in PROFILES.items():
        generate(weapon, settings)
