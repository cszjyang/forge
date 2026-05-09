#!/bin/bash

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
FORGE_HOME="$(cd "$script_dir/../.." && pwd)"
PYTHON_ENV="${PYTHON_ENV:-$FORGE_HOME/.venv}"
export PATH="$PYTHON_ENV/bin:$PATH"

# Configure this value for your cluster before running experiments.
memcached_ip="10.0.0.7"
