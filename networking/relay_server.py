#!/usr/bin/env python3
"""
Tamagotchi Relay Server
Runs on Raspberry Pi to relay messages between two ESP32 devices
"""

import socket
import threading
import time

# Server configuration
HOST = '0.0.0.0'  # Listen on all interfaces
PORT = 8888

# Store connected clients
clients = []
clients_lock = threading.Lock()

def handle_client(conn, addr):
    """Handle individual client connection"""
    print(f"[+] New connection from {addr}")

    # Add client to list
    with clients_lock:
        clients.append(conn)

    try:
        # Send welcome message
        welcome = "CONNECTED\n"
        conn.send(welcome.encode())

        while True:
            # Receive data from client
            data = conn.recv(1024)
            if not data:
                break

            message = data.decode('utf-8').strip()
            print(f"[{addr}] Received: {message}")

            # Relay to all other clients
            with clients_lock:
                for client in clients:
                    if client != conn:  # Don't send back to sender
                        try:
                            client.send((message + '\n').encode())
                            print(f"[RELAY] Sent to other client: {message}")
                        except:
                            pass

    except Exception as e:
        print(f"[!] Error with client {addr}: {e}")

    finally:
        # Remove client from list
        with clients_lock:
            if conn in clients:
                clients.remove(conn)
        conn.close()
        print(f"[-] Connection closed: {addr}")

def main():
    """Start the relay server"""
    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)

    try:
        server.bind((HOST, PORT))
        server.listen(5)
        print(f"[*] Relay server listening on {HOST}:{PORT}")
        print(f"[*] Waiting for ESP32 clients to connect...")

        while True:
            conn, addr = server.accept()
            client_thread = threading.Thread(target=handle_client, args=(conn, addr))
            client_thread.daemon = True
            client_thread.start()

    except KeyboardInterrupt:
        print("\n[*] Server shutting down...")
    finally:
        server.close()

if __name__ == "__main__":
    main()