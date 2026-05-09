#!/usr/bin/env bash
set -euo pipefail

FORGE_HOME="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${BUILD_DIR:-$FORGE_HOME/build}"

if command -v nproc >/dev/null 2>&1; then
  DEFAULT_JOBS="$(nproc)"
else
  DEFAULT_JOBS=1
fi
JOBS="${JOBS:-$DEFAULT_JOBS}"
CLEAN=0
CACHE_RESET_ARGS=(
  -U penalty_us
  -U hash_bucket_assoc
  -U hash_bucket_num
  -U max_group_size
  -U group_bytes
  -U eva_real_size
  -U num_thread_per_smm
  -U alloc_retry
  -U max_sample_num
  -U num_cn
  -U num_mn
)

usage() {
  cat <<EOF
Usage: ./build.sh [--clean] [cmake-options]

Environment variables:
  BUILD_DIR   Build directory. Default: $FORGE_HOME/build
  JOBS        Parallel build jobs. Default: $DEFAULT_JOBS

Examples:
  ./build.sh
  ./build.sh --clean
  ./build.sh -Dpenalty_us=100
  BUILD_DIR=/tmp/forge-build JOBS=4 ./build.sh
EOF
}

while [ $# -gt 0 ]; do
  case "$1" in
    -h|--help)
      usage
      exit 0
      ;;
    --clean)
      CLEAN=1
      shift
      ;;
    *)
      break
      ;;
  esac
done

if [ "$CLEAN" -eq 1 ]; then
  echo "[build] Removing $BUILD_DIR"
  rm -rf "$BUILD_DIR"
fi

echo "[build] Configuring in $BUILD_DIR"
cmake -S "$FORGE_HOME" -B "$BUILD_DIR" \
  "${CACHE_RESET_ARGS[@]}" \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=1 \
  -G "Unix Makefiles" \
  "$@"

echo "[build] Building with $JOBS job(s)"
cmake --build "$BUILD_DIR" --parallel "$JOBS"
