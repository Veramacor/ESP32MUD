# ESP32 MUD Multi-Player Test Scripts

Two Python scripts for testing the MUD with multiple simultaneous players.

## Prerequisites

- Python 3.6+
- Network access to ESP32 (same WiFi or LAN)
- ESP32 MUD running and listening

## Setup

1. Find your ESP32's IP address:
   - Check Serial output during boot: "IP: 192.168.1.X"
   - Or use: `ipconfig` and look for your ESP32

2. Update both scripts with your ESP32 IP and port:
   ```python
   ESP32_IP = "192.168.1.100"  # Change this to your IP
   MUD_PORT = 2000             # Change this if using different port
   ```

## Step 1: Create Players

Run the player creation script first:

```bash
python create_players.py
```

This will:
- Create 7 new players: playera, playerb, playerc, ... playerg
- Set each player's password to match their username
- Assign all to Human race
- Start them at the Church
- Disconnect after creation

**Expected output:**
```
Creating playera...
✓ Connected to 192.168.1.100:2000
✓ Got name prompt
✓ Got password prompt (new character)
✓ Got race selection prompt
✓ Character created successfully!
✓ Disconnected from playera
...
```

## Step 2: Login and Explore

Once all 7 players are created, run the exploration script:

```bash
python login_and_explore.py
```

This will:
- Log in all 7 players simultaneously
- Keep them connected for 5 minutes (300 seconds)
- Every 3 seconds, each player randomly chooses a direction:
  - north
  - south
  - east
  - west
- Players will explore the entire map, bumping into walls and discovering rooms
- Send periodic status messages to console
- Gracefully disconnect all players

**Expected output:**
```
Logging in 7 players...

[playera] ✓ Connected to server
[playera] → Sent name
[playera] → Sent password
[playera] ✓ Logged in successfully!
...

Successfully logged in: 7/7 players

Starting exploration for 7 players...

[playera] ✓ Starting exploration for 300 seconds
[playerb] ✓ Starting exploration for 300 seconds
...

[playera] (5s) Moved north → Tavern
[playerb] (5s) Moved south → Library
...

[playera] ✓ Exploration complete (300s, 100 moves)
...

EXPLORATION SUMMARY
Total moves across all players: 700
```

## Troubleshooting

### Connection Refused
```
✗ Connection refused! Is ESP32 running at 192.168.1.100:2000?
```
**Solution:**
- Check ESP32 is powered on
- Check MUD is running (look at Serial monitor)
- Verify correct IP address
- Verify correct port number
- Check WiFi connection

### Timeout Errors
```
✗ Connection timeout for playera
```
**Solution:**
- ESP32 may be overloaded - reduce number of simultaneous connections
- Check network latency
- Make sure ESP32 isn't too far from WiFi router

### "Unexpected response" Errors
```
⚠ Unexpected welcome response
```
**Solution:**
- Protocol may have changed in MUD code
- Check actual login sequence against script
- Review Serial output for unusual responses

## What to Watch For

During exploration:
1. **Players should move around and discover different rooms**
2. **Occasional "You can't go that way" is normal** (hitting walls)
3. **All 7 players should show activity** in the output
4. **Look for NPC interactions** - if any NPCs are in rooms
5. **Check game logic** - do items drop? Do NPCs spawn?

## Example Full Run

```bash
# Terminal 1: Monitor ESP32 Serial output
# (Leave open to see what the MUD is doing)

# Terminal 2: Create players
python create_players.py

# Wait for it to complete...

# Terminal 3: Login and explore
python login_and_explore.py

# Watch both terminal 2 and the Serial output
# Should see lots of activity from 7 simultaneous players
```

## Customization

Edit the scripts to change:

**create_players.py:**
- `PLAYERS = ['a', 'b', 'c', 'd', 'e', 'f', 'g']` - Add/remove players
- `RACE = '0'` - Change starting race (0=Human, 1=Elf, 2=Dwarf, 3=Orc, 4=Halfling)

**login_and_explore.py:**
- `EXPLORE_DURATION = 300` - Change exploration time (seconds)
- `DIRECTIONS = ['north', 'south', 'east', 'west']` - Add/remove directions
- `time.sleep(3)` - Change time between moves

## Advanced: Monitor Activity

To see what's happening on the MUD side, check Serial output while scripts run:

```
[PLAYER] playera logged in
[PLAYER] playerb logged in
...
[COMBAT] Combat check...
[NPC] NPC respawned...
```

Good luck exploring! 🗺️
