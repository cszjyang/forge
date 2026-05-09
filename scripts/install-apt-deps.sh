#!/usr/bin/env bash
set -euo pipefail

# Ubuntu/Debian packages needed to build FORGE and run the experiment scripts.
APT_PACKAGES=(
  build-essential
  ca-certificates
  cmake
  curl
  g++
  iproute2
  libboost-dev
  libmemcached-dev
  make
  memcached
  openssh-client
  pkg-config
  python3
  python3-pip
  python3-venv
  rsync
  wget
)

log() {
  printf '[install-apt-deps] %s\n' "$*"
}

die() {
  printf '[install-apt-deps] ERROR: %s\n' "$*" >&2
  exit 1
}

# Keep this script intentionally narrow: it installs packages only. Memcached
# service binding and experiment IP settings should stay explicit in the README.
if ! command -v apt-get >/dev/null 2>&1; then
  die "apt-get not found; this script supports Ubuntu/Debian systems only"
fi

# Use sudo when the script is run as a normal user.
SUDO=()
if [ "$(id -u)" -ne 0 ]; then
  command -v sudo >/dev/null 2>&1 || die "sudo not found; run as root or install sudo"
  SUDO=(sudo)
fi

# Install common build and runtime dependencies used on every node.
export DEBIAN_FRONTEND=noninteractive
log "Installing apt packages: ${APT_PACKAGES[*]}"
"${SUDO[@]}" apt-get update
"${SUDO[@]}" apt-get install -y "${APT_PACKAGES[@]}"

cat <<'EOF'
[install-apt-deps] Done.
[install-apt-deps] Next steps:
[install-apt-deps]   ./scripts/install-python-env.sh
EOF
