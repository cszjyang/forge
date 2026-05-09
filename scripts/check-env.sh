#!/usr/bin/env bash
set -u

EXPECTED_OFED_VERSION="${EXPECTED_OFED_VERSION:-MLNX_OFED_LINUX-24.10-2.1.8.0}"
EXPECTED_HUGEPAGES="${EXPECTED_HUGEPAGES:-20480}"
SSH_CONNECT_TIMEOUT="${SSH_CONNECT_TIMEOUT:-10}"

# Resolve project-local paths and the expected Python virtual environment.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FORGE_HOME="$(cd "$SCRIPT_DIR/.." && pwd)"
SETTINGS_FILE="$FORGE_HOME/experiments/scripts/utils/settings.py"
SHELL_SETTINGS_FILE="$FORGE_HOME/experiments/scripts/shell_settings.sh"
PYTHON_ENV="${PYTHON_ENV:-$FORGE_HOME/.venv}"
PYTHON_BIN="$PYTHON_ENV/bin/python"
failures=0
warnings=0

# Use colors only for interactive terminals. Piped logs stay plain text.
if [ -t 1 ] && [ -z "${NO_COLOR:-}" ]; then
  COLOR_RED="$(printf '\033[31m')"
  COLOR_YELLOW="$(printf '\033[33m')"
  COLOR_GREEN="$(printf '\033[32m')"
  COLOR_RESET="$(printf '\033[0m')"
else
  COLOR_RED=""
  COLOR_YELLOW=""
  COLOR_GREEN=""
  COLOR_RESET=""
fi

ok() {
  printf '[check-env] %sOK%s: %s\n' "$COLOR_GREEN" "$COLOR_RESET" "$*"
}

warn() {
  warnings=$((warnings + 1))
  printf '[check-env] %sWARNING%s: %s\n' "$COLOR_YELLOW" "$COLOR_RESET" "$*"
}

fail() {
  failures=$((failures + 1))
  printf '[check-env] %sMISSING%s: %s\n' "$COLOR_RED" "$COLOR_RESET" "$*"
}

check_command() {
  if command -v "$1" >/dev/null 2>&1; then
    ok "$1: $(command -v "$1")"
  else
    fail "$1 not found"
  fi
}

# Classify common non-interactive SSH failures into actionable messages.
ssh_failure_reason() {
  text="$1"
  lower="$(printf '%s' "$text" | tr '[:upper:]' '[:lower:]')"

  if printf '%s' "$lower" | grep -q 'timed out'; then
    printf 'connection timed out'
  elif printf '%s' "$lower" | grep -q -e 'permission denied' -e 'authentication failed' -e 'no authentication methods available'; then
    printf 'passwordless SSH is not configured'
  elif printf '%s' "$lower" | grep -q 'remote host identification has changed'; then
    printf 'host key changed'
  elif printf '%s' "$lower" | grep -q 'host key verification failed'; then
    printf 'host key verification failed'
  elif printf '%s' "$lower" | grep -q 'connection refused'; then
    printf 'SSH service refused the connection'
  elif printf '%s' "$lower" | grep -q -e 'could not resolve hostname' -e 'name or service not known'; then
    printf 'hostname cannot be resolved'
  elif printf '%s' "$lower" | grep -q -e 'no route to host' -e 'network is unreachable'; then
    printf 'network is unreachable'
  else
    printf 'SSH command failed'
  fi
}

# Check passwordless SSH to every node in experiments/scripts/utils/settings.py.
check_ssh_connectivity() {
  if ! command -v ssh >/dev/null 2>&1; then
    return
  fi
  if ! command -v python3 >/dev/null 2>&1; then
    warn "python3 not found; skipping SSH connectivity check from $SETTINGS_FILE"
    return
  fi
  if [ ! -r "$SETTINGS_FILE" ]; then
    warn "settings file not found; skipping SSH connectivity check: $SETTINGS_FILE"
    return
  fi

  mapfile -t ssh_nodes < <(python3 - "$SETTINGS_FILE" <<'PY'
import ast
import sys
from pathlib import Path

settings = Path(sys.argv[1])
mod = ast.parse(settings.read_text(), filename=str(settings))
values = {}
for node in mod.body:
    if isinstance(node, ast.Assign):
        for target in node.targets:
            if isinstance(target, ast.Name) and target.id in {"user", "server_list"}:
                values[target.id] = ast.literal_eval(node.value)

user = values.get("user", "")
for host in values.get("server_list", []):
    if "@" in host or not user:
        print(host)
    else:
        print(f"{user}@{host}")
PY
  )

  if [ "${#ssh_nodes[@]}" -eq 0 ]; then
    warn "server_list is empty; skipping SSH connectivity check"
    return
  fi

  for node in "${ssh_nodes[@]}"; do
    if output="$(ssh -o BatchMode=yes -o ConnectTimeout="$SSH_CONNECT_TIMEOUT" -o StrictHostKeyChecking=accept-new "$node" true 2>&1)"; then
      ok "passwordless SSH: $node"
    else
      reason="$(ssh_failure_reason "$output")"
      detail="$(printf '%s' "$output" | sed -n '1p')"
      if [ -n "$detail" ]; then
        fail "$reason for $node: $detail"
      else
        fail "$reason for $node"
      fi
    fi
  done
}

