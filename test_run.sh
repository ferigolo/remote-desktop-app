#!/bin/bash
# Start signaling server
cd signaling-server
source .venv/bin/activate || true
uv run main.py > ../sig.log 2>&1 &
SIG_PID=$!
sleep 2
cd ..

# Start engine
cd engine/build
./core-engine > ../../core.log 2>&1 &
CORE_PID=$!
cd ../..

sleep 4

# Start client
cd engine/build
./native-client > ../../client.log 2>&1 &
CLIENT_PID=$!

sleep 5

# Kill everything
kill -9 $CLIENT_PID $CORE_PID $SIG_PID
echo "Done"
