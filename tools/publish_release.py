#!/usr/bin/env python
"""Publica una entrega de Super Mario Clone: compila, arma el instalador y la version portable y crea el release en GitHub.

Uso (desde la carpeta del proyecto, con Python 3):
    python tools/publish_release.py 1.1.0 [--notas "texto"] [--sin-push]
    python tools/publish_release.py 1.0.0 --primera      (publica la version que ya figura en Version.h)

Que hace, en orden (el mismo esquema que Battle City):
  1. cambia la version en src/core/Version.h;
  2. compila en Release (build-release/, sin el editor aparte);
  3. arma dist/SuperMarioClone/ (exe, DLLs, assets, levels, LEEME.txt), dist/SuperMarioClone-Setup-<v>.exe (instalador) y
     dist/SuperMarioClone-PC.zip (portable);
  4. hace commit "Version <v>" y lo sube (git push);
  5. crea el release pc-v<v> con el instalador y el zip. El juego instalado lo busca por esa etiqueta y se actualiza solo.
Las credenciales de GitHub salen del administrador de credenciales de git (el mismo del push). Con --sin-push no toca git ni
GitHub (solo compila y arma los archivos).
IMPORTANTE: el juego compara su version con la del release mas nuevo: la version siempre tiene que SUBIR.
"""
import argparse, glob, json, os, re, shutil, subprocess, sys, urllib.error, urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = "Dann1994/SuperMarioClone"
MSYS = r"C:\msys64"
BASH = MSYS + r"\usr\bin\bash.exe"
VERSION_H = os.path.join(ROOT, "src", "core", "Version.h")
DIST = os.path.join(ROOT, "dist")
GAME = os.path.join(DIST, "SuperMarioClone")


def sh(cmd):
    """Corre un comando en bash de MSYS2 (con el compilador en el PATH)."""
    env = {**os.environ, "MSYSTEM": "MINGW64", "CHERE_INVOKING": "1"}
    print("$", cmd)
    subprocess.check_call([BASH, "-lc", "export PATH=/mingw64/bin:/usr/bin:$PATH; cd '%s' && %s" % (ROOT.replace("\\", "/"), cmd)],
                          env=env)


def read_version():
    return re.search(r'SMC_VERSION "([0-9.]+)"', open(VERSION_H, encoding="utf-8").read()).group(1)


def set_version(v):
    t = open(VERSION_H, encoding="utf-8").read()
    t = re.sub(r'SMC_VERSION "[0-9.]+"', 'SMC_VERSION "%s"' % v, t)
    open(VERSION_H, "w", encoding="utf-8", newline="\n").write(t)


def cmp_version(a, b):
    pa = [int(x) for x in a.split(".")]
    pb = [int(x) for x in b.split(".")]
    return (pa > pb) - (pa < pb)


def package():
    """La carpeta del juego: exe + DLLs (las que deja el build) + assets + niveles de la campana + LEEME."""
    out = os.path.join(ROOT, "build-release")
    if os.path.isdir(GAME):
        shutil.rmtree(GAME)
    os.makedirs(GAME)
    shutil.copy(os.path.join(out, "mario.exe"), GAME)
    for dll in glob.glob(os.path.join(out, "*.dll")):
        shutil.copy(dll, GAME)
    shutil.copytree(os.path.join(ROOT, "assets"), os.path.join(GAME, "assets"))
    # solo los mundos de la campana (levels/custom es del editor aparte)
    shutil.copytree(os.path.join(ROOT, "levels"), os.path.join(GAME, "levels"),
                    ignore=lambda d, names: [n for n in names if n == "custom"])
    shutil.copy(os.path.join(ROOT, "installer", "LEEME.txt"), GAME)
    print("carpeta del juego:", GAME, "-", len(glob.glob(os.path.join(GAME, "*.dll"))), "DLLs")


def token():
    out = subprocess.run(["git", "credential", "fill"], input="protocol=https\nhost=github.com\n\n", capture_output=True, text=True,
                         env={**os.environ, "GIT_TERMINAL_PROMPT": "0"}).stdout
    return [l.split("=", 1)[1] for l in out.splitlines() if l.startswith("password=")][0]


