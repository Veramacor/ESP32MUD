# ESP32MUD - File Index & Data Format Reference

## Source Code Files

### src/ESP32MUD.cpp
**Size**: ~11,400 lines  
**Purpose**: Complete MUD game engine  
**Language**: Arduino C++ (ESP32 framework)  
**Key sections**:
- Global constants and configuration (lines 1-100)
- Struct definitions (Shop, Player, NPC, Item, etc.)
- Core game functions (cmdLook, cmdMove, cmdAttack, etc.)
- Shop system (initializeShops, handleBuy, handleSell)
- Combat engine (executeCombat, calculateToHit)
- NPC management (respawnNPCs, loadNPCDefinitions)
- Quest system (questEvent, completeQuest)
- Session management (handleLogin, handleLogout, duplicate detection)
- Session logging (logSessionLogin, logSessionLogout)
- QR code generation (cmdQrCode with player targeting)
- Time synchronization (syncTimeFromNTP)
- WiFi provisioning (handleProvisioning)
- Persistence (loadPlayerData, savePlayerData)

### src/GetSpawnRoom.txt
**Size**: ~1 line  
**Format**: `x y z` (space-separated integers)  
**Purpose**: Default spawn coordinates for new players  
**Example**: `253 244 49`

### include/YmodemBootloader.h
**Purpose**: Binary file upload handler  
**Used for**: Firmware updates and data file uploads via serial  
**Protocol**: Ymodem (send via serial terminal)

### include/version.h
**Auto-generated at build time**  
**Contains**: Version number, build timestamp  
**Used by**: Firmware identification

### scripts/version_generator.py
**Purpose**: Pre-build script that generates version.h  
**Triggers**: Before each compilation  
**Output**: include/version.h with timestamp and version

## Data Files (LittleFS)

These files live on the device's LittleFS filesystem. Upload them using `pio run --target uploadfs`.

### data/rooms.txt
**Format**: 
```
x|y|z|description|exit_list
```
**Example**:
```
250|244|50|You stand in a bustling town square. Merchants hawk their wares.|N:251,244,50|S:249,244,50|E:251,244,50|W:249,244,50
```
**Fields**:
- `x, y, z` - 3D voxel coordinates (unlimited range)
- `description` - Room text (up to 256 chars, supports word wrapping)
- `exit_list` - Pipe-separated exits (N, S, E, W, NE, NW, SE, SW, U, D)
- Exit format: `DIRECTION:x,y,z` (colon, then comma-separated coords)

### data/items.vxd (Item Definitions)
**Format**:
```
itemid|type|name=Name|desc=Description|weight=0|ac=0|damage=1d4|can_contain=0|stackable=1|value=10|other_attr=value
```

**Example**:
```
torch|misc|name=Torch|type=misc|desc=A wooden torch that glows with orange flame|weight=2|can_contain=0|stackable=1|value=5
skinning_knife|weapon|name=Knife|type=weapon|desc=A keen-edged skinning knife|weight=1|damage=1d6|ac=0|stackable=0|value=15
```

**Standard attributes**:
- `itemid` - Unique identifier (no spaces, lowercase)
- `type` - Category: weapon, armor, misc, consumable, container
- `name` - Display name used in rooms and inventory
- `desc` - Description (optional)
- `weight` - Weight units (integers)
- `ac` - Armor class bonus (armor only; negative = better)
- `damage` - Damage dice (weapons only; e.g., 1d6, 2d4+1)
- `can_contain` - Capacity if it's a container (0 = not a container)
- `stackable` - 1 = can stack, 0 = single items only
- `value` - Resale value in gold coins

**Custom attributes**: Any additional `key=value` pairs are stored and accessible to NPCs/quests.

### data/items.vxi (Item Instances)
**Format**:
```
x|y|z|itemid|quantity|condition|container_parent_itemid
```

**Example**:
```
250|244|50|torch|3|good|none
250|245|50|skinning_knife|1|good|none
```

**Fields**:
- `x, y, z` - Location in 3D world
- `itemid` - References items.vxd
- `quantity` - Number of items (used for stackable items)
- `condition` - Quality level: excellent, good, worn, damaged, broken
- `container_parent_itemid` - If inside another item, its ID (or "none")

### data/npcs.vxd (NPC Definitions)
**Format**:
```
npcid|name|hp=20|damage=1d4|ac=5|aggression=1|exp=10|gold=5|respawn=600|loot=itemid|dialog_combat=Taunt text
```

**Example**:
```
goblin|Goblin Warrior|hp=12|damage=1d6|ac=6|aggression=2|exp=15|gold=3|respawn=600|loot=dagger|dialog_combat=Arrgh!
```

**Standard fields**:
- `npcid` - Unique identifier
- `name` - Display name
- `hp` - Maximum hit points
- `damage` - Damage dice (e.g., 1d4)
- `ac` - Armor class (higher = worse)
- `aggression` - 1=passive, 2=defensive, 3=aggressive
- `exp` - Experience reward on kill
- `gold` - Gold dropped on death
- `respawn` - Seconds until respawn (600 = 10 minutes)
- `loot` - Optional itemid dropped on death
- `dialog_combat` - Combat taunt text

### data/npcs.vxi (NPC Instances)
**Format**:
```
x|y|z|npcid|hp|last_spawn_time
```

**Example**:
```
252|244|50|goblin|12|1705785000
253|244|50|goblin|8|1705785120
```

