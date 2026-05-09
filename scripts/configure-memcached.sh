#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FORGE_HOME="$(cd "$SCRIPT_DIR/.." && pwd)"
SETTINGS_FILE="$FORGE_HOME/experiments/scripts/utils/settings.py"
SHELL_SETTINGS_FILE="$FORGE_HOME/experiments/scripts/shell_settings.sh"
MEMCACHED_CONF="/etc/memcached.conf"
MEMCACHED_PORT=11211

log() {
  printf '[configure-memcached] %s\n' "$*"
}

die() {
  printf '[configure-memcached] ERROR: %s\n' "$*" >&2
  exit 1
}

# Strip an optional user@ prefix so the script can compare plain IPs.
normalize_host() {
  local host="$1"
  host="${host#*@}"
  host="${host#[}"
  host="${host%]}"
  printf '%s\n' "$host"
}

read_controller_ip() {
  command -v python3 >/dev/null 2>&1 || die "python3 not found"
  [ -r "$SETTINGS_FILE" ] || die "settings file not found: $SETTINGS_FILE"

  python3 - "$SETTINGS_FILE" <<'PY'
import ast
import sys
from pathlib import Path

settings = Path(sys.argv[1])
mod = ast.parse(settings.read_text(), filename=str(settings))
for node in mod.body:
    if not isinstance(node, ast.Assign):
        continue
    for target in node.targets:
        if isinstance(target, ast.Name) and target.id == "controller":
            value = ast.literal_eval(node.value)
            if not isinstance(value, str):
                raise SystemExit("controller is not a string")
            print(value)
            raise SystemExit(0)
raise SystemExit("controller not found")
PY
}

write_shell_settings() {
  local memcached_ip="$1"
  command -v python3 >/dev/null 2>&1 || die "python3 not found"
  [ -f "$SHELL_SETTINGS_FILE" ] || die "shell settings file not found: $SHELL_SETTINGS_FILE"
  [ -w "$SHELL_SETTINGS_FILE" ] || die "cannot write $SHELL_SETTINGS_FILE"

  python3 - "$SHELL_SETTINGS_FILE" "$memcached_ip" <<'PY'
import re
import sys
from pathlib import Path

path = Path(sys.argv[1])
memcached_ip = sys.argv[2]
lines = path.read_text().splitlines()
out = []
found = False

for line in lines:
    if re.match(r'^\s*memcached_ip\s*=', line):
        out.append(f'memcached_ip="{memcached_ip}"')
        found = True
    else:
        out.append(line)

if not found:
    out.append(f'memcached_ip="{memcached_ip}"')

path.write_text("\n".join(out) + "\n")
PY
}

get_local_ips() {
  if command -v ip >/dev/null 2>&1; then
    ip -o -4 addr show scope global | awk '{split($4, a, "/"); print a[1]}'
  elif command -v hostname >/dev/null 2>&1; then
    hostname -I 2>/dev/null | tr ' ' '\n' | sed '/^$/d'
  else
    die "cannot determine local IPv4 addresses"
  fi
}

configure_memcached_conf() {
  local memcached_ip="$1"
  local sudo_cmd=()
  if [ "$(id -u)" -ne 0 ]; then
    command -v sudo >/dev/null 2>&1 || die "sudo not found"
    sudo_cmd=(sudo)
  fi

  "${sudo_cmd[@]}" python3 - "$MEMCACHED_CONF" "$memcached_ip" <<'PY'
import re
import sys
from pathlib import Path

path = Path(sys.argv[1])
memcached_ip = sys.argv[2]
lines = path.read_text().splitlines()
out = []
seen = {"-l": False, "-I": False, "-m": False}

for line in lines:
    stripped = line.lstrip()
    if stripped.startswith("#"):
        out.append(line)
        continue
    if re.match(r'^-l(\s|$)', stripped):
        out.append(f"-l {memcached_ip}")
        seen["-l"] = True
    elif re.match(r'^-I(\s|$)', stripped):
        out.append("-I 128m")
        seen["-I"] = True
    elif re.match(r'^-m(\s|$)', stripped):
        out.append("-m 2048")
        seen["-m"] = True
    else:
        out.append(line)

for opt, present in seen.items():
    if present:
        continue
    if opt == "-l":
        out.append(f"-l {memcached_ip}")
    elif opt == "-I":
        out.append("-I 128m")
    elif opt == "-m":
        out.append("-m 2048")

path.write_text("\n".join(out) + "\n")
PY
}

restart_memcached() {
  local sudo_cmd=()
  if [ "$(id -u)" -ne 0 ]; then
    sudo_cmd=(sudo)
  fi

  if command -v systemctl >/dev/null 2>&1; then
    "${sudo_cmd[@]}" systemctl restart memcached
  elif command -v service >/dev/null 2>&1; then
    "${sudo_cmd[@]}" service memcached restart
  else
    die "neither systemctl nor service found"
  fi
}

check_memcached() {
  local memcached_ip="$1"
  python3 - "$memcached_ip" "$MEMCACHED_PORT" <<'PY'
import socket
import sys
import time

ip = sys.argv[1]
port = int(sys.argv[2])
attempts = 10
delay_seconds = 0.5
last_error = None

for attempt in range(1, attempts + 1):
    try:
        with socket.create_connection((ip, port), timeout=1):
            raise SystemExit(0)
    except OSError as exc:
        last_error = exc
        if attempt < attempts:
            time.sleep(delay_seconds)

print(last_error, file=sys.stderr)
raise SystemExit(1)
PY
}

main() {
  local controller_ip is_controller
  controller_ip="$(normalize_host "$(read_controller_ip)")"
  [ -n "$controller_ip" ] || die "controller IP is empty"

  write_shell_settings "$controller_ip"
  log "updated $SHELL_SETTINGS_FILE: memcached_ip=$controller_ip"

  is_controller=0
  while IFS= read -r local_ip; do
    if [ "$local_ip" = "$controller_ip" ]; then
      is_controller=1
      break
    fi
  done < <(get_local_ips)

  if [ "$is_controller" -eq 0 ]; then
    log "local node is not controller; skipped $MEMCACHED_CONF"
    return 0
  fi

  configure_memcached_conf "$controller_ip"
  restart_memcached

  if check_memcached "$controller_ip"; then
    log "memcached reachable at $controller_ip:$MEMCACHED_PORT"
  else
    die "memcached is not reachable at $controller_ip:$MEMCACHED_PORT after restart; check $MEMCACHED_CONF and service status"
  fi
}

main "$@"
