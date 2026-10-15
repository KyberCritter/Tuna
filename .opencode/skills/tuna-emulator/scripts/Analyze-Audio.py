#!/usr/bin/env python3
"""Measure a WAV with only the Python standard library; never infer hardware acceptance.
Supports PCM16/24/32 and IEEE float32 (including WAVE_FORMAT_EXTENSIBLE).
"""
import argparse
import json
import math
import statistics
import struct
from pathlib import Path


def read_wav(path):
    data = Path(path).read_bytes()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError("Expected a RIFF WAV")
    position, fmt, pcm = 12, None, None
    while position + 8 <= len(data):
        kind, size = struct.unpack_from("<4sI", data, position)
        body = data[position + 8:position + 8 + size]
        if len(body) != size:
            raise ValueError("Truncated WAV chunk")
        if kind == b"fmt ":
            fmt = body
        elif kind == b"data":
            pcm = body
        position += 8 + size + (size & 1)
    if fmt is None or pcm is None or len(fmt) < 16:
        raise ValueError("Missing WAV format/data")
    encoding, channels, rate, _, align, bits = struct.unpack_from("<HHIIHH", fmt)
    if encoding == 65534:
        if len(fmt) < 40:
            raise ValueError("Truncated extensible format")
        encoding = struct.unpack_from("<H", fmt, 24)[0]
    width = bits // 8
    if not channels or not rate or align != channels * width or len(pcm) % align:
        raise ValueError("Invalid WAV alignment")
    if encoding == 3 and bits == 32:
        values = [x[0] for x in struct.iter_unpack("<f", pcm)]
    elif encoding == 1 and bits in (16, 24, 32):
        scale = float(1 << (bits - 1))
        values = [int.from_bytes(pcm[i:i + width], "little", signed=True) / scale
                  for i in range(0, len(pcm), width)]
    else:
        raise ValueError(f"Unsupported encoding/bits: {encoding}/{bits}")
    if not all(math.isfinite(v) for v in values):
        raise ValueError("Non-finite audio samples")
    mono = [sum(values[i:i + channels]) / channels for i in range(0, len(values), channels)]
    per_channel = []
    for index in range(channels):
        channel = values[index::channels]
        channel_peak = max(map(abs, channel), default=0)
        channel_rms = math.sqrt(sum(v * v for v in channel) / len(channel)) if channel else 0
        per_channel.append(dict(index=index, peak=channel_peak, peak_dbfs=db(channel_peak),
                                rms=channel_rms, rms_dbfs=db(channel_rms)))
    return rate, channels, mono, per_channel


def db(value):
    return 20 * math.log10(value) if value > 0 else None


def analyze(path):
    rate, channels, samples, per_channel = read_wav(path)
    peak = max(map(abs, samples), default=0)
    threshold = max(peak * 0.01, 1e-5)
    active = [i for i, v in enumerate(samples) if abs(v) >= threshold]
    if not active:
        raise ValueError("Capture is silent; no playback acceptance")
    first, last = active[0], active[-1]
    signal = samples[first:last + 1]
    rms = math.sqrt(sum(v * v for v in signal) / len(signal))
    diffs = sorted(abs(b - a) for a, b in zip(signal, signal[1:]))
    # Diagnostic edge metrics, not a universal click detector: codec/notes also have edges.
    p99 = diffs[min(len(diffs) - 1, int(len(diffs) * 0.99))] if diffs else 0
    largest = diffs[-1] if diffs else 0
    second = [abs(signal[i + 1] - 2 * signal[i] + signal[i - 1]) for i in range(1, len(signal) - 1)]
    chunks = [signal[i:i + max(1, rate // 1000)] for i in range(0, len(signal), max(1, rate // 1000))]
    quiet_ms = sum(math.sqrt(sum(v * v for v in chunk) / len(chunk)) < rms * 0.01 for chunk in chunks)
    return dict(file=str(path), sample_rate=rate, channels=channels, per_channel=per_channel,
                capture_seconds=len(samples) / rate, active_start_seconds=first / rate,
                active_span_seconds=len(signal) / rate, peak=peak, peak_dbfs=db(peak),
                rms=rms, rms_dbfs=db(rms), dc=sum(signal) / len(signal),
                clipped_samples=sum(abs(v) >= 0.9999 for v in signal),
                maximum_sample_step=largest, p99_sample_step=p99,
                maximum_second_difference=max(second, default=0),
                rms_second_difference=math.sqrt(sum(v * v for v in second) / max(1, len(second))),
                quiet_millisecond_windows=quiet_ms,
                acceptance="diagnostic measurements only; inspect/listen and compare identical fixtures")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("wav")
    parser.add_argument("--output")
    args = parser.parse_args()
    result = json.dumps(analyze(args.wav), indent=2)
    if args.output:
        Path(args.output).write_text(result + "\n", encoding="utf-8")
    print(result)


if __name__ == "__main__":
    main()