def api(tok, method, url, data=None, headers=None, raw=False):
    h = {"Authorization": "Bearer " + tok, "Accept": "application/vnd.github+json", "User-Agent": "smc-publish",
         "X-GitHub-Api-Version": "2022-11-28"}
    h.update(headers or {})
    body = data if raw else (json.dumps(data).encode() if data is not None else None)
    try:
        with urllib.request.urlopen(urllib.request.Request(url, data=body, method=method, headers=h), timeout=900) as r:
            txt = r.read()
            return json.loads(txt) if txt else {}
    except urllib.error.HTTPError as e:
        sys.exit("HTTP %d: %s" % (e.code, e.read().decode()[:400]))


def make_release(tok, tag, name, body, sha, files):
    r = api(tok, "POST", "https://api.github.com/repos/%s/releases" % REPO,
            {"tag_name": tag, "target_commitish": sha, "name": name, "body": body, "draft": False, "prerelease": False})
    for path, ctype in files:
        up = r["upload_url"].split("{")[0] + "?name=" + os.path.basename(path)
        a = api(tok, "POST", up, open(path, "rb").read(), {"Content-Type": ctype}, raw=True)
        print("  subido", a["browser_download_url"], a["size"])
    print("release", r["html_url"])


def main():
    ap = argparse.ArgumentParser(description="Publica una entrega de Super Mario Clone")
    ap.add_argument("version", help="version nueva, por ejemplo 1.1.0 (tiene que ser mayor que la actual)")
    ap.add_argument("--notas", default="", help="texto de las novedades (va en la descripcion del release)")
    ap.add_argument("--sin-push", action="store_true", help="solo compila y arma los archivos (no commit ni GitHub)")
    ap.add_argument("--primera", action="store_true", help="publica la version que ya figura en Version.h")
    a = ap.parse_args()
    if not re.fullmatch(r"[0-9]+(\.[0-9]+){1,2}", a.version):
        sys.exit("la version tiene que ser como 1.1.0")
    old = read_version()
    if cmp_version(a.version, old) <= 0 and not (a.primera and a.version == old):
        sys.exit("la version nueva (%s) tiene que ser mayor que la actual (%s)" % (a.version, old))
    set_version(a.version)
    sh("cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DMARIO_BUILD_EDITOR=OFF > /dev/null && "
       "cmake --build build-release --target mario")
    package()
    sh("bash installer/build_installer.sh")
    zip_path = os.path.join(DIST, "SuperMarioClone-PC.zip")
    if os.path.exists(zip_path):
        os.remove(zip_path)
    shutil.make_archive(zip_path[:-4], "zip", DIST, "SuperMarioClone")
    setup = os.path.join(DIST, "SuperMarioClone-Setup-%s.exe" % a.version)
    files = [(setup, "application/octet-stream"), (zip_path, "application/zip")]
    for f, _ in files:
        if not os.path.exists(f):
            sys.exit("falta " + f)
    if a.sin_push:
        print("Listo (sin publicar). Archivos en", DIST)
        return
    subprocess.check_call(["git", "-C", ROOT, "add", "-A"])                      # build*/ y dist/ estan en .gitignore
    subprocess.call(["git", "-C", ROOT, "commit", "-q", "-m", "Version %s: %s" % (a.version, (a.notas or "entrega")[:120]),
                     "-m", "Co-Authored-By: Claude Opus 5.5 <noreply@anthropic.com>"])
    subprocess.check_call(["git", "-C", ROOT, "push", "origin", "HEAD"])
    sha = subprocess.check_output(["git", "-C", ROOT, "rev-parse", "HEAD"], text=True).strip()
    tok = token()
    notas = a.notas or "Entrega %s." % a.version
    make_release(tok, "pc-v" + a.version, "Super Mario Clone %s - PC (Windows)" % a.version,
                 notas + "\n\nInstalador (`SuperMarioClone-Setup-%s.exe`) o version portable (`SuperMarioClone-PC.zip`). "
                 "El juego instalado busca y aplica las actualizaciones solo." % a.version, sha, files)
    print("Entrega %s publicada." % a.version)


if __name__ == "__main__":
    main()
