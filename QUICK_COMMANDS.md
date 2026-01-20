# ESP32MUD - Quick Command Reference

## Player Commands

### Navigation
```
NORTH / N              Move north
SOUTH / S              Move south
EAST / E               Move east
WEST / W               Move west
NORTHEAST / NE         Move northeast
NORTHWEST / NW         Move northwest
SOUTHEAST / SE         Move southeast
SOUTHWEST / SW         Move southwest
UP / U                 Move up
DOWN / D               Move down
```

### Inspection
```
LOOK / L               View current room details
LOOK <target>         Inspect specific target
INVENTORY / I         Show your inventory
SCORE                 Show character stats and progression
EQUIPMENT / EQ        Show equipped items
TIME                  Display current server time
```

### Items & Equipment
```
GET <item>            Pick up item from room
DROP <item>           Drop item in current room
WEAR <item>           Equip item
WIELD <item>          Equip weapon (main hand)
REMOVE <item>         Remove equipped item
PUT <item> IN <container>  Store item in container
```

### Commerce
```
SHOPS                 List all nearby shops
BUY <item>           Purchase item from shop
SELL <item>          Sell item to shop
SELL ALL             Sell all items to current shop (with type filtering)
```

### Communication
```
SAY <message>        Speak to players in current room
SHOUT <message>      Broadcast message to entire world
TELL <player> <msg>  Send private message to player
REPLY <message>      Reply to last message received
```

### Combat
```
KILL <npc>           Attack NPC in current room
FLEE                 Attempt to flee from combat
```

### Quests
```
QUEST LIST           Show available quests
QUEST STATUS         Show current quest progress
QUEST ACCEPT <id>    Accept a quest
QUEST COMPLETE <id>  Complete a quest step (if conditions met)
```

### Character
```
RACE <name>          Set your character race
TITLE                View your character title
```

## Wizard Commands

Wizard commands are only available to players in the `wizardExceptions` array or those who receive wizard mode.

### Navigation & Teleportation
```
GOTO <x> <y> <z>     Teleport to specific coordinates
```

### Player Management
```
HEAL <player>        Restore target player to full HP
KICK <player>        Disconnect player session
MAKE_WIZARD <player> Grant wizard status to player
```

### World Management
```
REBOOT               Initiate graceful world reboot (6-hour default cycle)
RESETWORLDITEMS      Reset all item spawns to original locations
INVIS                Toggle invisibility (wizard-only command)
```

### Debugging & Inspection
```
DEBUG ITEMS          Dump all item definitions from items.vxd
DEBUG NPCS           List all NPC definitions
DEBUG SESSIONS       Show last 50 login/logout events (descending)
DEBUG FLASHSPACE     Display storage usage and LittleFS stats
DEBUG PLAYERS        List all active players
```

### Admin Tools
```
WIZ                  Show wizard command help
```

## Item Attribute System

Items can have custom attributes defined in `items.vxd`. Common attributes:

```
name              Display name (used in rooms and inventory)
type              Item category (weapon, armor, misc, etc.)
desc              Item description
weight            Weight in units
ac                Armor class (armor only)
damage            Damage dice (weapons only)
can_contain       Capacity if container (0 = not container)
container_vol     Volume of item for containers
stackable         Can stack in inventory (1 = yes)
value             Resale value to shops
```

## Shop Locations

### Blacksmith (254, 244, 50)
- Sells: Weapons and armor
- Buys: Weapons and armor only
- Common items: Dagger, sword, shield, chainmail, plate

### Esperthertu Wares (251, 248, 50)
- Sells: Miscellaneous items and consumables
- Buys: Misc items only
- Example items: Torch, rope, waterskin, bedroll, lockpick set, herbs

## Session Management

### Automatic Features
- **One session per player** - Duplicate logins disconnect the old session
- **Location persistence** - Players respawn in their last known room
- **Session logging** - All logins/logouts recorded to `/session_log.txt`
- **Time synchronization** - Device syncs time from NTP on WiFi connection

## QR Code Generation

Wizards can generate QR codes with messages:

```
QRCODE <playername> <message>
```

Examples:
```
QRCODE Atew Welcome to Esperthertu!
QRCODE                    (displays QR locally if no player name given)
```

QR codes display in the player's terminal and can encode up to ~100 characters of text.

## Session Logging

The `debug sessions` command shows the last 50 login/logout events in descending order (newest first).

Format in `/session_log.txt`:
```
2026-01-20 14:32:15 | LOGIN | PlayerName
2026-01-20 14:35:42 | LOGOUT | PlayerName
```

## Tips for Players

1. **Weight management** - Items have weight; you can only carry so much
2. **Equipment matters** - Armor reduces damage; weapon choice affects combat
3. **Shop types** - Each shop specializes; sell to the right shop for better prices
4. **Location memory** - Logging out and back in returns you to your last location
5. **Communication** - Use SAY for room-only chat, SHOUT for world-wide broadcasts

## Tips for Wizards

1. **Safe teleportation** - Use GOTO to quickly navigate the world
2. **Session monitoring** - Run `debug sessions` to track player activity
3. **Inventory issues** - Use DEBUG ITEMS to verify item definitions match shop itemIds
4. **Time sync** - NTP syncs automatically; device time is critical for session logs
5. **Flash usage** - Monitor FLASHSPACE to prevent running out of storage
