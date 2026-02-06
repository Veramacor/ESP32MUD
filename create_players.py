#!/usr/bin/env python3
"""
Create 7 test players (playera - playerg) on the ESP32 MUD
Each player has same password as username
"""

import socket
import time
import sys

# Configuration
ESP32_IP = "192.168.50.2"      # Change to your ESP32 IP
MUD_PORT = 4000                  # Change to your MUD port
PLAYERS = ['a', 'b', 'c', 'd', 'e', 'f', 'g']  # Player suffixes
RACE = '0'  # 0=Human, 1=Elf, 2=Dwarf, 3=Orc, 4=Halfling

def receive_until_prompt(sock, timeout=2):
    """Receive data until we see a prompt or timeout"""
    sock.settimeout(timeout)
    data = b""
    try:
        while True:
            chunk = sock.recv(4096)
            if not chunk:
                break
            data += chunk
    except socket.timeout:
        pass
    except Exception as e:
        print(f"  Error receiving: {e}")
    
    return data.decode('utf-8', errors='ignore')

def create_player(player_suffix):
    """Create a single player"""
    player_name = f"player{player_suffix}"
    password = player_name  # Same as username
    
    print(f"\n{'='*60}")
    print(f"Creating {player_name}...")
    print(f"{'='*60}")
    
    try:
        # Connect
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        sock.connect((ESP32_IP, MUD_PORT))
        print(f"✓ Connected to {ESP32_IP}:{MUD_PORT}")
        
        # Step 1: Receive welcome and name prompt
        print("→ Waiting for name prompt...")
        response = receive_until_prompt(sock)
        if "Enter your name:" in response:
            print("✓ Got name prompt")
        else:
            print(f"⚠ Unexpected response: {response[:100]}")
        
        time.sleep(0.3)
        
        # Step 2: Send player name
        print(f"→ Sending name: {player_name}")
        sock.send(f"{player_name}\n".encode())
        time.sleep(0.5)
        
        # Step 3: Receive password prompt (for new character)
        response = receive_until_prompt(sock)
        if "New character" in response or "password" in response.lower():
            print("✓ Got password prompt (new character)")
        else:
            print(f"⚠ Unexpected response: {response[:100]}")
        
        time.sleep(0.3)
        
        # Step 4: Send password
        print(f"→ Sending password: {password}")
        sock.send(f"{password}\n".encode())
        time.sleep(0.5)
        
        # Step 5: Receive race selection
        response = receive_until_prompt(sock)
        if "Choose your race:" in response:
            print("✓ Got race selection prompt")
            print(f"  {response}")
        else:
            print(f"⚠ Unexpected response: {response[:100]}")
        
        time.sleep(0.3)
        
        # Step 6: Send race choice (0 = Human)
        print(f"→ Sending race: {RACE} (Human)")
        sock.send(f"{RACE}\n".encode())
        time.sleep(1)
        
        # Step 7: Receive welcome and room description
        response = receive_until_prompt(sock, timeout=3)
        if "Welcome" in response:
            print("✓ Character created successfully!")
            print(f"  Response contains: Welcome, Church description")
        else:
            print(f"⚠ Unexpected response: {response[:100]}")
        
        # Stay connected for 10 seconds
        print(f"→ Keeping {player_name} online for 10 seconds...")
        time.sleep(10)
        print(f"→ Logging out {player_name}...")
        
        # Disconnect with quit
        sock.send(b"quit\n")
        time.sleep(0.2)
        response = receive_until_prompt(sock, timeout=1)
        sock.close()
        print(f"✓ {player_name} logged out and disconnected")
        
        return True
        
    except socket.timeout:
        print(f"✗ Connection timeout for {player_name}")
        return False
    except ConnectionRefusedError:
        print(f"✗ Connection refused! Is ESP32 running at {ESP32_IP}:{MUD_PORT}?")
        return False
    except Exception as e:
        print(f"✗ Error creating {player_name}: {e}")
        return False

def main():
    print("\n" + "="*60)
    print("ESP32 MUD - Player Creation Script")
    print("="*60)
    print(f"Target: {ESP32_IP}:{MUD_PORT}")
    print(f"Players: playera - playerg")
    print(f"Password: same as username")
    print(f"Race: Human (0)")
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
        print("\nPlease check:")
        print("  1. ESP32 is powered on and connected")
        print("  2. MUD is running (check Serial output)")
        print(f"  3. IP address is correct: {ESP32_IP}")
        print(f"  4. Port is correct: {MUD_PORT}")
        sys.exit(1)
    
    time.sleep(1)
    
    # Create each player
    successful = 0
    failed = 0
    
    for suffix in PLAYERS:
        if create_player(suffix):
            successful += 1
        else:
            failed += 1
        
        time.sleep(1)  # Wait between player creations
    
    # Summary
    print("\n" + "="*60)
    print("CREATION SUMMARY")
    print("="*60)
    print(f"Successful: {successful}/{len(PLAYERS)}")
    print(f"Failed: {failed}/{len(PLAYERS)}")
    print("="*60)
    
    if successful == len(PLAYERS):
        print("\n✓ All players created successfully!")
        print("\nNext step: Run login_and_explore.py to log them all in and explore")
    else:
        print(f"\n⚠ {failed} player(s) failed to create. Check errors above.")
    
    print()

if __name__ == "__main__":
    main()
