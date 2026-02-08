#!/usr/bin/env python3
"""
INTENSE MUD Stress Test: 7 players, 3 hours, rapid movement + random actions
- Fast movement when exits appear
- Random 'look', 'get all', 'save' commands
- Random shout messages
- Auto 'get all' when gold coins detected
- Emote interactions when players meet
"""

import socket
import threading
import time
import random
import sys

# Configuration
ESP32_IP = "192.168.50.2"
MUD_PORT = 4000
PLAYERS = ['a', 'b', 'c', 'd', 'e', 'f', 'g']
DIRECTIONS = ['north', 'south', 'east', 'west']
EXPLORE_DURATION = 10800  # 3 hours

# Funny shout messages
SHOUT_MESSAGES = [
    "is this mic on?!",
    "HELLO WORLD!",
    "anyone wanna fight?!",
    "I'm rich!",
    "who stole my cheese?!",
    "THIS IS SPARTA!",
    "I love this MUD!",
    "lag much?",
    "can i get a buff?",
    "where is everyone?",
    "STOP HITTING ME!",
    "follow me!",
    "run away!",
    "help me!!!",
    "i found treasure!",
    "anyone need a healer?",
    "watch this!",
    "oops",
    "did that just happen?",
    "AGAIN?!",
]

# Emotes extracted from ESP32MUD.cpp
EMOTES = [
    "admire", "announce", "apologize", "applaud", "approach", "beam", "beck",
    "beckon", "blush", "boast", "boop", "bounce", "bow", "brag", "bump",
    "caress", "celebrate", "chant", "cheer", "chirp", "chortle", "circle",
    "clap", "comfort", "compliment", "congratulate", "console", "cough", "cry",
    "curtsy", "dance", "declare", "encourage", "farewell", "flex", "flinch",
    "freeze", "frown", "gaze", "gesture", "giggle", "glance", "glare", "glomp",
    "glow", "grin", "grimace", "groan", "growl", "hail", "harmonize", "hover",
    "hug", "hum", "inspect", "invite", "jump", "kick", "kiss", "kneel", "laugh",
    "lean", "lick", "mourn", "mock", "moan", "muse", "nudge", "nuzzle", "pat",
    "peer", "pester", "point", "poke", "ponder", "pose", "praise", "pray", "prod",
    "purr", "recite", "regard", "retreat", "roar", "salivate", "salute", "scoff",
    "scoot", "scowl", "shake", "shiver", "shout", "shrug", "shudder", "sigh",
    "sing", "skip", "smile", "smirk", "snarl", "snicker", "sniff", "snort",
    "snuggle", "sneeze", "study", "stare", "stretch", "sway", "tap", "taunt",
    "tease", "thank", "think", "threaten", "tickle", "tilt", "tremble", "wave",
    "welcome", "whimper", "whine", "whirl", "whistle", "wink", "yawn", "puke",
    "barf", "gag", "fart", "spit", "choke", "burp", "belch", "heave", "retch",
    "gunk", "phlegm", "hack", "wheeze", "fume", "splutter", "muck", "slime",
]