**Fields**:
- `x, y, z` - Location in world
- `npcid` - References npcs.vxd
- `hp` - Current hit points
- `last_spawn_time` - Unix timestamp of last spawn (for respawn calculation)

### data/quests.txt
**Format**:
```
questid|name|step1_description|step1_trigger_type:trigger_value|step1_reward_xp:amount|step2_description|step2_trigger_type:trigger_value|step2_reward_xp:amount|...
```

**Trigger types**:
- `kill:npcid` - Kill specific NPC
- `location:x,y,z` - Reach specific location
- `collect:itemid` - Collect item
- `deliver:itemid:npcid` - Deliver item to NPC

**Example**:
```
goblin_slayer|Slay the Goblin|Kill the goblin guarding the bridge|kill:goblin|xp:50|gold:20|Report back to the guard captain|location:250,244,50|xp:25|gold:10
```

### data/credentials.txt
**Format**:
```
SSID
PASSWORD
```

**Example**:
```
MyWiFiNetwork
SecurePassword123
```

**Purpose**: WiFi credentials  
**Created**: During WiFi provisioning (first boot via serial)  
**Security note**: Stored in plaintext; restrict LittleFS access

### data/session_log.txt
**Auto-generated** on first login  
**Format**:
```
YYYY-MM-DD HH:MM:SS | LOGIN | PlayerName
YYYY-MM-DD HH:MM:SS | LOGOUT | PlayerName
```

**Example**:
```
2026-01-20 14:32:15 | LOGIN | Atew
2026-01-20 14:35:42 | LOGOUT | Atew
2026-01-20 14:35:45 | LOGIN | Ploi
2026-01-20 14:40:10 | LOGIN | Atew
```

**Features**:
- Appended on each login/logout event
- Used for audit trails
- Timestamps from NTP (or device RTC if synced)
- Viewable via `debug sessions` command (last 50 entries, descending)

### data/player_*.txt
**Format**:
```
Name|Race|Level|XP|HP|MaxHP|AC|Damage|LastX|LastY|LastZ|Equipped:slot=itemid|Inventory:itemid:qty|...
```

**Example**:
```
Atew|Human|5|2500|18|25|10|1d6+2|250|244|50|Head=none|Torso=chainmail|Legs=leather_leggings|Inventory:sword:1|Inventory:torch:2|Inventory:gold:150
```

**Sections**:
1. **Core stats** - Name, race, level, experience, health
2. **Combat stats** - AC (armor class), damage dice
3. **Last location** - X, Y, Z coordinates for respawn
4. **Equipment** - 11 slots (head, torso, hands, legs, feet, arms, L/R hand, neck, belt, back)
5. **Inventory** - Items with quantities (itemid:qty format)

**Naming convention**: `player_NAME.txt` (lowercase, no spaces)

## Configuration Files

### platformio.ini
**Contains**: Build configuration, board settings, library dependencies, environment definitions  
**Key sections**:
- `[env:seeed_xiao_esp32c3]` - Board-specific settings
- Monitor port and baud rate (115200)
- Build flags and optimization settings
- Library version specifications

### .gitignore
**Ignored files**: LittleFS data, compiled binaries, build artifacts

## File Locations on Device

```
LittleFS (~/):
  rooms.txt            ← World map
  items.vxd            ← Item definitions
  items.vxi            ← Item instances
  npcs.vxd             ← NPC definitions
  npcs.vxi             ← NPC instances
  quests.txt           ← Quest definitions
  credentials.txt      ← WiFi SSID/password
  session_log.txt      ← Login/logout audit (auto-generated)
  player_atew.txt      ← Player save file (auto-generated)
  player_ploi.txt      ← Player save file (auto-generated)
  ...
```

## Estimated File Sizes

| File | Typical Size | Limit |
|------|-------------|-------|
| rooms.txt | 10-50 KB | 500 KB (5000 rooms max) |
| items.vxd | 5-10 KB | 100 KB (500+ items) |
| items.vxi | 2-5 KB | 50 KB (2000+ instances) |
| npcs.vxd | 2-5 KB | 50 KB (100+ NPCs) |
| npcs.vxi | 2-5 KB | 50 KB (50 living NPCs) |
| quests.txt | 1-3 KB | 30 KB (20+ quests) |
| credentials.txt | <1 KB | Fixed |
| session_log.txt | Grows with usage | ~50 KB (1000+ entries) |
| player_*.txt | 0.5-2 KB each | 10 KB each (32 items max) |
| **Total** | **~30-100 KB** | **~1.8 MB usable** |

## Upload/Download via Serial

### Uploading Data Files
```bash
# Upload filesystem
pio run --target uploadfs --environment seeed_xiao_esp32c3

# This syncs entire data/ folder to device LittleFS
```

### Uploading Individual Files (Manual)
Use Ymodem protocol via serial terminal:
1. Open serial monitor: `pio device monitor --environment seeed_xiao_esp32c3`
2. Send file via Ymodem (terminal-specific)
3. Device receives and stores to LittleFS

## Data Persistence Strategy

- **Per-player files** (`player_*.txt`) - Loaded on login, saved on logout
- **World state files** - Loaded at startup, saved at specific intervals or on shutdown
- **Item instances** - Persisted in items.vxi
- **NPC state** - Current HP stored in npcs.vxi, respawn timers calculated from timestamps
- **Quest state** - Stored in player files (per-player progress)
- **Sessions** - Logged to session_log.txt, not persisted across reboots (one-way audit log)

---

**Last Updated**: January 20, 2026  
**ESP32MUD Version**: v1.1.0
