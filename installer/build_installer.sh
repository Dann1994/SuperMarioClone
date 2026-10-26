#!/usr/bin/env bash
# Arma el instalador de PC: dist/SuperMarioClone-Setup-<version>.exe (la version sale de src/core/Version.h).
# Necesita Inno Setup 6 (por defecto en C:/android-dev/tools/innosetup, el mismo de Battle City; ISCC=ruta/ISCC.exe para otra
# ubicacion) y la carpeta dist/SuperMarioClone armada (tools/publish_release.py la arma: exe, DLLs, assets, levels, LEEME.txt).
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ISCC="${ISCC:-/c/android-dev/tools/innosetup/ISCC.exe}"
VERSION="$(grep -o 'SMC_VERSION "[0-9.]*"' "$ROOT/src/core/Version.h" | grep -o '[0-9][0-9.]*')"
"$ISCC" "//DAppVersion=$VERSION" "$(cygpath -w "$ROOT/installer/SuperMarioClone.iss")"
echo "Instalador: $ROOT/dist/SuperMarioClone-Setup-$VERSION.exe"
