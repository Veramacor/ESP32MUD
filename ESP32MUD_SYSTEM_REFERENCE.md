# ESP32MUD System Reference Guide

**Last Updated:** January 18, 2026  
**Version:** 1.0.0  
**Platform:** Seeed XIAO ESP32C3 (WiFi-enabled embedded MUD server)

---

## Table of Contents

1. [Overview](#overview)
2. [Core Systems](#core-systems)
3. [Game World Structure](#game-world-structure)
4. [Player System](#player-system)
5. [Item System](#item-system)
6. [Combat System](#combat-system)
7. [Quest System](#quest-system)
8. [NPC System](#npc-system)
9. [Command Reference](#command-reference)
10. [Wizard Commands](#wizard-commands)
11. [Data Structures](#data-structures)
12. [File Formats](#file-formats)
13. [Game Mechanics](#game-mechanics)

---

## Overview

**ESP32MUD** is a **telnet-based Multi-User Dungeon (MUD)** game server running on the Seeed XIAO ESP32C3 microcontroller. It's a complete fantasy RPG with combat, quests, NPCs, items, equipment, shops, and a voxel-based 3D world system.

### Key Features

- **Telnet-based multiplayer** (up to 10 concurrent players)
- **Voxel world system** with 3D coordinates (X, Y, Z) and 10-directional movement (N, S, E, W, NE, NW, SE, SW, U, D)
- **Combat engine** with real-time combat rounds, weapon damage, armor absorption, and NPC counter-attacks
- **Item system** with parent/child relationships (containers), equipment slots, and shops
- **Quest system** with multi-step quests, NPC interactions, and rewards
- **NPC system** with dialog, aggression, respawning, and loot drops
- **Equipment management** with 11 body slots and stat bonuses
- **Experience/leveling** with 20 levels per character and race-specific titles
- **File-based persistence** using LittleFS for world state, player saves, and item definitions
- **Wizard mode** with admin commands for world management, teleportation, and debugging

### Technical Specifications

- **Hardware:** Seeed XIAO ESP32C3
- **RAM:** 327,680 bytes (uses ~16% for game state)
- **Flash:** 4 MB embedded (2 MB app partition, 2 MB SPIFFS)
- **WiFi:** 802.11b/g/n @ 2.4GHz
- **Serial:** USB-CDC for provisioning and debug output
- **Telnet Port:** 23 (configurable)
- **Max Players:** 10
- **Max NPCs:** 50
- **Inventory Size:** 32 items per player
- **Auto-reboot:** 6-hour cycle with player warnings at 5min, 2min, 1min, 30sec, 5sec

---

## Core Systems

### 1. Startup & Initialization

The system initializes in this order:

1. **Serial & WiFi Setup** - Initialize serial monitor and WiFi provisioning
2. **LittleFS Mounting** - Mount SPIFFS file system
3. **WiFi Credentials** - Load from `/credentials.txt` or enter provisioning mode
4. **Data Loading** - Load definitions and world state:
   - `/items.vxd` - Item definitions
   - `/npcs.vxd` - NPC definitions
   - `/rooms.txt` - World voxel map (3D coordinates)
   - `/items.vxi` - World item instances
   - `/npcs.vxi` - NPC instances
   - `/quests.txt` - Quest definitions
5. **Server Start** - Begin listening on telnet port 23
6. **Global Reboot Timer** - Initialize 6-hour reboot cycle

### 2. Global Reboot System

- **Interval:** 6 hours (configurable to 2 minutes for testing)
- **Warnings:** Posted at 5 minutes, 2 minutes, 1 minute, 30 seconds, and 5 seconds before reboot
- **Behavior:** Server restarts via `esp_restart()`, all players disconnected
- **Persistence:** All player data and world state saved to files before restart

### 3. Connection Management

- **Max Players:** 10 slots
- **Telnet Protocol:** Standard telnet over WiFiClient
- **Input Buffer:** Reads until `\n`, removes trailing `\r`
- **Active Check:** Player marked inactive if connection closes

### 4. Player Login Flow

1. **New Connection** → Find empty slot in `players[]` array
2. **Account Prompt** → Ask for username
3. **Existing Player?** → Load saved data from `/players/[name].txt`
4. **Password** → Prompt and verify
5. **New Player Creation** → Initialize stats, race selection, spawn location
6. **Welcome Messages** → Display MUD welcome and room description
7. **Player Ready** → `active=true`, `loggedIn=true`

---

## Game World Structure

### Voxel Coordinate System

The world uses a **3D voxel coordinate system** with:
- **X, Y, Z** - Integer coordinates
- **Range:** Limited by 21-bit encoding (can support ±2 million)
- **Room Definition:** Each unique (X, Y, Z) tuple maps to one `Room` struct

### Room Definition

```cpp
struct Room {
    int x, y, z;                        // Voxel coordinates
    char name[32];                      // "Town Square", "Forest Clearing", etc.
    char description[512];              // Multi-line room description
    int exit_n, exit_s, exit_e, exit_w; // 1 = passage exists, 0 = blocked
    int exit_ne, exit_nw, exit_se, exit_sw;
    int exit_u, exit_d;                 // Up/Down exits
    String exitList;                    // "Exits: north, south, east, up"
    
    // Portal system
    bool hasPortal;                     // Can player use a portal here?
    int px, py, pz;                     // Destination coordinates
    char portalCommand[32];             // "enter", "climb", etc. (command to use portal)
    char portalText[128];               // Flavor text when using portal
};
```

### World File Format

**File:** `/rooms.txt`  
**Format:** Space-separated values

```
x y z name description exits_string portal_data
```

Example:
```
0 0 0 Town_Square A bustling town square n s e w u d 0 0 0 0
1 0 0 Forest A dark forest s w 0 0 0 0
```

### Room Loading & Caching

- **On demand:** Rooms loaded only when player moves
- **Binary Index:** Optional `/rooms.idx` for fast voxel lookup
- **Player Room Cache:** Each player maintains current `Room` struct

### 10-Directional Movement

| Command | Direction | Coordinates |
|---------|-----------|------------|
| n / north | North | (0, -1, 0) |
| s / south | South | (0, +1, 0) |
| e / east | East | (+1, 0, 0) |
| w / west | West | (-1, 0, 0) |
| ne / northeast | NE | (+1, -1, 0) |
| nw / northwest | NW | (-1, -1, 0) |
| se / southeast | SE | (+1, +1, 0) |
| sw / southwest | SW | (-1, +1, 0) |
| u / up | Up | (0, 0, +1) |
| d / down | Down | (0, 0, -1) |

---

## Player System

### Player Structure

```cpp
struct Player {
    // Connection & Auth
    WiFiClient client;                  // Telnet connection
    bool active, loggedIn, IsWizard;
    char name[32];                      // Player name (lowercase stored, display capitalized)
    char password[32];
    
    // Stats & Progression
    int raceId;                         // 0-4 (Human, Elf, Dwarf, Orc, Halfling)
    int hp, maxHp;                      // Hit points
    int coins;                          // Gold currency
    int xp;                             // Experience points
    int level;                          // 1-20
    int questsCompleted;                // Counter
    
    // Combat & Defense
    int attack;                         // Base damage (1)
    int weaponBonus;                    // From wielded weapon
    int armorBonus;                     // From worn armor
    int baseDefense;                    // Base AC (9)
    bool wimpMode;                      // Auto-flee when hp <= 5?
    
    // Location
    Room currentRoom;                   // Current room data (cached)
    int roomX, roomY, roomZ;            // Current voxel coordinates
    
    // Inventory (indices into worldItems array)
    int invIndices[32];                 // Array of world item indices
    int invCount;                       // Number of items carried
    
    // Equipment (indices into worldItems array)
    int wieldedItemIndex;               // Currently wielded weapon (-1 = empty)
    int wornItemIndices[SLOT_COUNT];    // Armor by slot (-1 = empty)
    
    // Quest Tracking
    bool questStepDone[10][10];         // [questId-1][stepId-1]
    bool questCompleted[10];
    
    // Combat State
    bool inCombat;                      // Currently in combat?
    unsigned long nextCombatTime;       // Earliest next attack (3000 ms cooldown)
    NpcInstance* combatTarget;          // Pointer to current NPC opponent
    
    // Wizard Features
    bool IsInvisible;                   // Invisible to other players?
    bool showStats;                     // Detailed combat stats display
    DebugDestination debugDest;         // Serial or Telnet debug output
    String EnterMsg, ExitMsg;           // Custom arrival/departure messages
    
    // Timers
    unsigned long lastHealCheck;        // Passive healing timer
};
```

### Character Races

Five races with unique title progressions:

1. **Human** (raceId=0) - Balanced
2. **Elf** (raceId=1) - Magical heritage
3. **Dwarf** (raceId=2) - Underground heritage
4. **Orc** (raceId=3) - Warrior culture
5. **Halfling** (raceId=4) - Nimble scouts

Each race has 20 unique titles per level (e.g., Human Level 1 = "Utter Novice", Human Level 20 = "High King")

### Leveling System

| Level | XP Required |
|-------|------------|
| 1 | 0 |
| 2 | 100 |
| 3 | 300 |
| 4 | 600 |
| 5 | 1000 |
| ... | *Quadratic progression* |
| 20 | 38000 |

**Formula:** `XP_for_level = level² * 100`

**Stats Gained Per Level:**
- HP: +2
- Base Damage: +1
- Carry Capacity: +5 items

### Healing System

- **Passive healing:** 1 HP per 30 seconds during inactivity
- **Manual healing:** Wizard `heal` command restores to full
- **Death healing:** At 0 HP, removed from combat but not kicked off server
- **Wizard immortality:** Wizards cannot reach 0 HP (capped at 1)

### Player Persistence

**File:** `/players/[name].txt`

Saved automatically on logout, contains:
- Race ID, level, XP, coins
- Current position (X, Y, Z)
- HP, max HP
- Quest progress (bitmasks for steps and completions)
- Title
- Wizard status and custom messages

---

## Item System

### Core Item Model

Items use a **parent/child hierarchy** with **ownership tracking**:

```cpp
struct WorldItem {
    int x, y, z;                                    // World coordinates (-1 = no location)
    String name;                                    // Item ID (e.g., "sword_iron", "gold_coin")
    String ownerName;                              // "" = world item, "PlayerName" = inventory
    String parentName;                             // "" = top-level, "ItemID" = inside container
    int value;                                     // Gold value for trading
    std::map<std::string, std::string> attributes; // Instance overrides (name, desc, armor, damage, etc.)
    std::vector<int> children;                     // Indices of child items (for containers)
};
```

### Item Ownership Rules

| ownerName | parentName | Location |
|-----------|-----------|----------|
| "" | "" | World floor (at X, Y, Z) |
| "" | "ItemID" | Inside another item (container) |
| "PlayerName" | "" | In player's inventory |
| "PlayerName" | "ItemID" | In container inside player's inventory |

### Item Definition System

**File:** `/items.vxd` (Item definitions)

Each item has a template in `itemDefs` map with:
- **Type:** "weapon", "armor", "food", "drink", "sign", "container", etc.
- **Attributes:** name, description, damage, armor, value, weight, etc.

```
sword_iron weapon name=Iron Sword damage=3 value=50 weight=2
```

### Item Attributes

Common attributes:

| Attribute | Type | Example | Purpose |
|-----------|------|---------|---------|
| name | string | "Iron Sword" | Display name |
| description | string | "A sharp blade..." | Look command text |
| type | enum | "weapon" | Category |
| damage | int | 3 | Weapon damage bonus |
| armor | int | 2 | Defense bonus |
| value | int | 50 | Gold price |
| weight | int | 2 | Carry weight |
| mobile | bool | 1 | Can be picked up? |
| container | bool | 1 | Can hold items? |
| wearable | string | "head" | Body slot (head, chest, legs, etc.) |
| wieldable | bool | 1 | Can be wielded as weapon? |
| food | bool | 1 | Can be eaten? |
| consumable | bool | 1 | Disappears when used? |

### Item Resolution

Items can be referenced by:
1. **Item ID** (exact match): "sword_iron"
2. **Display name** (partial match): "Iron Sword" or "iron"
3. **Full-word match:** Breaking display name by spaces and checking each word

**Resolution Priority:**
1. Inventory (highest priority)
2. Worn items
3. Room floor items
4. Container children (if searching inside a container)

### Equipment Slots

```cpp
enum BodySlot {
    SLOT_HEAD = 0,      // Helmets, crowns
    SLOT_CHEST = 1,     // Breastplate, armor
    SLOT_CHEST2 = 2,    // Chainmail, underarmor
    SLOT_LEGS = 3,
    SLOT_FEET = 4,
    SLOT_HANDS = 5,
    SLOT_ARMS = 6,
    SLOT_BACK = 7,      // Cloaks, capes
    SLOT_WAIST = 8,     // Belts
    SLOT_NECK = 9,      // Amulets
    SLOT_FINGER = 10,   // Rings
    SLOT_COUNT = 11     // Total slots
};
```

### Equipment Bonuses

When an item is **wielded** or **worn**:
- **Damage bonus** from wielded weapon added to `weaponBonus`
- **Armor bonus** from worn items added to `armorBonus`
- Bonuses recalculated on every equip/unequip

**Equipment Command Examples:**
```
wield sword_iron       → Equip weapon
unwield                → Unequip weapon
wear armor_plate       → Equip armor to appropriate slot
remove armor_plate     → Unequip armor
```

### Containers

Items can hold other items if they have `container="1"` attribute:

```
GET item FROM container    → Remove from container to inventory
PUT item IN container      → Move from inventory to container
GET ALL FROM container     → Empty all items from container
```

### Coin System

**Special item:** `gold_coin` (stackable)

- Display name: "X gold coins" (pluralized if > 1)
- Cannot be dropped individually (merge into piles)
- Merged when dropped: `mergeCoinPilesAt(x, y, z)`
- Spawned with `spawnGoldAt(x, y, z, amount)`

### Shop System

Shops are implemented as **containers** with special "sign" items:

**File:** Shop sign in world (e.g., "bakery_sign" with children items)

**Shop Types:**
- `signtype="shop"` - Buys/sells general goods
- `signtype="blacksmith"` - Buys/sells weapons and armor
- `signtype="magicshop"` - Only magic items

**Commands:**
```
buy sword              → Purchase from shop (must be in same room as sign)
sell armor             → Sell item to shop
                       → Shops buy at 50% of item value
```

**Shop Behavior:**
- Restocked every 10 minutes to max 3 items per type
- Items cloned when purchased (original remains)
- Player must have enough coins

---

## Combat System

### Combat Overview

ESP32MUD uses a **turn-based combat system** with:
- **3-second combat rounds** between attacks
- **To-hit roll** against player AC (armor class)
- **Damage roll** with armor absorption
- **NPC counter-attacks** with separate mechanics
- **Dialog during combat** from NPCs
- **Auto-flee** when player HP drops below 5 (if wimp mode enabled)

### Combat Initiation

```
KILL npc_name          → Start combat with NPC
```

**What Happens:**
1. Player targets the NPC
2. NPC becomes hostile to player (if not already)
3. Combat sets `inCombat=true`, `combatTarget=NPC*`
4. `nextCombatTime` set to 3 seconds in future
5. NPC marks player as `targetPlayer[playerIndex]`

### Combat Round (THE COMBAT ENGINE)

**Function:** `doCombatRound(Player &p, int playerIndex)`

Called when:
- Player in combat AND 3+ seconds have passed since last attack
- Triggered by any command while in combat

**Round Flow:**

1. **Validity Check**
   - Is NPC still alive?
   - Is NPC still in same room?
   - Is player still in same room?
   - If any false → combat ends

2. **Player Attack**
   - Get player total damage: `playerDamage = attack + weaponBonus`
   - Roll to-hit: `rollToHit(playerDamage, npcDefense)`
   - If hit:
     - Roll damage: `damageRoll = random(1, playerDamage+1)`
     - Calculate NPC absorption
     - Apply damage to NPC HP
     - Broadcast attack message
     - Check NPC death
   - If miss:
     - Broadcast miss message

3. **NPC Counter-Attack** (if still alive and targeting player)
   - Get NPC total damage from definition
   - Roll to-hit against player AC
   - If hit:
     - Roll NPC damage
     - Player armor absorbs 0-armorBonus damage
     - Apply final damage to player
     - Broadcast counter-attack
   - If miss:
     - Broadcast NPC miss

4. **NPC Dialog** (if aggressive)
   - Every 3rd round of combat, random NPC dialog
   - Dialog rotates through 3 stored dialog lines

5. **Status Checks**
   - If NPC dead: handle death (gold drop, XP, respawn)
   - If player dead: handle player death
   - If player HP ≤ 5 and wimp mode: auto-flee

6. **Set Cooldown**
   - `nextCombatTime = millis() + 3000` (3-second round)

### To-Hit Mechanics

```cpp
bool rollToHit(int attackBonus, int defense) {
    int roll = random(1, 21);  // d20
    return (roll + attackBonus) > defense;
}
```

**Base defense:** 9 (DC 10 means roll 2+ with +0 bonus)  
**Armor bonus:** Reduces enemy to-hit (increases AC)  
**Attack bonus:** From weapon damage stat

### Damage Calculation

```
Player attacks:
  damageRoll = random(1, playerDamage + 1)
  finalDamage = damageRoll - npc_armor_absorption
  
NPC attacks:
  damageRoll = random(1, npcBaseDamage + 1)
  armorAbsorb = random(0, playerArmor + 1)
  finalDamage = damageRoll - armorAbsorb (minimum 1)
```

### Combat Status Display

For **Wizard players with `showStats=true`**, detailed combat info:
- Base armor and damage
- Armor absorption amount
- To-hit bonuses
- Final damage dealt
- NPC stats

### NPC Aggression

NPCs have an `aggressive` attribute:

```
- aggressive=1 → NPC initiates combat with players in room
- aggressive=0 → NPC only attacks if attacked first
```

NPCs check for nearby players periodically with `checkNPCAggro()`.

### Wimp Mode

```
WIMP          → Toggle wimp mode
              → Enabled: auto-flee at HP ≤ 5
              → Disabled: fight to death
```

When wimp mode triggers:
1. Player flees in random direction
2. Message: "You flee in panic!"
3. Combat ends
4. Move to adjacent room

### Player Death

When player HP reaches 0:

```
handlePlayerDeath(Player &p)
  - Mark all NPCs as not hostile to player
  - Remove player from all NPC target lists
  - End combat
  - Display death message
  - Player stays connected, still in room
  - Can interact but cannot attack
  - Wizard death protection: Cap at 1 HP
```

### NPC Death

When NPC HP reaches 0:

```
- Broadcast death message (random from death message pool)
- Award XP to player (from NPC definition)
- Drop gold (from NPC definition or gold attribute)
- Mark NPC as dead (`alive=false`)
- Schedule respawn (`respawnTime = now + 30 minutes`)
- End player combat
- Trigger quest events ("kill" task completion)
- Unload NPC from quest if specified
```

---

## Quest System

### Quest Overview

Quests are **multi-step objectives** that track progress and award rewards:
- **10 quest slots** (questId 1-10)
- **Up to 10 steps** per quest
- **Step tracking:** Player tracks which steps completed
- **Completion tracking:** Player tracks quest completion
- **Rewards:** XP and gold upon completion

### Quest Structure

```cpp
struct QuestDef {
    int questId;                           // 1-10
    String name;                           // "Defeat the Goblin King"
    String description;                    // Long description text
    String difficulty;                     // "Easy", "Medium", "Hard"
    String completionDialog;               // Message when quest done
    String unloadNpcId;                    // NPC to remove when done (optional)
    int rewardXp;                          // XP awarded on completion
    int rewardGold;                        // Gold awarded on completion
    std::vector<QuestStep> steps;         // Multi-step objectives
};

struct QuestStep {
    int questId, step;                     // 1-10 range
    String task;                           // Type: "give", "kill", "say", "reach", "pickup"
    String item;                           // For give/pickup/drop/use tasks
    String target;                         // NPC id for give/kill/escort
    String phrase;                         // For "say" task
    int targetX, targetY, targetZ;        // For reach/location tasks
};
```

### Step Task Types

| Task | Parameters | Description |
|------|-----------|-------------|
| give | item, target | Give item to NPC |
| kill | target | Defeat NPC |
| say | phrase, targetX/Y/Z | Say phrase in room |
| reach | targetX/Y/Z | Travel to location |
| pickup | item, targetX/Y/Z | Pick up item in location |
| drop | item, targetX/Y/Z | Drop item in location |
| use | item, targetX/Y/Z | Use item in location |

### Quest Tracking

```cpp
bool questStepDone[10][10];     // [questId-1][stepId-1]
bool questCompleted[10];        // [questId-1]
```

### Quest Event Handler

```cpp
onQuestEvent(Player &p, 
             const String &task, 
             const String &param1,
             const String &npc_or_item,
             const String &phrase,
             int x, int y, int z)
```

Called when:
- Player kills NPC
- Player gives item to NPC
- Player reaches location
- Player picks up item
- etc.

**What it does:**
1. Search all 10 quests
2. Find matching step
3. Mark step as done
4. Check if all steps done
5. If complete → Award XP and gold, remove NPC if needed

### Quest Commands

```
QUEST LIST        → Show available quests and status
QUEST 1           → Detailed info on quest 1
QUEST INFO <quest> → (Wizard) Show step info
```

### Quest File Format

**File:** `/quests.txt`

```
quest_id quest_name description difficulty xp_reward gold_reward unload_npc
step_num task_type target item phrase target_x target_y target_z
step_num task_type target item phrase target_x target_y target_z
...
(blank line between quests)
```

---

## NPC System

### NPC Overview

NPCs (Non-Player Characters) are:
- **Spawned from definitions** loaded at startup
- **Instances placed in world** at specific coordinates
- **Dynamic** - Can move, fight, respawn, hold items
- **Dialogs** - Up to 3 random dialog lines per NPC
- **Aggression** - Some are hostile, some friendly
- **Loot** - Drop gold and trigger quest events on death

### NPC Definition

**File:** `/npcs.vxd`

```cpp
struct NpcDefinition {
    std::string type;                      // "goblin", "dragon", "merchant"
    std::map<std::string, std::string> attributes; // name, hp, damage, gold, etc.
};
```

Example definition:
```
goblin_warrior monster name=Goblin Warrior hp=10 damage=2 gold=5 aggressive=1 xp=25 dialog_1=You dare challenge me? dialog_2=Prepare to die! dialog_3=Taste my blade!
```

### NPC Instance

**File:** `/npcs.vxi` (NPC world placement)

```cpp
struct NpcInstance {
    int x, y, z;                    // Current position
    int spawnX, spawnY, spawnZ;     // Original spawn point
    String npcId;                   // Reference to definition (e.g., "goblin_warrior")
    int hp;                         // Current HP
    int gold;                       // Loot gold
    bool alive;                     // Currently alive?
    unsigned long respawnTime = 0;  // Scheduled respawn time
    
    // Combat
    int targetPlayer;               // Index of player being fought (-1 = none)
    bool hostileTo[MAX_PLAYERS];    // Hostile to this player?
    
    // Dialog
    unsigned long nextDialogTime;   // Cooldown for next dialog
    int dialogIndex;                // Which dialog (0-2)
    int dialogOrder[3];             // Shuffled order (for variety)
    int combatDialogCounter;        // Every 3rd round of combat
};
```

### NPC Attributes

Common attributes:

| Attribute | Type | Example | Purpose |
|-----------|------|---------|---------|
| name | string | "Goblin Warrior" | Display name |
| hp | int | 10 | Hit points |
| damage | int | 2 | Base damage to players |
| gold | int | 5 | Gold dropped on death |
| xp | int | 25 | XP awarded on death |
| aggressive | bool | 1 | Start combat automatically? |
| dialog_1 | string | "Attack!" | Combat dialog 1 |
| dialog_2 | string | "You're dead!" | Combat dialog 2 |
| dialog_3 | string | "Die!" | Combat dialog 3 |

### NPC Life Cycle

1. **Spawn** → Created at startup from `/npcs.vxi`
2. **Alive** → Can be attacked, dialog, roam
3. **Death** → Drop gold, schedule respawn, mark `alive=false`
4. **Respawn** → After 30 minutes, become alive again at spawn location with full HP

### NPC Aggression Check

Function: `checkNPCAggro()`

- Runs periodically
- For each aggressive NPC in room with players:
  - Set NPC as hostile to player
  - Initiate combat (`p.inCombat=true`, NPC `targetPlayer=index`)

### NPC Dialog

- **3 random dialog lines** per NPC
- **Combat dialog:** Every 3 combat rounds, random line from dialog pool
- **Combat bonus:** Adds exclamation mark to dialog

Example combat sequence:
1. Round 1: Player hits NPC
2. Round 2: Player hits NPC
3. Round 3: NPC yells dialog
4. Round 4: Player hits NPC
5. (Repeat cycle)

### NPC Finding

Function: `getNPCsAt(int x, int y, int z)`

Returns vector of all living NPC pointers in room.

---

## Command Reference

### Movement Commands

```
N / NORTH        Move north (0, -1, 0)
S / SOUTH        Move south (0, +1, 0)
E / EAST         Move east (+1, 0, 0)
W / WEST         Move west (-1, 0, 0)
NE / NORTHEAST   Move northeast (+1, -1, 0)
NW / NORTHWEST   Move northwest (-1, -1, 0)
SE / SOUTHEAST   Move southeast (+1, +1, 0)
SW / SOUTHWEST   Move southwest (-1, +1, 0)
U / UP           Move up (0, 0, +1)
D / DOWN         Move down (0, 0, -1)
```

**Portal Movement:**
```
<PORTAL_COMMAND> Activate portal (e.g., "enter", "climb")
                 Defined per room in rooms.txt
```

### Examination Commands

```
L / LOOK         Show current room description and exits
LOOK AT <item>   Examine specific item or NPC
LOOK <item>      Shorthand for LOOK AT
READ <item>      Alias for LOOK AT
LOOK IN <cont>   Show items inside container
SEARCH <item>    Deep search including hidden items
EXAMINE <item>   Detailed item inspection
```

### Inventory Commands

```
I / INVENTORY    Show all carried items with equipment status
GET <item>       Pick up item from floor
GET ALL          Pick up all items in room
DROP <item>      Drop item from inventory to floor
DROP ALL         Drop all items
GIVE <item> TO <player>  Transfer item to another player
GIVE <amount> COINS TO <player>  Transfer gold coins
```

### Container Commands

```
GET <item> FROM <container>      Pick up item from container
PUT <item> IN <container>        Store item in container
PUT ALL IN <container>           Store all items in container
GET ALL FROM <container>         Empty container
```

### Equipment Commands

```
WIELD <weapon>   Equip weapon (adds damage bonus)
UNWIELD          Unequip current weapon
WEAR <armor>     Equip armor to appropriate slot (adds armor bonus)
REMOVE <armor>   Unequip armor
```

### Consumption Commands

```
EAT <food>       Consume food item
DRINK <drink>    Consume beverage
                 (Items must have type="food" or type="drink")
```

### Combat Commands

```
KILL <npc>       Start combat with NPC
WIMP             Toggle wimp mode (auto-flee at low HP)
```

### Social Commands

```
SAY <message>          Speak to players in room
SHOUT <message>        Broadcast to all players
ACTION <verb> <arg>    Perform emote (wave, dance, etc.)
ACTIONS                List available emotes
```

### Shop Commands

```
BUY <item>       Purchase from shop in current room
SELL <item>      Sell item to shop
                 (Shops buy at 50% of item value)
```

### Character Commands

```
SCORE            Display character stats (level, XP, HP, etc.)
LEVELS           Show experience chart to next level
LEVELS <race>    Show level titles for race
HELP             Show help menu
PASSWORD         Change account password
```

### Quest Commands

```
QUEST LIST       List all quests and completion status
QUEST <number>   Show detailed quest info and current step
```

### Misc Commands

```
QUIT             Disconnect from game
```

---

## Wizard Commands

**Wizard Mode Requirements:**
- Account must be created with `IsWizard=true`
- Only wizards can execute admin commands

### Wizard Movement & Control

```
GOTO <x> <y> <z>          Teleport to coordinates
GOTO <player>              Teleport to another player's location
HEAL <player>              Restore player to full HP
INVIS                      Toggle invisibility (hidden from other players)
STATS                      Toggle detailed combat stats display
```

### Wizard Customization

```
ENTERMSG <message>         Set custom arrival message
EXITMSG <message>          Set custom departure message
ENTERMSG                   Clear custom entry message
EXITMSG                    Clear custom exit message
```

### Wizard World Management

```
DEBUG ITEMS                Dump all item definitions to Serial Monitor
DEBUG FILES                Dump all data files to Serial Monitor
DEBUG FLASHSPACE           Show flash storage usage (KB)
DEBUG DELETE <filename>    Delete file from filesystem (e.g., "/world_items.vxi")
```

### Wizard Information Display

```
WIZ                        Show wizard commands list
```

### Wizard Features (Automatic)

- **Wizard stats:** Detailed combat roll info shown when `showStats=true`
- **Death protection:** Cannot reach 0 HP (capped at 1)
- **Invisibility:** When `IsInvisible=true`, not shown in room lists or announcements
- **Version display:** Wizard login shows ESP32MUD version and compilation date/time

---

## Data Structures

### Core Game Data

**itemDefs** - `std::map<std::string, ItemDefinition>`
- All item templates
- Maps item ID to attributes

**worldItems** - `std::vector<WorldItem>`
- All items in existence (inventory, floor, containers)
- Index 0 reserved
- Dynamic array

**npcDefs** - `std::map<std::string, NpcDefinition>`
- All NPC templates
- Maps NPC type ID to attributes

**npcInstances** - `std::vector<NpcInstance>`
- All living and dead NPCs in world
- Updated with respawns

**quests** - `std::map<int, QuestDef>`
- Quest definitions
- Maps questId (1-10) to details

**players** - `Player[MAX_PLAYERS]`
- Fixed array of 10 player slots
- players[i].active indicates if slot in use

---

## File Formats

### Rooms File

**File:** `/rooms.txt`  
**Format:** Space-separated values

```
x y z name description exit_n exit_s exit_e exit_w exit_ne exit_nw exit_se exit_sw exit_u exit_d portal_flag px py pz portal_command portal_text
```

Example:
```
0 0 0 Town_Square A_bustling_town_square 1 1 1 1 1 1 1 1 1 1 0 0 0 0 enter You_are_transported
```

### Items Definition File

**File:** `/items.vxd`  
**Format:** Space-separated with nested attributes

```
item_id item_type key1=val1 key2=val2 key3=val3
```

Example:
```
sword_iron weapon name=Iron_Sword damage=3 value=50 weight=2 description=A_sharp_blade wieldable=1 mobile=1
```

### World Items File

**File:** `/items.vxi` or `/world_items.vxi`  
**Format:** Tab-separated with embedded attributes

```
item_id ownerName parentName x y z value attr1:val1|attr2:val2|...
```

Example:
```
sword_iron 	 	0 0 0 50 name:Iron_Sword|damage:3
gold_coin PlayerOne 	-1 -1 -1 25
```

### NPC Definition File

**File:** `/npcs.vxd`  
**Format:** Space-separated with attributes

```
npc_id npc_type key1=val1 key2=val2
```

Example:
```
goblin_warrior monster name=Goblin_Warrior hp=10 damage=2 gold=5 aggressive=1 xp=25
```

### NPC Instance File

**File:** `/npcs.vxi`  
**Format:** Tab-separated values

```
npc_id parentId x y z spawnX spawnY spawnZ hp alive
```

Example:
```
goblin_warrior 	0 0 0 0 0 0 10 1
```

### Quests File

**File:** `/quests.txt`  
**Format:** Multi-line with section headers

```
QUEST quest_id quest_name description difficulty xp_reward gold_reward
STEP step_num task type target item phrase targetX targetY targetZ
UNLOAD npc_id
(blank line)
```

Example:
```
QUEST 1 Defeat the Goblin King Slay the king of goblins Hard 100 50
STEP 1 kill goblin_king
STEP 2 return shopkeeper

```

### Player Save File

**File:** `/players/[name].txt`  
**Format:** Key=value pairs

```
name=PlayerName
raceId=0
level=5
xp=500
coins=1000
maxHp=15
hp=15
roomX=0
roomY=0
roomZ=0
IsWizard=0
questStepDone=0x...
questCompleted=0x...
...
```

---

## Game Mechanics

### Experience System

- **Killed NPCs grant XP** from NPC definition
- **Quest completion grants XP** from quest definition
- **Level calculation:** `level = sqrt(xp / 100)`
- **Leveling rewards:**
  - +2 max HP
  - +1 base damage
  - +5 carry capacity
  - New title (race-specific)

### Damage Mechanics

**Player Attack:**
```
baseDamage = weaponBonus + attack (base 1)
roll = random(1 to baseDamage)
vs npcDefense (default 9)
```

**NPC Attack:**
```
baseDamage = from definition
roll = random(1 to baseDamage)
vs playerArmor (baseDefense + armorBonus)
absorption = random(0 to playerArmorBonus)
finalDamage = roll - absorption (minimum 1)
```

### Armor & Defense

- **Base AC:** 9 (difficulty to hit)
- **Armor Bonus:** From worn items
- **Damage Reduction:** Armor absorbs 0 to armorBonus damage
- **Stacking:** Each worn item in different slot adds bonus

### Carry Capacity

- **Base:** 32 items per inventory slot
- **Per level:** +5 items (up to level 20)
- **Base + Level 20:** 32 + (20 × 5) = 132 item slots
- **Gold:** Doesn't count against carry limit (stored separately)

### Gold System

- **Currency:** Integer coins
- **Stacking:** Multiple coins merge into "X gold coins" item
- **Shop prices:** Typically 50-100 gold per item
- **NPC loot:** 1-10 gold per NPC death
- **Shop buying:** Trades at 50% of item value
- **Untradeable:** Some items have value=0

### Respawn System

- **NPC respawn timer:** 30 minutes after death
- **NPC respawn location:** Original spawn point (spawnX, spawnY, spawnZ)
- **HP restoration:** Full HP on respawn
- **Hostility reset:** All players reset as non-hostile
- **Quest integration:** Respawned NPCs can be killed again

### Passive Healing

- **Rate:** 1 HP every 30 seconds
- **Trigger:** Only when not in active combat
- **Check frequency:** Every 30 seconds
- **Wizard healing:** Wizards heal faster or can use HEAL command

### Save System

- **Auto-save:** On disconnect
- **File location:** `/players/[name].txt`
- **Data included:** All stats, inventory, quests, location, etc.
- **Load:** On login, recreate player state from file

### World Reboot

- **Cycle:** Every 6 hours
- **Warnings:** At 5min, 2min, 1min, 30sec, 5sec
- **Process:** Broadcast warning → Wait → `esp_restart()`
- **Recovery:** On restart, all files restored, world resets to `/items.vxi` and `/npcs.vxi`

---

## Quick Reference: Common Tasks

### Add a New Item to World

1. Define in `/items.vxd`:
   ```
   new_item weapon name=New_Item damage=5 value=75 weight=2
   ```

2. Add to `/items.vxi`:
   ```
   new_item 	 	5 5 0 75 name:New_Item|damage:5
   ```

3. Restart server or manually spawn with YMODEM

### Add an NPC to World

1. Define in `/npcs.vxd`:
   ```
   new_npc monster name=New_NPC hp=15 damage=3 gold=10 aggressive=1 xp=30
   ```

2. Add instance to `/npcs.vxi`:
   ```
   new_npc 	5 5 1 5 5 1 15 1
   ```

3. Restart server

### Create a Quest

1. Add to `/quests.txt`:
   ```
   QUEST 1 My Quest Get me a sword Medium 50 25
   STEP 1 give sword merchant
   STEP 2 reach 10 10 0
   
   ```

2. Restart server

### Create a Wizard Account

1. Edit player file manually:
   ```
   IsWizard=1
   ```

Or wait for player login and manually edit `/players/[name].txt`

### View Debug Info (Wizard)

```
debug items    → Shows all item definitions + world state
debug files    → Dumps all data files to Serial Monitor
debug flashspace → Shows storage usage
```

---

## Known Limitations & Constraints

| Limit | Value | Impact |
|-------|-------|--------|
| Max Players | 10 | Hard limit on concurrent connections |
| Max NPCs | 50 | Hard limit on living NPCs |
| Max Inventory Items | 32 | Per-player carry limit |
| Max Item Name Length | 32 | Item IDs, player names |
| Max Room Description | 512 | Room flavor text |
| Max Quests | 10 | Quest slots 1-10 |
| Max Quest Steps | 10 | Per quest |
| Max Coordinates | ±2,097,151 | 21-bit range per axis |
| Max Level | 20 | Experience cap |
| Max Equipment Slots | 11 | Body slots for armor |
| Combat Cooldown | 3000 ms | Time between attacks |
| NPC Respawn | 600 seconds | 10-minute default (configurable) |
| Global Reboot | 6 hours | Auto-restart interval |
| Flash Storage | 2 MB | SPIFFS filesystem limit |
| RAM Usage | ~327 KB | Total available |

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────┐
│                 ESP32 Hardware                       │
│  ┌──────────────────────────────────────────────┐   │
│  │ WiFi + Serial I/O                            │   │
│  └──────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
             ↓
┌─────────────────────────────────────────────────────┐
│            Main Event Loop                           │
│ ┌────────────────────────────────────────────────┐  │
│ │ - Check WiFi clients (telnet connections)     │  │
│ │ - Read player input (one line per update)     │  │
│ │ - Parse commands and dispatch to handlers     │  │
│ │ - Update combat timers (3-sec rounds)         │  │
│ │ - Update healing (30-sec passive)             │  │
│ │ - Check NPC aggression                        │  │
│ │ - Check global reboot timer                   │  │
│ └────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────┘
             ↓
┌─────────────────────────────────────────────────────┐
│         Command Handler (handleCommand)              │
│ ┌────────────────────────────────────────────────┐  │
│ │ - Parse command and arguments                 │  │
│ │ - Dispatch to appropriate command function    │  │
│ │ - Validate player state and permissions      │  │
│ │ - Update world state (items, NPCs, quests)   │  │
│ │ - Broadcast messages to other players        │  │
│ └────────────────────────────────────────────────┘  │
└─────────────────────────────────────────────────────┘
             ↓
┌─────────────────────────────────────────────────────┐
│         Core Game Systems                           │
│ ┌──────────────┐ ┌──────────────┐ ┌────────────┐  │
│ │ Item System  │ │ Combat       │ │ Quest      │  │
│ │              │ │ Engine       │ │ Tracker    │  │
│ │ - Inventory  │ │              │ │            │  │
│ │ - Equipment  │ │ - To-hit     │ │ - Steps    │  │
│ │ - Containers │ │ - Damage     │ │ - Events   │  │
│ │              │ │ - Rounds     │ │ - Rewards  │  │
│ └──────────────┘ └──────────────┘ └────────────┘  │
│ ┌──────────────┐ ┌──────────────┐ ┌────────────┐  │
│ │ NPC System   │ │ Player Mgmt   │ │ World      │  │
│ │              │ │              │ │ Structure  │  │
│ │ - Spawning   │ │ - Stats      │ │ - Voxels   │  │
│ │ - Aggro      │ │ - Persistence│ │ - Rooms    │  │
│ │ - Dialog     │ │ - Leveling   │ │ - Portals  │  │
│ │ - Respawn    │ │ - Healing    │ │            │  │
│ └──────────────┘ └──────────────┘ └────────────┘  │
└─────────────────────────────────────────────────────┘
             ↓
┌─────────────────────────────────────────────────────┐
│            LittleFS Persistence                      │
│  ┌──────────────────────────────────────────────┐   │
│  │ - /rooms.txt (world map)                     │   │
│  │ - /items.vxd, /items.vxi (item defs/state)  │   │
│  │ - /npcs.vxd, /npcs.vxi (NPC defs/instances)│   │
│  │ - /quests.txt (quest definitions)            │   │
│  │ - /players/[name].txt (player save files)    │   │
│  │ - /rooms.idx, /rooms.bin (optional indexes) │   │
│  └──────────────────────────────────────────────┘   │
└─────────────────────────────────────────────────────┘
```

---

## Notes for Future Development

### Performance Considerations

- **Large world maps:** Consider voxel index optimization for rooms.txt lookup
- **Many items:** WorldItems vector grows unbounded; consider cleanup/archiving
- **NPCs:** 50-NPC limit appropriate for embedded device
- **Players:** 10-player limit based on RAM (~16% with full world state)

### Extension Ideas

1. **Skill system** - Class-based abilities (warrior, mage, rogue)
2. **Magic spells** - MP/mana system, spell definitions
3. **PvP combat** - Player vs. Player duels and arenas
4. **Guilds** - Player groups with shared resources
5. **Economy** - Banking, trading posts, auctions
6. **Crafting** - Combine items to create new items
7. **Dynamic world** - Timed events, weather, day/night cycles
8. **Multiple areas** - Load different world files for different zones
9. **Web interface** - HTML dashboard for game monitoring
10. **Discord bot** - Link game to Discord for notifications

### Known Issues & TODOs

- [ ] Room index binary search may need optimization for large maps
- [ ] Memory fragmentation with long-running servers
- [ ] No encryption for credentials (plaintext WiFi password storage)
- [ ] No input rate limiting (spam protection)
- [ ] No logging/audit trail for player actions
- [ ] Portal system currently read-only from rooms.txt

---

## File Checklist for Production

Before deploying to production:

- [ ] `/credentials.txt` - WiFi SSID and password
- [ ] `/rooms.txt` - Complete world map with coordinates and exits
- [ ] `/items.vxd` - All item definitions (weapons, armor, containers, etc.)
- [ ] `/items.vxi` - Starting world items (treasures, quest items, shop goods)
- [ ] `/npcs.vxd` - All NPC definitions (monsters, merchants, quest-givers)
- [ ] `/npcs.vxi` - NPC world placements and spawn points
- [ ] `/quests.txt` - All quest definitions with steps and rewards
- [ ] `/GetSpawnRoom.txt` - Initial spawn location coordinates (X Y Z)
- [ ] Wizard account pre-created in `/players/[admin].txt`

---

## Version History

| Version | Date | Notes |
|---------|------|-------|
| 1.0.0 | Jan 18, 2026 | Initial public release |
| 0.9.0 | Jan 15, 2026 | Beta: combat, quests, shops working |
| 0.5.0 | Jan 10, 2026 | Alpha: basic movement and inventory |

---

**End of ESP32MUD System Reference Guide**

*This documentation should be referenced for all future development and troubleshooting. Updated with each major feature addition or system change.*
