"""Busca el ciclo (loop) de una pista: donde empieza a repetirse y cuanto dura.

Lo usa gen_music.py (los resultados quedan escritos ahi). Necesita numpy y el
ffmpeg de MSYS2 (pacman -S mingw-w64-x86_64-ffmpeg).

Uso: python find_loop.py archivo.mp3
Imprime LOOPSTART y LOOPLENGTH en muestras a 44100 Hz.
"""
import subprocess
import sys

import numpy as np

FFMPEG = r"C:\msys64\mingw64\bin\ffmpeg.exe"


def decode(path, rate):
    raw = subprocess.run([FFMPEG, "-v", "error", "-i", path, "-ac", "1", "-ar", str(rate), "-f", "s16le", "-"],
                         capture_output=True, check=True).stdout
    return np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0


def features(x, hop=256, n=1024):
    frames = []
    win = np.hanning(n)
    for i in range(0, len(x) - n, hop):
        spec = np.abs(np.fft.rfft(x[i:i + n] * win))
        # 48 bandas log
        edges = np.unique(np.geomspace(2, len(spec) - 1, 49).astype(int))
        bands = np.array([spec[a:b].sum() for a, b in zip(edges[:-1], edges[1:])])
        frames.append(np.log1p(bands))
    f = np.array(frames)
    f -= f.mean(axis=1, keepdims=True)
    norm = np.linalg.norm(f, axis=1, keepdims=True) + 1e-9
    return f / norm


def main(path):
    rate = 11025
    hop = 256
    x = decode(path, rate)
    dur = len(x) / rate
    F = features(x, hop)
    nf = len(F)
    fps = rate / hop
    # Energia por frame (para detectar el fade-out / silencio final).
    energy = np.array([np.sqrt(np.mean(x[i:i + hop] ** 2)) for i in range(0, len(x) - hop, hop)])
    peak = np.percentile(energy, 95)
    loud = np.where(energy > peak * 0.25)[0]
    end_music = loud[-1] if len(loud) else nf

    best = None
    min_lag = int(2.0 * fps)
    max_lag = int(min(end_music * 0.8, nf - 10))
    scores = []
    for lag in range(min_lag, max_lag):
        a = F[: end_music - lag]
        b = F[lag:end_music]
        if len(a) < fps * 2:
            break
        s = float(np.mean(np.sum(a * b, axis=1)))
        scores.append((lag, s))
    if not scores:
        print("sin ciclo")
        return
    lags, vals = zip(*scores)
    vals = np.array(vals)
    top = vals.max()
    # El periodo mas corto con puntaje casi maximo (evita los multiplos).
    cand = [l for l, v in scores if v >= top - 0.02]
    lag = min(cand)
    # Inicio del ciclo: primer frame desde el que t y t+lag coinciden por
    # al menos 2 segundos seguidos.
    sim = np.sum(F[: nf - lag] * F[lag:], axis=1)
    w = int(2 * fps)
    start = 0
    for t in range(0, nf - lag - w):
        if np.mean(sim[t:t + w]) > 0.9:
            start = t
            break

    # Refinar a 44100 con correlacion cruzada sobre la forma de onda.
    hr = 44100
    X = decode(path, hr)
    s0 = int(start / fps * hr)
    L0 = int(lag / fps * hr)
    ref_t = s0 + int(1.0 * hr)
    ref = X[ref_t: ref_t + hr // 2]
    search = int(0.03 * hr)
    best_off, best_c = 0, -1e9
    for off in range(-search, search + 1):
        seg = X[ref_t + L0 + off: ref_t + L0 + off + len(ref)]
        if len(seg) < len(ref):
            continue
        c = float(np.dot(ref, seg) / (np.linalg.norm(ref) * np.linalg.norm(seg) + 1e-9))
        if c > best_c:
            best_c, best_off = c, off
    L = L0 + best_off
    # Ajustar el inicio: el primer punto donde x(t) == x(t+L) (busqueda hacia atras).
    print(f"duracion={dur:.2f}s periodo~{lag / fps:.2f}s inicio~{start / fps:.2f}s fin_musica~{end_music / fps:.2f}s")
    print(f"corr={best_c:.4f}")
    print(f"LOOPSTART={s0} LOOPLENGTH={L} (start {s0 / hr:.3f}s, len {L / hr:.3f}s, end {(s0 + L) / hr:.3f}s)")


if __name__ == "__main__":
    main(sys.argv[1])
