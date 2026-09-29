#!/usr/bin/env bash
set -euo pipefail

build_script="$1"
fixture_parent="$(mktemp -d)"
trap 'rm -rf "$fixture_parent"' EXIT

fixture_root="$fixture_parent/project"
mkdir -p "$fixture_root/scripts"
cp "$build_script" "$fixture_root/scripts/build.sh"
touch "$fixture_root/sentinel"

if bash "$fixture_root/scripts/build.sh" \
    --build-dir "$fixture_root/../project" --clean --configure-only \
    >"$fixture_parent/output" 2>&1; then
  echo "build script accepted the source directory as a build directory" >&2
  exit 1
fi

if [ ! -f "$fixture_root/sentinel" ]; then
  echo "build script deleted the source directory" >&2
  exit 1
fi
