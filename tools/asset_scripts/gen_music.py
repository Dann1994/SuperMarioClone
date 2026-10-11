"""Convierte la musica de Documentaciones/Themes (MP3) a assets/sound/music (OGG).

Seccion 8.2 de la especificacion: musica en OGG con loop eficiente. Los MP3
son grabaciones con la intro, el ciclo repetido varias veces y un fade-out;
aca cada tema que se repite se recorta despues de su primer ciclo y se le
escriben las etiquetas LOOPSTART / LOOPLENGTH (en muestras), que SDL_mixer
respeta al reproducir en loop: la intro suena una sola vez y despues repite
exacto el ciclo, sin silencio ni salto.

Ademas genera la version acelerada de cada tema que se repite (*_fast.ogg),
la que suena cuando quedan menos de 100 de tiempo, como el original.

Los puntos de loop se encontraron con find_loop.py (correr de nuevo si se
cambia algun MP3) y aca se ajustan sobre el audio ya procesado (refine):
el largo del ciclo se vuelve a medir (acelerar no lo escala exacto) y el
punto de salto se corre unos milisegundos hasta donde las dos puntas de la
onda coinciden. Encima, los ultimos 30 ms del ciclo se funden con los 30 ms
anteriores a su inicio (CROSSFADE): al saltar, lo que sigue sonando es la
continuacion natural de lo que venia sonando, sin "clic" ni tropiezo de
ritmo aunque la grabacion no repita exacta muestra por muestra (las ondas
cuadradas del NES grabadas se desfasan un poco de una vuelta a la otra).

Uso, parado en la raiz del proyecto (necesita numpy y el ffmpeg de MSYS2:
pacman -S mingw-w64-x86_64-ffmpeg):
  python tools/asset_scripts/gen_music.py
"""
import os
import subprocess

import numpy as np

FFMPEG = r"C:\msys64\mingw64\bin\ffmpeg.exe"
SRC = "Documentaciones/Themes"
DST = "assets/sound/music"
RATE = 44100
# Velocidad de la version "apurada" (menos de 100 de tiempo).
HURRY_TEMPO = 1.4
# El loop arranca un poco despues del punto donde la musica empieza a
# repetirse: asi el salto siempre cae dentro de la parte repetida.
SAFETY = RATE  # 1 segundo
CROSSFADE = int(0.03 * RATE)

# nombre de salida: (archivo fuente, inicio de la repeticion, largo del ciclo)
# en muestras a 44100 Hz (find_loop.py). None = no se repite (jingle).
TRACKS = {
    "overworld": ("01. Ground Theme.mp3", 370688, 3803983),
    "underground": ("02. Underground Theme.mp3", 145408, 554728),
    "underwater": ("03. Underwater Theme.mp3", 0, 1127104),
    "castle": ("04. Castle Theme.mp3", 0, 352445),
    "invincibility": ("05. Invincibility Theme.mp3", 0, 281903),
    "level_complete": ("06. Level Complete Theme.mp3", None, None),
    "castle_complete": ("07. Castle Complete Theme.mp3", None, None),
    "game_over": ("09. Game Over Theme.mp3", None, None),
    "into_pipe": ("10. Into the Pipe Theme.mp3", None, None),
    "ending": ("11. Saved the Princess (NES).mp3", None, None),
}


def decode(src, tempo, channels):
    args = [FFMPEG, "-v", "error", "-i", os.path.join(SRC, src)]
    if tempo != 1.0:
        args += ["-af", f"atempo={tempo}"]
    args += ["-ac", str(channels), "-ar", str(RATE), "-f", "s16le", "-"]
    raw = subprocess.run(args, capture_output=True, check=True).stdout
    return np.frombuffer(raw, dtype=np.int16).astype(np.float32).reshape(-1, channels)


def spectral_frames(x, at, count, hop=512, size=2048):
    window = np.hanning(size)
    frames = np.array([np.log1p(np.abs(np.fft.rfft(x[at + i * hop:at + i * hop + size] * window))[:400])
                       for i in range(count)])
    frames -= frames.mean(axis=1, keepdims=True)
    return frames / (np.linalg.norm(frames, axis=1, keepdims=True) + 1e-9)


