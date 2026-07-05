# Dictionary to keep track of connected peers. Format: { "id": websocket_connection }
import asyncio
import json
import websockets

peers = {}


async def handler(websocket):
    current_id = None
    try:
        async for message in websocket:
            data = json.loads(message)

            # 1. Registration Phase: A peer identifies itself
            if data.get("type") == "register":
                current_id = data.get("id")
                peers[current_id] = websocket
                print(f"✅ Peer registered: {current_id}")
                continue

            # 2. Relaying Phase: Forward messages (SDP or ICE) to the target
            target_id = data.get("target")
            if target_id and target_id in peers:
                print(f"➡️ Relaying message from {current_id} to {target_id}")

                # Forward the exact message to the target, appending the sender's ID
                data["sender"] = current_id
                await peers[target_id].send(json.dumps(data))
            else:
                print(f"⚠️ Target {target_id} not found or not registered.")

    except websocket.exceptions.ConnectionClosed:
        pass
    finally:
        # Cleanup when someone disconnects
        if current_id in peers:
            print(f"❌ Peer disconnected: {current_id}")
            del peers[current_id]


async def main():
    print("🚀 Signaling Server starting on ws://0.0.0.0:8080")
    # Run the server on port 8080
    async with websockets.serve(handler, "0.0.0.0", 8080):
        await asyncio.Future()  # Run forever


if __name__ == "__main__":
    asyncio.run(main())
