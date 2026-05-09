#!/usr/bin/env bash
set -euo pipefail

HUGEPAGES="${1:-20480}"

log() {
  printf '[setup-hugepages] %s\n' "$*"
}

die() {
  printf '[setup-hugepages] ERROR: %s\n' "$*" >&2
  exit 1
}

# This runtime setting is needed only on the memory nodes.
if ! [[ "$HUGEPAGES" =~ ^[0-9]+$ ]] || [ "$HUGEPAGES" -eq 0 ]; then
  die "hugepages must be a positive integer, got: $HUGEPAGES"
fi

if [ ! -w /proc/sys/vm/nr_hugepages ] && ! command -v sudo >/dev/null 2>&1; then
  die "sudo not found; run as root or install sudo"
fi

log "Run this script on memory nodes only."
log "Setting vm.nr_hugepages=$HUGEPAGES"

if [ -w /proc/sys/vm/nr_hugepages ]; then
  printf '%s\n' "$HUGEPAGES" > /proc/sys/vm/nr_hugepages
else
  printf '%s\n' "$HUGEPAGES" | sudo tee /proc/sys/vm/nr_hugepages >/dev/null
fi

log "Current hugepage status:"
grep -E 'HugePages_Total|HugePages_Free|HugePages_Rsvd|HugePages_Surp|Hugepagesize' /proc/meminfo
