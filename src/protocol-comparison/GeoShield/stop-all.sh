#!/bin/bash

echo "Stopping all processes..."
pkill -f "upstream [0-9]"
pkill -f "downstream [0-9]"

echo "All servers and clients have been stopped"