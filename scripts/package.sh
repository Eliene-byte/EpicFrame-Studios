#!/usr/bin/env bash
# Builds the portable distribution folder. Runs in the MSYS2 MINGW64 shell.
set -euo pipefail

BUILD_DIR="${1:-build}"
OUT_DIR="${2:-dist}"
PREFIX="${MSYSTEM_PREFIX:-/mingw64}"

rm -rf "$OUT_DIR"
mkdir -p "$OUT_DIR"

cp "$BUILD_DIR/EpicFrameStudios.exe" "$OUT_DIR/"
cp "$PREFIX/bin/ffmpeg.exe" "$OUT_DIR/"

# Copy the complete DLL closure of the app and of ffmpeg.exe.
queue=("$OUT_DIR/EpicFrameStudios.exe" "$OUT_DIR/ffmpeg.exe")
for _ in 1 2 3 4 5 6; do
    next=()
    for file in "${queue[@]}"; do
        while read -r dll; do
            [ -z "$dll" ] && continue
            base="$(basename "$dll")"
            if [ ! -f "$OUT_DIR/$base" ]; then
                cp "$dll" "$OUT_DIR/"
                next+=("$OUT_DIR/$base")
            fi
        done < <(ldd "$file" 2>/dev/null | awk '/=> \/mingw/ {print $3}')
    done
    [ ${#next[@]} -eq 0 ] && break
    queue=("${next[@]}")
done

cp README.md LICENSE THIRD_PARTY.md "$OUT_DIR/" 2>/dev/null || true
if [ -f "LEIA-ME.txt" ]; then cp "LEIA-ME.txt" "$OUT_DIR/"; fi

VERSION="${GITHUB_REF_NAME:-0.1.0}"
VERSION="${VERSION#v}"
ZIP_NAME="EpicFrame-Studios-${VERSION}-windows-x64.zip"

(
    cd "$OUT_DIR"
    zip -9 -r "../$ZIP_NAME" . > /dev/null
)
mv "$ZIP_NAME" "$OUT_DIR/" 2>/dev/null || true

echo "Pacote criado:"
ls -lh "$OUT_DIR"
