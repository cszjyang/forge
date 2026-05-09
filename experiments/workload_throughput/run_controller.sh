#!/bin/bash

. ../scripts/shell_settings.sh
echo "memcached_ip=$memcached_ip"

. ./common.sh

num_servers=$2
num_clients=$3
workload=$4
cache_size=$5

get_out_fname $1
out_fname=$ret_out_fname

if [ -z "$out_fname" ]; then
    echo "Unsupported server type $1"
    exit
fi

python "$FORGE_HOME/experiments/controller.py" $workload -s $num_servers -c $num_clients -o $out_fname -m $memcached_ip -r $cache_size
