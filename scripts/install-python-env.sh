#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FORGE_HOME="$(cd "$SCRIPT_DIR/.." && pwd)"
PYTHON_ENV="${PYTHON_ENV:-$FORGE_HOME/.venv}"
PYTHON="${PYTHON:-python3}"

# Python packages used by workload downloads, experiment orchestration, and
# optional workload analysis.
PYTHON_PACKAGES=(
  "gdown==5.2.0"
  "python-memcached==1.62"
  "fabric==3.2.2"
  "numpy==1.26.4"
  "pandas==2.2.3"
)

log() {
  printf '[install-python-env] %s\n' "$*"
}

die() {
  printf '[install-python-env] ERROR: %s\n' "$*" >&2
  exit 1
}

# Create a repository-local virtual environment so the artifact does not modify
# the system Python installation.
command -v "$PYTHON" >/dev/null 2>&1 || die "$PYTHON not found; run ./scripts/install-apt-deps.sh first"

if [ -x "$PYTHON_ENV/bin/python" ]; then
  log "Found existing Python virtual environment at $PYTHON_ENV"
elif [ -e "$PYTHON_ENV" ]; then
  die "$PYTHON_ENV exists but does not contain bin/python"
else
  log "Creating Python virtual environment at $PYTHON_ENV"
  if ! "$PYTHON" -m venv "$PYTHON_ENV"; then
    die "failed to create venv; install python3-venv and retry"
  fi
fi

PYTHON_BIN="$PYTHON_ENV/bin/python"
[ -x "$PYTHON_BIN" ] || die "python not found at $PYTHON_BIN"

log "Installing Python packages: ${PYTHON_PACKAGES[*]}"
"$PYTHON_BIN" -m pip install --upgrade pip
"$PYTHON_BIN" -m pip install "${PYTHON_PACKAGES[@]}"

cat <<EOF
[install-python-env] Done.
[install-python-env] Manual activation:
[install-python-env]   source "$PYTHON_ENV/bin/activate"
[install-python-env]
[install-python-env] Run this script once on the controller and on every machine in server_list.
EOF