def refine(x, start, length):
    """Ajusta el ciclo sobre el audio procesado (ver el docstring)."""
    # 1) Largo grueso: parecido espectral de 6 s desde el inicio contra la
    #    misma zona un ciclo mas adelante, +-50 ms en pasos de 2.5 ms. Es lo
    #    robusto: no depende de la fase de la onda (que en la grabacion se
    #    corre un poco de una vuelta a la otra).
    count = int(6 * RATE / 512)
    reference = spectral_frames(x, start, count)
    best_len, best_sim = length, -1.0
    step = int(0.0025 * RATE)
    for offset in range(-int(0.05 * RATE), int(0.05 * RATE) + 1, step):
        if start + length + offset + count * 512 + 2048 >= len(x):
            continue
        sim = float(np.mean(np.sum(reference * spectral_frames(x, start + length + offset, count), axis=1)))
        if sim > best_sim:
            best_sim, best_len = sim, length + offset
    # 2) Fase fina: correlacion de la forma de onda, +-3 ms alrededor.
    ref = x[start:start + RATE // 4]
    coarse, best_c = best_len, -1.0
    for offset in range(-int(0.003 * RATE), int(0.003 * RATE) + 1):
        seg = x[start + coarse + offset:start + coarse + offset + len(ref)]
        if len(seg) < len(ref):
            continue
        c = float(np.dot(ref, seg) / (np.linalg.norm(ref) * np.linalg.norm(seg) + 1e-9))
        if c > best_c:
            best_c, best_len = c, coarse + offset
    # 3) Inicio: en los proximos 20 ms, el punto donde el salto (fin del
    #    ciclo -> inicio) empalma mejor en nivel y en pendiente.
    best_start, best_cost = start, float("inf")
    for s in range(start, start + int(0.02 * RATE)):
        e = s + best_len
        if e + 1 >= len(x):
            break
        cost = abs(x[e] - x[s]) + abs((x[e + 1] - x[e]) - (x[s + 1] - x[s]))
        if cost < best_cost:
            best_cost, best_start = cost, s
    return best_start, best_len, best_sim


def encode(src, dst, start=None, length=None, tempo=1.0):
    out = os.path.join(DST, dst)
    if start is None:
        # Jingle: conversion directa, sin loop.
        subprocess.run([FFMPEG, "-v", "error", "-y", "-i", os.path.join(SRC, src), "-ar", str(RATE), "-ac", "2",
                        "-c:a", "libvorbis", "-q:a", "5", out], check=True)
        return

    mono = decode(src, tempo, 1)[:, 0]
    loop_start, loop_length, corr = refine(mono, round((start + SAFETY) / tempo), round(length / tempo))
    loop_end = loop_start + loop_length
    print(f"  {dst}: inicio {loop_start / RATE:.3f}s, ciclo {loop_length / RATE:.3f}s, parecido {corr:.3f}")

    stereo = decode(src, tempo, 2)
    # Hasta el final del ciclo, con el fundido en el empalme, y despues medio
    # segundo de lo que sigue al inicio del ciclo (por si el decodificador
    # lee un poco mas alla del final del loop).
    pcm = stereo[:loop_end].copy()
    fade = np.linspace(0.0, 1.0, CROSSFADE, dtype=np.float32)[:, None]
    pcm[loop_end - CROSSFADE:loop_end] = (stereo[loop_end - CROSSFADE:loop_end] * (1.0 - fade) +
                                          stereo[loop_start - CROSSFADE:loop_start] * fade)
    pcm = np.concatenate([pcm, stereo[loop_start:loop_start + RATE // 2]])
    data = np.clip(pcm, -32768, 32767).astype(np.int16).tobytes()
    subprocess.run([FFMPEG, "-v", "error", "-y", "-f", "s16le", "-ar", str(RATE), "-ac", "2", "-i", "-",
                    "-metadata", f"LOOPSTART={loop_start}", "-metadata", f"LOOPLENGTH={loop_length}",
                    "-c:a", "libvorbis", "-q:a", "5", out], input=data, check=True)


def main():
    os.makedirs(DST, exist_ok=True)
    for name, (src, start, length) in TRACKS.items():
        encode(src, f"{name}.ogg", start, length)
        if start is not None:
            encode(src, f"{name}_fast.ogg", start, length, HURRY_TEMPO)
        print("ok", name)


if __name__ == "__main__":
    main()
