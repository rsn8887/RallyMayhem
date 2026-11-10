#!/bin/bash
# Builds Rally Mayhem for macOS and packs it into a .dmg (drag the app to Applications).
#
#   ./make_dmg.sh [build folder]      default: build-macos next to the project folder
#
# Needs Homebrew's sdl2, sdl2_ttf and cmake. The SDL libraries (and everything they link to) are copied
# into the app, so it runs on Macs without Homebrew. The app is signed ad hoc (no developer ID): the first
# time, open it with right-click > Open, or allow it under System Settings > Privacy & Security.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
BUILD="${1:-$ROOT/../build-macos}"
mkdir -p "$BUILD"
BUILD="$(cd "$BUILD" && pwd)"
VERSION="$(sed -n 's/^project(RallyMayhem VERSION \([0-9.]*\).*/\1/p' "$ROOT/CMakeLists.txt")"
APP="$BUILD/RallyMayhem.app"
DMG="$BUILD/RallyMayhem-$VERSION-macOS-$(uname -m).dmg"

echo "== building in $BUILD"
cmake -S "$ROOT" -B "$BUILD" -DCMAKE_BUILD_TYPE=Release
cmake --build "$BUILD" --target rallymayhem -j "$(sysctl -n hw.ncpu)"

echo "== copying libraries into the app"
FW="$APP/Contents/Frameworks"
EXE="$APP/Contents/MacOS/RallyMayhem"
rm -rf "$FW"
mkdir -p "$FW"

# library references of a binary that are not part of macOS (Homebrew and other local installs)
deps() {
  otool -L "$1" | tail -n +2 | awk '{print $1}' | grep -v -E '^(/usr/lib/|/System/)' || true
}
# turn a reference (absolute, @rpath, @loader_path) into a real file path; $2 = the binary that refers to it
resolve() {
  local ref="$1" from="$2" dir name
  dir="$(dirname "$from")"
  case "$ref" in
    @loader_path/*|@executable_path/*) echo "$dir/${ref#*/}" ;;
    @rpath/*)
      name="${ref#@rpath/}"
      for d in "$dir" "${HOMEBREW_PREFIX:-/opt/homebrew}/lib" /usr/local/lib; do
        if [ -e "$d/$name" ]; then echo "$d/$name"; return; fi
      done
      echo "$ref" ;;
    *) echo "$ref" ;;
  esac
}

# copy every library the game needs (recursively) and point all references at the copies
queue=("$EXE")
orig=("$EXE")          # where each queued binary came from, to resolve its relative references
i=0
bundle_deps() {
  while [ $i -lt ${#queue[@]} ]; do
    bin="${queue[$i]}"; src="${orig[$i]}"
    i=$((i + 1))
    for ref in $(deps "$bin"); do
      case "$ref" in @executable_path/../Frameworks/*) continue ;; esac   # already bundled
      path="$(resolve "$ref" "$src")"
      name="$(basename "$path")"
      if [ ! -e "$FW/$name" ]; then
        if [ ! -e "$path" ]; then echo "error: cannot find $ref (needed by $bin)"; exit 1; fi
        cp -L "$path" "$FW/$name"
        chmod u+w "$FW/$name"
        install_name_tool -id "@executable_path/../Frameworks/$name" "$FW/$name" 2>/dev/null
        queue+=("$FW/$name"); orig+=("$path")
        echo "   $name"
      fi
      install_name_tool -change "$ref" "@executable_path/../Frameworks/$name" "$bin" 2>/dev/null
    done
  done
}
bundle_deps

# Homebrew's "sdl2" can be sdl2-compat: SDL2 implemented on top of SDL3, which it loads at run time with
# dlopen("libSDL3.dylib"), first from its own folder. otool cannot see that, so SDL3 is copied in by hand.
for sdl2 in "$FW"/libSDL2*.dylib; do
  if [ -e "$sdl2" ] && [ ! -e "$FW/libSDL3.dylib" ] && grep -q "libSDL3" "$sdl2"; then
    for d in "${HOMEBREW_PREFIX:-/opt/homebrew}/lib" /usr/local/lib; do
      if [ -e "$d/libSDL3.dylib" ]; then
        cp -L "$d/libSDL3.dylib" "$FW/libSDL3.dylib"
        chmod u+w "$FW/libSDL3.dylib"
        install_name_tool -id "@executable_path/../Frameworks/libSDL3.dylib" "$FW/libSDL3.dylib" 2>/dev/null
        queue+=("$FW/libSDL3.dylib"); orig+=("$d/libSDL3.dylib")
        echo "   libSDL3.dylib (used by sdl2-compat)"
        break
      fi
    done
    if [ ! -e "$FW/libSDL3.dylib" ]; then echo "error: this SDL2 needs libSDL3.dylib, which was not found"; exit 1; fi
  fi
done
bundle_deps

echo "== signing"
for lib in "$FW"/*; do codesign --force --sign - "$lib"; done
codesign --force --sign - "$APP"
codesign --verify --deep --strict "$APP"

echo "== checking that nothing outside the app is referenced"
for bin in "$EXE" "$FW"/*; do
  if deps "$bin" | grep -v '^@executable_path/' | grep -q .; then
    echo "error: $bin still refers to:"; deps "$bin" | grep -v '^@executable_path/'; exit 1
  fi
done

echo "== making $DMG"
STAGE="$BUILD/dmg"
rm -rf "$STAGE" "$DMG"
mkdir -p "$STAGE"
cp -R "$APP" "$STAGE/Rally Mayhem.app"
ln -s /Applications "$STAGE/Applications"
hdiutil create -volname "Rally Mayhem" -srcfolder "$STAGE" -ov -format UDZO "$DMG" >/dev/null
rm -rf "$STAGE"
echo "done: $DMG"
