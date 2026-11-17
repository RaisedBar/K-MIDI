#!/bin/bash
# Builds the portable Mac version: build/macos/K-MIDI-<version>-mac.zip, holding K-MIDI.app.
#
# Run it on a Mac with Xcode, CMake and vcpkg (VCPKG_ROOT set), from any folder:
#   installer/make-mac-portable.sh
#
# It builds Release for Apple Silicon and for Intel (presets macos-arm64 and macos-x64), runs the tests,
# joins the two programs into one universal K-MIDI.app with lipo, signs it ad hoc and zips it.
# The libraries are linked statically, so the app needs nothing else installed.

set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

version="$(sed -n 's/^project(KMidi VERSION \([0-9.]*\).*/\1/p' CMakeLists.txt)"
[ -n "$version" ] || { echo "Couldn't find the version in CMakeLists.txt" >&2; exit 1; }

for arch in arm64 x64; do
    echo "== Configure ($arch)"
    cmake --preset "macos-$arch"
    echo "== Build ($arch)"
    cmake --build "build/macos-$arch" --config Release
done

# The tests run for this Mac's own architecture, and for the other one too if it can run it (Rosetta).
echo "== Test"
ctest --test-dir build/macos-arm64 -C Release --output-on-failure
if arch -x86_64 /usr/bin/true 2>/dev/null; then
    ctest --test-dir build/macos-x64 -C Release --output-on-failure
else
    echo "(Intel tests skipped: this Mac can't run Intel programs)"
fi

echo "== Universal app"
out="build/macos"
app="$out/K-MIDI.app"
rm -rf "$out"
mkdir -p "$out"
cp -R build/macos-arm64/Release/K-MIDI.app "$app"
lipo -create \
    build/macos-arm64/Release/K-MIDI.app/Contents/MacOS/K-MIDI \
    build/macos-x64/Release/K-MIDI.app/Contents/MacOS/K-MIDI \
    -output "$app/Contents/MacOS/K-MIDI"
lipo -info "$app/Contents/MacOS/K-MIDI"
# Not signed with a Developer ID yet: ad-hoc signing is the least macOS runs a program with
codesign --force --deep --sign - "$app"
codesign --verify --deep --strict "$app"

echo "== Zip"
zip="$out/K-MIDI-$version-mac.zip"
ditto -c -k --sequesterRsrc --keepParent "$app" "$zip"
echo "Made $root/$zip"