# Check hugepages only when this node is listed as a memory node.
check_memory_node_hugepages() {
  if ! command -v python3 >/dev/null 2>&1 || [ ! -r "$SETTINGS_FILE" ]; then
    return
  fi
  if ! command -v ip >/dev/null 2>&1; then
    warn "ip command not found; skipping memory node hugepage check"
    return
  fi

  mapfile -t mn_ips < <(python3 - "$SETTINGS_FILE" <<'PY'
import ast
import sys
from pathlib import Path

settings = Path(sys.argv[1])
mod = ast.parse(settings.read_text(), filename=str(settings))
for node in mod.body:
    if isinstance(node, ast.Assign):
        for target in node.targets:
            if isinstance(target, ast.Name) and target.id == "mn_list":
                for host in ast.literal_eval(node.value):
                    print(str(host).split("@", 1)[-1])
PY
  )

  [ "${#mn_ips[@]}" -ne 0 ] || return

  mapfile -t local_ips < <(ip -o -4 addr show | awk '{print $4}' | cut -d/ -f1)
  is_memory_node=0
  for mn_ip in "${mn_ips[@]}"; do
    for local_ip in "${local_ips[@]}"; do
      if [ "$mn_ip" = "$local_ip" ]; then
        is_memory_node=1
      fi
    done
  done

  [ "$is_memory_node" -eq 1 ] || return

  if [ ! -r /proc/sys/vm/nr_hugepages ]; then
    warn "cannot read /proc/sys/vm/nr_hugepages on memory node"
    return
  fi

  hugepages="$(cat /proc/sys/vm/nr_hugepages)"
  if [ "$hugepages" -ge "$EXPECTED_HUGEPAGES" ] 2>/dev/null; then
    ok "hugepages: $hugepages (memory node)"
  else
    warn "memory node hugepages too low: ${hugepages:-unknown}, expected at least $EXPECTED_HUGEPAGES; run ./scripts/setup-hugepages.sh $EXPECTED_HUGEPAGES"
  fi
}

# Basic tools needed by the orchestration scripts and build.
printf '[check-env] Checking FORGE environment\n'
printf '[check-env] PYTHON_ENV=%s\n' "$PYTHON_ENV"

for cmd in ssh rsync cmake make g++ python3; do
  check_command "$cmd"
done

check_ssh_connectivity

# Python environment used by Fabric, workload downloads, memcached control, and
# workload analysis.
if [ -x "$PYTHON_BIN" ]; then
  python_version="$("$PYTHON_BIN" --version 2>&1)"
  if package_check="$("$PYTHON_BIN" - <<'PY'
import importlib
import sys

packages = [
    ("gdown", "gdown"),
    ("fabric", "fabric"),
    ("python-memcached", "memcache"),
    ("numpy", "numpy"),
    ("pandas", "pandas"),
]

missing = []
for package, module in packages:
    try:
        importlib.import_module(module)
    except Exception:
        missing.append(package)

print(", ".join(missing or [package for package, _ in packages]))
sys.exit(1 if missing else 0)
PY
  )"; then
    ok "python: $python_version ($PYTHON_BIN), packages: $package_check"
  else
    fail "python packages missing in $PYTHON_BIN: $package_check; run ./scripts/install-python-env.sh"
  fi
else
  fail "python env not found at $PYTHON_BIN; run ./scripts/install-python-env.sh"
fi

# RDMA stack checks are warnings by default because driver installation is system-specific.
if command -v ofed_info >/dev/null 2>&1; then
  found_ofed="$(ofed_info -s 2>/dev/null | head -n 1 | sed 's/:$//')"
  if [ "$found_ofed" = "$EXPECTED_OFED_VERSION" ]; then
    ok "MLNX_OFED version: $found_ofed"
  else
    warn "MLNX_OFED version mismatch; expected $EXPECTED_OFED_VERSION, found ${found_ofed:-unknown}"
  fi
else
  warn "ofed_info not found; expected $EXPECTED_OFED_VERSION"
fi

if command -v ibv_devinfo >/dev/null 2>&1; then
  if ibv_devinfo >/dev/null 2>&1; then
    ok "ibv_devinfo succeeded"
  else
    warn "ibv_devinfo exists but failed"
  fi
else
  warn "ibv_devinfo not found"
fi

if [ -d /sys/class/infiniband ] && find /sys/class/infiniband -mindepth 1 -maxdepth 1 | grep -q .; then
  ok "RDMA devices found under /sys/class/infiniband"
else
  warn "no RDMA devices found under /sys/class/infiniband"
fi

check_memory_node_hugepages

# Runtime coordination and build-time libmemcached dependency.
if command -v memcached >/dev/null 2>&1; then
  ok "memcached: $(command -v memcached)"
else
  warn "memcached command not found; install it on the node that runs the controller service"
fi

if ldconfig -p 2>/dev/null | grep -q libmemcached; then
  ok "libmemcached found by ldconfig"
else
  warn "libmemcached not found by ldconfig; install libmemcached-dev or equivalent before building"
fi

memcached_ip="$(sed -n -E 's/^memcached_ip=["'\'']?([^"'\'']+)["'\'']?$/\1/p' "$SHELL_SETTINGS_FILE" 2>/dev/null | tail -n 1)"
if [ -n "$memcached_ip" ]; then
  if python3 - "$memcached_ip" <<'PY'
import socket
import sys

host = sys.argv[1]
try:
    socket.create_connection((host, 11211), timeout=3).close()
except OSError as exc:
    print(exc)
    sys.exit(1)
PY
  then
    ok "memcached reachable at $memcached_ip:11211"
  else
    warn "cannot connect to memcached at $memcached_ip:11211; check node-0 /etc/memcached.conf and service status"
  fi
else
  warn "memcached_ip not found in $SHELL_SETTINGS_FILE"
fi

# Nonzero exit means required user-space dependencies are missing.
printf '[check-env] Summary: %d failure(s), %d warning(s)\n' "$failures" "$warnings"
if [ "$failures" -ne 0 ]; then
  exit 1
fi
