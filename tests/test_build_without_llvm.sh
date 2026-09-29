#!/usr/bin/env bash
set -euo pipefail

build_script="$1"
build_dir="$(mktemp -d)"
trap 'rm -rf "$build_dir"' EXIT

LLVM_DIR="$build_dir/missing-llvm" \
Clang_DIR="$build_dir/missing-clang" \
  bash "$build_script" --configure-only --build-dir "$build_dir/build" \
  >"$build_dir/output" 2>&1 || {
    cat "$build_dir/output" >&2
    exit 1
  }
