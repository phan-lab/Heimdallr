#!/bin/bash

if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <number_of_servers>"
    exit 1
fi

num_servers=$1
base_port=8888


# --- artifact-reproduction patch ---------------------------------------
# The original pinned servers to CPUs 63,62,... which fails outright on any
# machine with fewer cores.  Pin modulo the real core count instead; with
# more processes than cores the assignment simply wraps.
NCPU=$(nproc)
cpu_max_id=$((NCPU - 1))
# ----------------------------------------------------------------------

mkdir -p logs_$1/servers

for ((i=0; i<num_servers; i++)); do
    port=$((base_port + i))
    taskset -c $(( i % NCPU ))  ./upstream $port $i $num_servers > logs_$1/servers/server_${i}.log 2>logs_$1/servers/server_${i}.stats &
    echo "Started server $i on port $port"
done

echo "Started $num_servers servers"