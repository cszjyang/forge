#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
INSTALL_DIR="${INSTALL_DIR:-$SCRIPT_DIR/install}"
OFED_VERSION="${OFED_VERSION:-24.10-2.1.8.0}"
EXPECTED_OFED_VERSION="${EXPECTED_OFED_VERSION:-MLNX_OFED_LINUX-$OFED_VERSION}"
OFED_ARCH="${OFED_ARCH:-x86_64}"
OFED_INSTALL_OPTS="${OFED_INSTALL_OPTS:---add-kernel-support}"

# This script downloads and unpacks the MLNX_OFED installer only. Install it
# manually after inspecting the generated command below.
log() {
  printf '[prepare-mlnx-ofed] %s\n' "$*"
}

die() {
  printf '[prepare-mlnx-ofed] ERROR: %s\n' "$*" >&2
  exit 1
}

# Infer the MLNX_OFED OS tag for common distributions. Override with OFED_OS.
detect_os_tag() {
  if [ ! -r /etc/os-release ]; then
    return 1
  fi

  . /etc/os-release
  case "${ID:-}:${VERSION_ID:-}" in
    ubuntu:20.04) printf 'ubuntu20.04' ;;
    ubuntu:22.04) printf 'ubuntu22.04' ;;
    ubuntu:24.04) printf 'ubuntu24.04' ;;
    rhel:8.* | rocky:8.* | almalinux:8.* | centos:8.*) printf 'rhel8.9' ;;
    rhel:9.* | rocky:9.* | almalinux:9.* | centos:9.*) printf 'rhel9.4' ;;
    *) return 1 ;;
  esac
}

download_file() {
  mkdir -p "$(dirname "$2")"
  if command -v curl >/dev/null 2>&1; then
    curl -fL "$1" -o "$2"
  elif command -v wget >/dev/null 2>&1; then
    wget "$1" -O "$2"
  else
    die "curl or wget is required to download MLNX_OFED"
  fi
}

# Skip all work if the expected OFED version is already installed.
if command -v ofed_info >/dev/null 2>&1; then
  found_ofed="$(ofed_info -s 2>/dev/null | head -n 1 | sed 's/:$//')"
  if [ "$found_ofed" = "$EXPECTED_OFED_VERSION" ]; then
    log "Expected MLNX_OFED is already installed: $found_ofed"
    exit 0
  fi
  log "Current MLNX_OFED: ${found_ofed:-unknown}"
else
  log "ofed_info not found; MLNX_OFED may not be installed."
fi

if [ -z "${OFED_OS:-}" ]; then
  OFED_OS="$(detect_os_tag || true)"
fi

if [ -z "${OFED_OS:-}" ]; then
  die "Cannot infer OFED OS tag. Set OFED_OS explicitly, e.g. OFED_OS=ubuntu22.04"
fi

# Download and unpack the requested OFED tarball under scripts/install/.
OFED_TARBALL_NAME="MLNX_OFED_LINUX-$OFED_VERSION-$OFED_OS-$OFED_ARCH.tgz"
OFED_URL="${OFED_URL:-https://content.mellanox.com/ofed/MLNX_OFED-$OFED_VERSION/$OFED_TARBALL_NAME}"
OFED_TARBALL="${OFED_TARBALL:-$INSTALL_DIR/$OFED_TARBALL_NAME}"

if [ ! -f "$OFED_TARBALL" ]; then
  log "Downloading $OFED_URL"
  download_file "$OFED_URL" "$OFED_TARBALL"
else
  log "Using existing tarball $OFED_TARBALL"
fi

top_dir="$(tar -tzf "$OFED_TARBALL" | head -n 1 | cut -d/ -f1)"
[ -n "$top_dir" ] || die "Cannot inspect $OFED_TARBALL"

if [ ! -d "$INSTALL_DIR/$top_dir" ]; then
  log "Extracting $OFED_TARBALL to $INSTALL_DIR"
  mkdir -p "$INSTALL_DIR"
  tar -xzf "$OFED_TARBALL" -C "$INSTALL_DIR"
else
  log "Using existing extracted directory $INSTALL_DIR/$top_dir"
fi

cat <<EOF
[prepare-mlnx-ofed] Prepared MLNX_OFED installer.
[prepare-mlnx-ofed] Expected version: $EXPECTED_OFED_VERSION
[prepare-mlnx-ofed]
[prepare-mlnx-ofed] Manual install command:
[prepare-mlnx-ofed]   cd "$INSTALL_DIR/$top_dir"
[prepare-mlnx-ofed]   sudo ./mlnxofedinstall $OFED_INSTALL_OPTS
[prepare-mlnx-ofed]   sudo /etc/init.d/openibd restart
[prepare-mlnx-ofed]
[prepare-mlnx-ofed] Installing MLNX_OFED changes kernel modules and may require reboot.
EOF
