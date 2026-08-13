#!/bin/bash
if [ "$#" -ne 1 ]; then
    echo "Usage: $0 <number_of_servers/clients>"
    exit 1
fi

rm -rf logs
./stop-all.sh
make
cd keygen; ./keygen $1; cd ..
./run-clients.sh $1 $1
./run-servers.sh $1 $1
