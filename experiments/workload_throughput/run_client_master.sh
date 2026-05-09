#!/bin/bash

RUN_GDB=false
# First pass: look for the -g option
for arg in "$@"; do
  if [ "$arg" = "-g" ]; then
    RUN_GDB=true
  fi
done
# Remove the -g option
set -- "${@//-g/}"

. ../scripts/shell_settings.sh
echo "memcached_ip=$memcached_ip"

. ./common.sh
get_client_config $1
config_fname=$ret_config_fname

if [ -z $config_fname ]; then
    echo "Unknown client type $1"
    exit
fi

st_client_id=$2
workload=$3
num_threads=$4
num_all_clients=$5

# echo "config_fname=$config_fname"
# echo "workload=$workload"
# echo "st_client_id=$st_client_id"
# echo "num_threads=$num_threads"
# echo "num_all_clients=$num_all_clients"



if $RUN_GDB; then
    gdb --args "$FORGE_HOME/build/experiments/init" -C -c "$FORGE_HOME/experiments/configs/$config_fname" -w $workload -m $memcached_ip -n -1 -i $st_client_id -t $num_threads -A $num_all_clients -T 20
else
    "$FORGE_HOME/build/experiments/init" -C -c "$FORGE_HOME/experiments/configs/$config_fname" -w $workload -m $memcached_ip -n -1 -i $st_client_id -t $num_threads -A $num_all_clients -T 20
fi
