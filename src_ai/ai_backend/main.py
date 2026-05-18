import socket
import json
from agent import process_game_state

def start_server():
    host = '127.0.0.1'
    port = 8888

    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((host, port))
        s.listen()
        print(f"AI Backend listening on {host}:{port}")

        while True:
            conn, addr = s.accept()
            with conn:
                data = conn.recv(1024)
                if not data:
                    continue
                try:
                    payload = data.decode('utf-8')
                    state_dict = json.loads(payload)
                    print(f"Received GameState: {state_dict}")
                    
                    # Process state via LLM / Rule agent
                    action = process_game_state(state_dict)
                    
                    # Send action back
                    response_json = json.dumps(action)
                    print(f"Sending Action: {response_json}")
                    conn.sendall(response_json.encode('utf-8'))
                except json.JSONDecodeError:
                    print("Error: Malformed JSON received.")
                    conn.sendall(b'{"type": 0, "card_hand_idx": 0, "switch_to_idx": -1}')

if __name__ == "__main__":
    start_server()
