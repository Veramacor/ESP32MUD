#!/usr/bin/env python3
"""
Login 7 test players (playera - playerg) and have them randomly explore the MUD
Each player walks to random directions every 3 seconds
"""

import socket
import threading
import time
import random
import sys

# Configuration
ESP32_IP = "192.168.50.2"      # Change to your ESP32 IP
MUD_PORT = 4000                  # Change to your MUD port
PLAYERS = ['a', 'b', 'c', 'd', 'e', 'f', 'g']  # Player suffixes
DIRECTIONS = ['north', 'south', 'east', 'west']
EXPLORE_DURATION = 3000  # Explore for 5 minutes (300 seconds)

class MUDPlayer:
    def __init__(self, suffix):
        self.suffix = suffix
        self.name = f"player{suffix}"
        self.password = self.name
        self.sock = None
        self.connected = False
        self.running = False
        self.command_count = 0
        
    def receive_until_prompt(self, timeout=2):
        """Receive data until timeout"""
        self.sock.settimeout(timeout)
        data = b""
        try:
            while True:
                chunk = self.sock.recv(4096)
                if not chunk:
                    break
                data += chunk
        except socket.timeout:
            pass
        except Exception as e:
            print(f"[{self.name}] ✗ Error receiving: {e}")
        
        return data.decode('utf-8', errors='ignore')
    
    def login(self):
        """Login to MUD"""
        try:
            # Connect
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.connect((ESP32_IP, MUD_PORT))
            print(f"[{self.name}] ✓ Connected to server")
            
            # Step 1: Get welcome and name prompt
            response = self.receive_until_prompt()
            if "Enter your name:" not in response:
                print(f"[{self.name}] ⚠ Unexpected welcome response")
                return False
            
            time.sleep(0.2)
            
            # Step 2: Send name
            self.sock.send(f"{self.name}\n".encode())
            print(f"[{self.name}] → Sent name")
            time.sleep(0.3)
            
            # Step 3: Get password prompt
            response = self.receive_until_prompt()
            if "password" not in response.lower():
                print(f"[{self.name}] ⚠ Expected password prompt")
                return False
            
            time.sleep(0.2)
            
            # Step 4: Send password
            self.sock.send(f"{self.password}\n".encode())
            print(f"[{self.name}] → Sent password")
            time.sleep(0.5)
            
            # Step 5: Get welcome response
            response = self.receive_until_prompt(timeout=3)
            if "Welcome" in response:
                print(f"[{self.name}] ✓ Logged in successfully!")
                self.connected = True
                return True
            else:
                print(f"[{self.name}] ⚠ Unexpected login response")
                return False
                
        except socket.timeout:
            print(f"[{self.name}] ✗ Connection timeout")
            return False
        except ConnectionRefusedError:
            print(f"[{self.name}] ✗ Connection refused by server")
            return False
        except Exception as e:
            print(f"[{self.name}] ✗ Login error: {e}")
            return False
    
    def get_available_exits(self, response):
        """Parse 'Obvious exits: [directions]' from room description"""
        available = []
        for line in response.split('\n'):
            if 'Obvious exits:' in line:
                # Extract directions from line like "Obvious exits: north, south, east"
                after_colon = line.split('Obvious exits:')[1].strip()
                # Parse comma-separated directions
                exits = [d.strip().lower() for d in after_colon.split(',')]
                available = [d for d in exits if d in DIRECTIONS]
                break
        return available if available else DIRECTIONS  # Fall back to all directions
    
    def explore(self):
        """Randomly explore the MUD for EXPLORE_DURATION seconds"""
        if not self.connected:
            return
        
        self.running = True
        start_time = time.time()
        available_exits = DIRECTIONS.copy()  # Start with all directions
        
        print(f"[{self.name}] ✓ Starting exploration for {EXPLORE_DURATION} seconds")
        
        while self.running and (time.time() - start_time) < EXPLORE_DURATION:
            # Choose random direction from available exits
            direction = random.choice(available_exits) if available_exits else random.choice(DIRECTIONS)
            
            try:
                # Check if connection is still alive
                if not self.sock:
                    print(f"[{self.name}] ✗ Socket is None")
                    break
                
                # Send direction command
                self.sock.send(f"{direction}\n".encode())
                self.command_count += 1
                
                # Receive response with longer timeout to handle slow server
                response = self.receive_until_prompt(timeout=5)
                
                if not response:
                    # Empty response might mean connection is still processing
                    # Just wait and try again instead of disconnecting
                    pass
                else:
                    # Parse available exits from response
                    available_exits = self.get_available_exits(response)
                    
                    # Print status on first move and every 5 moves
                    if self.command_count % 5 == 1 or self.command_count <= 1:
                        elapsed = int(time.time() - start_time)
                        exits_str = ', '.join(available_exits) if available_exits else "none"
                        
                        # Get all response lines (full room description)
                        lines = response.strip().split('\n')
                        
                        # First line is room name/description
                        if lines:
                            room_line = lines[0]
                            # Remove [exits: ...] if inline
                            if "[exits:" in room_line:
                                room_line = room_line.split("[exits:")[0].strip()
                            # Print room info on first line
                            print(f"[{self.name}] ({elapsed}s) {room_line}")
                        
                        # Print exits on next line
                        print(f"                       [exits: {exits_str}]")
                
                # Wait before next move (1 second)
                time.sleep(1)
                
            except socket.timeout:
                # Timeout receiving - try next move
                print(f"[{self.name}] ⚠ Read timeout, continuing...")
                continue
            except Exception as e:
                print(f"[{self.name}] ✗ Error during exploration: {e}")
                break
        
        elapsed = int(time.time() - start_time)
        print(f"[{self.name}] ✓ Exploration complete ({elapsed}s, {self.command_count} moves)")
    
    def disconnect(self):
        """Disconnect from MUD"""
        if self.sock:
            try:
                self.sock.send(b"quit\n")
                time.sleep(0.2)
                self.sock.close()
                print(f"[{self.name}] ✓ Disconnected")
            except:
                pass
            self.connected = False

