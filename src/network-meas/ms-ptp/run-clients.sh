#!/bin/bash

if [ "$#" -ne 2 ]; then
    echo "Usage: $0 <number_of_clients> <number_of_servers>"
    exit 1
fi

num_clients_=$1
client_base_port=9000
server_base_port=8888

# Get number of running servers
num_servers=$2


# --- artifact-reproduction patch ---------------------------------------
# The original pinned servers to CPUs 63,62,... which fails outright on any
# machine with fewer cores.  Pin modulo the real core count instead; with
# more processes than cores the assignment simply wraps.
NCPU=$(nproc)
cpu_max_id=$((NCPU - 1))
# ----------------------------------------------------------------------

mkdir -p logs_$1/clients


# Start each client
for ((i=0; i<num_clients_; i++)); do
    client_port=$((client_base_port + i))
    ./follower $client_port $i $2 > logs_$1/clients/client_${i}.log 2>logs_$1/clients/client_${i}.stats &
    echo "Started client $i on port $client_port"
done

echo "Started $num_clients_ clients, connecting to $num_servers servers"