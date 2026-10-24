#!/usr/bin/env bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR"

JUCE_PATH="${JUCE_PATH:-$HOME/JUCE}"
BUILD_DIR="${BUILD_DIR:-$PROJECT_DIR/build_linux}"

if [ ! -f "$JUCE_PATH/CMakeLists.txt" ]; then
    echo "ERROR: JUCE not found at: $JUCE_PATH"
    echo "Set JUCE_PATH, for example:"
    echo "  JUCE_PATH=/path/to/JUCE ./build_linux.sh"
    exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "ERROR: cmake not found."
    exit 1
fi

if command -v ninja >/dev/null 2>&1; then
    GENERATOR="Ninja"
else
    GENERATOR="Unix Makefiles"
fi

echo "SignalAlign Linux Build"
echo "Project:   $PROJECT_DIR"
echo "JUCE:      $JUCE_PATH"
echo "Build:     $BUILD_DIR"
echo "Generator: $GENERATOR"

cmake -S "$PROJECT_DIR" -B "$BUILD_DIR" -G "$GENERATOR" \
    -DJUCE_PATH="$JUCE_PATH" \
    -DCMAKE_BUILD_TYPE=Release

cmake --build "$BUILD_DIR" --config Release --target \
    SignalAlign_VST3 \
    SignalAlign_Standalone \
    SignalAlign16_VST3

echo
echo "Linux build complete."
echo "Standalone: $BUILD_DIR/SignalAlign_artefacts/Release/Standalone/SignalAlign"
echo "VST3:       $BUILD_DIR/SignalAlign_artefacts/Release/VST3/SignalAlign.vst3"
echo "16 VST3:    $BUILD_DIR/SignalAlign16_artefacts/Release/VST3/SignalAlign 16.vst3"
