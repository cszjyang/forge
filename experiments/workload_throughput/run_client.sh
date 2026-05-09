#!/bin/bash

. ../scripts/shell_settings.sh
echo "memcached_ip=$memcached_ip"

. ./common.sh
get_client_config $1
config_fname=$ret_config_fname

if [ -z $config_fname ]; then
    echo "Unsupported client type $1"
    exit
fi

st_client_id=$2
workload=$3
num_threads=8

"$FORGE_HOME/build/experiments/init" -C -c "$FORGE_HOME/experiments/configs/$config_fname" -w $workload -m $memcached_ip -n -1 -i $st_client_id -t $num_threads -A 64 -T 20