def run_player(player):
    """Run a single player's login and exploration"""
    if player.login():
        player.explore()
    player.disconnect()

def main():
    print("\n" + "="*60)
    print("ESP32 MUD - Multi-Player Explorer")
    print("="*60)
    print(f"Target: {ESP32_IP}:{MUD_PORT}")
    print(f"Players: {len(PLAYERS)} (playera - playerg)")
    print(f"Duration: {EXPLORE_DURATION} seconds per player")
    print(f"Directions: north, south, east, west (random)")
    print("="*60)
    
    # Verify we can connect
    print("\nTesting connection...")
    try:
        test_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        test_sock.settimeout(2)
        test_sock.connect((ESP32_IP, MUD_PORT))
        test_sock.close()
        print(f"✓ Successfully connected to {ESP32_IP}:{MUD_PORT}")
    except Exception as e:
        print(f"✗ Cannot connect to {ESP32_IP}:{MUD_PORT}")
        print(f"  Error: {e}")
        sys.exit(1)
    
    time.sleep(1)
    
    # Create player objects
    players = [MUDPlayer(suffix) for suffix in PLAYERS]
    
    # Login all players first (sequential to avoid overwhelming server)
    print(f"\nLogging in {len(players)} players...\n")
    for player in players:
        if not player.login():
            print(f"[{player.name}] Failed to login!")
            player.disconnect()
        time.sleep(0.5)  # Stagger logins
    
    # Count successful logins
    connected = sum(1 for p in players if p.connected)
    print(f"\n{'='*60}")
    print(f"Successfully logged in: {connected}/{len(players)} players")
    print(f"{'='*60}\n")
    
    if connected == 0:
        print("✗ No players logged in. Aborting.")
        sys.exit(1)
    
    time.sleep(2)
    
    # Start all explorations in parallel
    print(f"Starting exploration for {connected} players...\n")
    threads = []
    
    for player in players:
        if player.connected:
            thread = threading.Thread(target=run_player, args=(player,), daemon=False)
            thread.start()
            threads.append(thread)
            time.sleep(0.3)  # Stagger exploration starts
    
    # Wait for all threads to complete
    for thread in threads:
        thread.join()
    
    # Cleanup
    for player in players:
        player.running = False
        player.disconnect()
    
    # Summary
    print("\n" + "="*60)
    print("EXPLORATION SUMMARY")
    print("="*60)
    total_moves = sum(p.command_count for p in players)
    print(f"Total moves across all players: {total_moves}")
    for player in players:
        if player.command_count > 0:
            print(f"  {player.name}: {player.command_count} moves")
    print("="*60)
    print("\n✓ Multi-player exploration complete!")
    print()

if __name__ == "__main__":
    main()
