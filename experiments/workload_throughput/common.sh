#!/bin/bash

function get_client_config() {
    ret_config_fname=""
    if [ "$1" = "forge" ]; then
        echo "client forge"
        python ../modify_single_config.py forge eviction_type=\"GROUP_LEVEL_EVICTION\"
        ret_config_fname="client_conf_forge.json"
    fi
}

function get_server_config() {
    ret_config_fname=""
    if [ "$1" = "forge" ]; then
        echo "server forge"
        python ../modify_single_config.py forge eviction_type=\"GROUP_LEVEL_EVICTION\"
        ret_config_fname="server_conf_forge.json"
    fi
}

function get_out_fname() {
    ret_out_fname=""
    if [ "$1" = "forge" ]; then
        echo "controller forge"
        ret_out_fname="forge"
    fi
}
