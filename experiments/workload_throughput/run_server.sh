#!/bin/bash

. ../scripts/shell_settings.sh
echo "memcached_ip=$memcached_ip"

. ./common.sh
get_server_config $1
config_fname=$ret_config_fname

if [ -z "$config_fname" ]; then
    echo "Unsupported server type $1"
    exit
fi

"$FORGE_HOME/build/experiments/init" -S -c "$FORGE_HOME/experiments/configs/$config_fname" -m $memcached_ip
