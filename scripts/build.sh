#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"

BUILD_DIR="build"
BUILD_TYPE="Release"
GENERATOR=""
JOBS=""
CLEAN=0
CONFIGURE_ONLY=0

usage() {
  cat <<'USAGE'
Usage: build.sh [options]

Options:
  --build-dir <dir>                 Build directory (default: build)
  --type <Release|Debug|RelWithDebInfo>
                                   Build type (default: Release)
  --generator <Ninja|Unix Makefiles>
                                   CMake generator (default: Ninja if available)
  --jobs <n>                        Parallel build jobs
  --clean                           Delete build directory before configuring
  --configure-only                  Only run CMake configure step
  -h, --help                        Show this help

Examples:
  ./build.sh --type Release
  ./build.sh --clean --build-dir out/build
USAGE
}

die() {
  echo "error: $*" >&2
  exit 1
}

note() {
  echo "==> $*"
}

require_arg() {
  local flag="$1"
  local value="${2:-}"
  if [ -z "$value" ]; then
    die "Missing value for $flag"
  fi
}

while [ $# -gt 0 ]; do
  case "$1" in
    --build-dir)
      require_arg "$1" "${2:-}"
      BUILD_DIR="$2"
      shift 2
      ;;
    --type)
      require_arg "$1" "${2:-}"
      BUILD_TYPE="$2"
      shift 2
      ;;
    --generator)
      require_arg "$1" "${2:-}"
      GENERATOR="$2"
      shift 2
      ;;
    --jobs)
      require_arg "$1" "${2:-}"
      JOBS="$2"
      shift 2
      ;;
    --clean)
      CLEAN=1
      shift
      ;;
    --configure-only)
      CONFIGURE_ONLY=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      die "Unknown argument: $1"
      ;;
  esac
done

case "$BUILD_TYPE" in
  Release|Debug|RelWithDebInfo)
    ;;
  *)
    die "Invalid build type: $BUILD_TYPE"
    ;;
esac

if [ -z "$BUILD_DIR" ]; then
  die "Build directory cannot be empty"
fi

if [ "$BUILD_DIR" != /* ]; then
  BUILD_DIR="${ROOT_DIR}/${BUILD_DIR}"
fi

if [ "$CLEAN" -eq 1 ]; then
  if [ "$BUILD_DIR" = "/" ] || [ "$BUILD_DIR" = "$ROOT_DIR" ]; then
    die "Refusing to clean build directory: $BUILD_DIR"
  fi
  note "Cleaning build directory: $BUILD_DIR"
  rm -rf "$BUILD_DIR"
fi

if ! command -v cmake >/dev/null 2>&1; then
  die "cmake not found in PATH"
fi

OS_NAME="$(uname -s)"

if [ -z "$GENERATOR" ]; then
  if command -v ninja >/dev/null 2>&1; then
    GENERATOR="Ninja"
  else
    GENERATOR="Unix Makefiles"
  fi
fi

if [ "$GENERATOR" = "Ninja" ] && ! command -v ninja >/dev/null 2>&1; then
  die "Generator 'Ninja' requested but ninja is not installed"
fi

if [ -n "$JOBS" ]; then
  if ! [[ "$JOBS" =~ ^[0-9]+$ ]] || [ "$JOBS" -lt 1 ]; then
    die "Invalid jobs count: $JOBS"
  fi
else
  case "$OS_NAME" in
    Darwin)
      if command -v sysctl >/dev/null 2>&1; then
        JOBS="$(sysctl -n hw.logicalcpu 2>/dev/null || true)"
      fi
      ;;
    Linux)
      if command -v nproc >/dev/null 2>&1; then
        JOBS="$(nproc 2>/dev/null || true)"
      fi
      ;;
  esac
  if [ -z "${JOBS:-}" ] && command -v getconf >/dev/null 2>&1; then
    JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || true)"
  fi
  if ! [[ "${JOBS:-}" =~ ^[0-9]+$ ]] || [ "${JOBS:-0}" -lt 1 ]; then
    JOBS=1
  fi
fi

note "Configuring"
note "  Root:       $ROOT_DIR"
note "  Build dir:  $BUILD_DIR"
note "  Type:       $BUILD_TYPE"
note "  Generator:  $GENERATOR"
note "  Jobs:       $JOBS"
cmake -S "$ROOT_DIR" -B "$BUILD_DIR" \
  -G "$GENERATOR" \
  -DCMAKE_BUILD_TYPE="$BUILD_TYPE"

if [ "$CONFIGURE_ONLY" -eq 1 ]; then
  note "Configure-only requested; skipping build"
  exit 0
fi

note "Building"
cmake --build "$BUILD_DIR" -j "$JOBS"
