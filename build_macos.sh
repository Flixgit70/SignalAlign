#!/bin/bash
set -euo pipefail

PROJECT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$PROJECT_DIR"

JUCE_PATH_DEFAULT="$HOME/JUCE"
JUCE_PATH="${JUCE_PATH:-$JUCE_PATH_DEFAULT}"

if [ ! -f "$JUCE_PATH/CMakeLists.txt" ]; then
    echo "ERROR: JUCE was not found at: $JUCE_PATH"
    echo "Set JUCE_PATH before running, for example:"
    echo "  JUCE_PATH=/Users/yourname/JUCE ./build_macos.sh"
    exit 1
fi

if ! command -v cmake >/dev/null 2>&1; then
    echo "ERROR: cmake not found. Install CMake first."
    exit 1
fi

GENERATOR="Ninja"
if ! command -v ninja >/dev/null 2>&1; then
    GENERATOR="Unix Makefiles"
fi

echo "SignalAlign 0.2.27 / SignalAlign 16"
echo "Project: $PROJECT_DIR"
echo "JUCE:    $JUCE_PATH"
echo "Build:   $PROJECT_DIR/build"
echo "Generator: $GENERATOR"

cmake -S . -B build -G "$GENERATOR" \
    -DJUCE_PATH="$JUCE_PATH" \
    -DCMAKE_BUILD_TYPE=Release

cmake --build build --config Release --target SignalAlign_VST3 SignalAlign_Standalone SignalAlign16_VST3

echo
echo "Build complete."
echo "COPY_PLUGIN_AFTER_BUILD is enabled, so SignalAlign.vst3 and SignalAlign 16.vst3 should also be copied to the default user VST3 folder."