class MUDPlayer:
    def __init__(self, suffix):
        self.suffix = suffix
        self.name = f"player{suffix}"
        self.password = self.name
        self.sock = None
        self.connected = False
        self.running = False
        self.command_count = 0
        self.last_room_line = ""
        self.last_emoted_with = None  # Prevent emote loops
        
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
            self.sock.settimeout(5)
            self.sock.connect((ESP32_IP, MUD_PORT))
            
            # Wait for login prompt
            response = self.receive_until_prompt(timeout=2)
            
            # Send name
            self.sock.send(f"{self.name}\n".encode())
            response = self.receive_until_prompt(timeout=2)
            
            # Send password
            self.sock.send(f"{self.password}\n".encode())
            response = self.receive_until_prompt(timeout=2)
            
            # Check for login success or rejection
            if "SERVER STATUS" in response or "full at this time" in response.lower():
                print(f"[{self.name}] ⚠ Server is full - continuing with other players")
                self.sock.close()
                return False
            elif "successfully" in response.lower() or "welcome" in response.lower():
                self.connected = True
                print(f"[{self.name}] ✓ Logged in successfully!")
                return True
            else:
                print(f"[{self.name}] ✗ Login failed")
                self.sock.close()
                return False
                
        except Exception as e:
            print(f"[{self.name}] ✗ Connection error: {e}")
            return False
    
    def get_available_exits(self, response):
        """Parse 'Obvious exits: [directions]' from room description"""
        available = []
        for line in response.split('\n'):
            if 'Obvious exits:' in line:
                after_colon = line.split('Obvious exits:')[1].strip()
                exits = [d.strip().lower() for d in after_colon.split(',')]
                available = [d for d in exits if d in DIRECTIONS]
                break
        return available if available else DIRECTIONS
    
    def has_gold_coins(self, response):
        """Check if response mentions gold coins"""
        return "gold coin" in response.lower() or "gold coins" in response.lower()
    
    def get_room_players(self, response):
        """Extract list of other players in room from response"""
        players_in_room = []
        for line in response.split('\n'):
            for suffix in PLAYERS:
                other_name = f"player{suffix}"
                if other_name != self.name and other_name in line and "Also here:" in line:
                    players_in_room.append(other_name)
        return players_in_room
    
    def do_random_action(self):
        """Randomly choose an action: look, get all, save, or shout"""
        action = random.choice(['look', 'get_all', 'save', 'shout'])
        
        if action == 'look':
            self.sock.send(b"look\n")
        elif action == 'get_all':
            self.sock.send(b"get all\n")
        elif action == 'save':
            self.sock.send(b"save\n")
        elif action == 'shout':
            msg = random.choice(SHOUT_MESSAGES)
            self.sock.send(f"shout {msg}\n".encode())
        
        return self.receive_until_prompt(timeout=3)
    
    def do_emote_with_player(self, target_name):
        """Do a random emote at another player (only once per meeting)"""
        if self.last_emoted_with == target_name:
            return  # Already did emote with this player
        
        emote = random.choice(EMOTES)
        self.sock.send(f"{emote} {target_name}\n".encode())
        self.last_emoted_with = target_name
        self.receive_until_prompt(timeout=2)
    
    def explore(self):
        """Rapidly explore the MUD with random actions and interactions"""
        if not self.connected:
            return
        
        self.running = True
        start_time = time.time()
        available_exits = DIRECTIONS.copy()
        
        print(f"[{self.name}] ✓ Starting INTENSE exploration for {EXPLORE_DURATION} seconds")
        
        while self.running and (time.time() - start_time) < EXPLORE_DURATION:
            # Choose random direction from available exits
            direction = random.choice(available_exits) if available_exits else random.choice(DIRECTIONS)
            
            try:
                if not self.sock:
                    print(f"[{self.name}] ✗ Socket is None")
                    break
                
                # FAST: Send direction immediately
                self.sock.send(f"{direction}\n".encode())
                self.command_count += 1
                
                # Receive response
                response = self.receive_until_prompt(timeout=5)
                
                if response:
                    # Parse exits for next move
                    available_exits = self.get_available_exits(response)
                    
                    # Auto 'get all' if gold coins detected
                    if self.has_gold_coins(response):
                        self.sock.send(b"get all\n")
                        self.receive_until_prompt(timeout=2)
                    
                    # Check for other players in room - do emote interaction
                    other_players = self.get_room_players(response)
                    if other_players:
                        target = random.choice(other_players)
                        self.do_emote_with_player(target)
                    else:
                        self.last_emoted_with = None  # Reset for next meeting
                    
                    # Randomly do an action (look, get all, save, shout)
                    if random.random() < 0.3:  # 30% chance
                        self.do_random_action()
                    
                    # Print status every 10 moves
                    if self.command_count % 10 == 0:
                        elapsed = int(time.time() - start_time)
                        exits_str = ', '.join(available_exits) if available_exits else "none"
                        print(f"[{self.name}] ({elapsed}s) moves: {self.command_count} | [exits: {exits_str}]")
                
                # FAST: Minimal wait before next move (0.5 seconds)
                time.sleep(0.5)
                
            except socket.timeout:
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
    print("ESP32 MUD - INTENSE Multi-Player Stress Test")
    print("="*60)
    print(f"Target: {ESP32_IP}:{MUD_PORT}")
    print(f"Players: {len(PLAYERS)} (playera - playerg)")
    print(f"Duration: {EXPLORE_DURATION} seconds (3 hours)")
    print(f"Features:")
    print(f"  - Fast movement (0.5s between moves)")
    print(f"  - Random: look, get all, save, shout")
    print(f"  - Auto get all on gold coins")
    print(f"  - Emote interactions with other players")
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
    
    # Create player threads
    print(f"\nSpawning {len(PLAYERS)} players...")
    threads = []
    players_created = []
    for suffix in PLAYERS:
        player = MUDPlayer(suffix)
        players_created.append(player)
        thread = threading.Thread(target=run_player, args=(player,), daemon=False)
        thread.start()
        threads.append(thread)
        time.sleep(0.5)  # Stagger logins
    
    # Wait for all threads
    print("\nWaiting for all players to finish...")
    for thread in threads:
        thread.join()
    
    # Count successful logins
    successful_logins = sum(1 for p in players_created if p.connected)
    total_commands = sum(p.command_count for p in players_created)
    
    print("\n" + "="*60)
    print("✓ INTENSE Multi-player exploration complete!")
    print(f"  Players logged in: {successful_logins}/{len(PLAYERS)}")
    print(f"  Total commands executed: {total_commands}")
    print("="*60 + "\n")

if __name__ == "__main__":
    main()
