#!/bin/bash

# Full path to your server executable
SERVER_PATH="./lurk_ok"

echo "Starting Lurk server auto-restart loop..."
while true; do
    echo "[$(date)] Starting Lurk server..."
    
    # Run the server
    $SERVER_PATH 5005

    echo "[$(date)] Server stopped or crashed. Restarting in 5 seconds..."
    sleep 5
done
