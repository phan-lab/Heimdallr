#!/bin/bash

echo "Stopping all processes..."
pkill -f "grand-master [0-9]"
pkill -f "follower [0-9]"

echo "All servers and clients have been stopped"