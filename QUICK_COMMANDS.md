# ESP32MUD - Quick Command Reference

Complete guide to all commands available in ESP32MUD, organized by system and location.

---

## 🗺️ Player Commands

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
MAP                    Toggle map display
TOWNMAP                Show map of Espertheru town
```

### Inspection & Examination
```
LOOK / L               View current room details
LOOK <target>          Inspect specific target
READ <item>            Read text on an object
EXAMINE <item> / EXAM  Inspect item for details
SEARCH <item>          Search item for hidden contents
INVENTORY / I          Show your inventory and equipment
SCORE / SC             Show character stats and progression
EQUIPMENT / EQ         Show equipped items
TIME                   Display current server time
WHO                    List all players currently online
```

### Items & Equipment
```
GET <item>             Pick up item from room
GET <item> FROM <container>  Get item from inside a container
DROP <item>            Drop item in current room
DROP <amount> COINS    Drop gold coins from inventory
GIVE <item> TO <player>    Give item to another player
GIVE <amount> COINS TO <player>  Transfer gold coins
WEAR <item>            Equip armor to appropriate body slot
WIELD <item>           Equip weapon in main hand
UNWIELD                Unequip current weapon
REMOVE <item>          Remove equipped item
PUT <item> IN <container>   Store item inside a container
PUT ALL IN <container>      Store all items in container
```

### Commerce & Shops
```
SHOPS                  List all nearby shops
BUY <item>             Purchase item from shop
SELL <item>            Sell item to shop (receive 50% of value)
SELL ALL               Sell all items of a specific type to shop
```

### Communication
```
SAY <message>          Speak to players in current room
SHOUT <message>        Broadcast message to entire world
TELL <player> <msg>    Send private message to player
REPLY <message>        Reply to last message received
ACTION <verb>          Perform an emote (wave, dance, etc.)
ACTIONS                List available emotes
```

### Combat
```
KILL <npc>             Attack NPC in current room (by name or ID)
FLEE                   Attempt to flee from combat
WIMP                   Toggle auto-flee when health is low (< 10%)
```

### Quests
```
QUEST LIST             Show all available quests and completion status
QUEST <number>         Show detailed info about a specific quest
QUEST STATUS           Show your current quest progress
```

### Character Management
```
RACE <name>            Set your character race
TITLE                  View your current character title (based on level)
PASSWORD               Change your account password
HELP                   Display help menu
QUIT                   Save and disconnect from game
SAVE                   Manually save your character file
```

---

## 🏦 Banking System (Location: 254, 245, 50)

Safe place to store your gold coins. Bank gold survives death and cannot be stolen.

```
DEPOSIT <amount>       Deposit gold coins from inventory into bank account
WITHDRAW <amount>      Withdraw gold coins from bank account to inventory
BALANCE                Check account balance (coins in hand + bank total)
READ SIGN              View bank sign with services and rules
```

### Banking Features
- ✅ **Safe from death** - Bank gold doesn't drop when you die
- ✅ **Safe from theft** - Other players cannot steal your bank gold
- ✅ **Persistent** - Bank balance saves across login/logout sessions
- ✅ **Location-locked** - Only accessible at bank (254, 245, 50)
- ⚠️ **Inventory coins risky** - On-hand coins are dropped on death and can be looted

**Strategy:** Keep valuable coins in the bank. Carry only what you need for immediate purchases.

---

## 🍺 Tavern & Drinks (Location: 249, 242, 50)

The Tavern Libations serves drinks that restore HP but increase drunkenness. Higher drunkenness affects movement and combat.

```
SHOPS                  List nearby taverns
BUY <drink>            Purchase and consume a drink
SELL ALL               Sell all items to tavern keeper
READ SIGN              View tavern menu and your drunkenness level
```

### Available Drinks
| Drink | Price | HP Restored | Drunkenness |
|-------|-------|-------------|-------------|
| Giant's Beer | 10gp | 2 | +3 |
| Honeyed Mead | 5gp | 3 | +2 |
| Faery Fire | 20gp | 5 | +5 |

### Drunkenness Effects
- **Level 6 (Sober):** No penalties
- **Level 5-1:** Movement and combat increasingly difficult
- **Level 0 (Drunk):** Maximum penalties, very slow movement
- **Recovery:** Decreases by 1 point every ~60 seconds of idle time

---

## 📧 Post Office & Email (Location: 252, 248, 50)

Send and receive emails. Incoming mail spawns as letter items in the Post Office.

```
SEND <recipient@email> <message>   Send an email (10gp cost)
MAIL                               Check for incoming mail
CHECKMAIL / CHECK MAIL             Alias for MAIL command
```

### How It Works
1. Send email from Post Office with `send` command (10gp fee)
2. Email retrieval happens periodically (POP3 protocol)
3. Incoming emails spawn as letter items in Post Office
4. You can `get letter` and read it with `read letter`
5. Keep letters in your inventory or Post Office

---

## 💊 Doctor's Office (Location: varies)

Healthcare services for HP healing and injury treatment.

```
HEAL 1                 Basic healing (costs vary)
HEAL 2                 Enhanced healing (costs vary)
HEAL 3                 Full healing (costs vary)
HEAL 4                 Cure lameness / shoulder injury
HEAL 5                 Cure hobbling / leg injury
HEAL 6                 Cure blindness / head injury
HEAL 7                 Purchase healthcare plan OR treat blindness
```

### Healthcare Plan
- **Cost:** 10,000gp (one-time purchase)
- **Benefit:** Reduces healing service costs to 500gp deductible (services 4-7)
- **Permanent:** Once purchased, stays in your character file

### Injuries
- **Blindness (Head Injury):** Cannot use LOOK, MAP, TOWNMAP. Healing cost: 7000gp (500gp with plan)
- **Lameness (Shoulder Injury):** Cannot wield weapons, must use bare fists. Healing cost: 2000gp (500gp with plan)
- **Hobbling (Leg Injury):** Movement penalty every other step. Healing cost: 5000gp (500gp with plan)

---

## 🎮 Games & Entertainment

### High-Low Card Game (Location: 247, 248, 50)
Classic card game at the Game Parlor. Guess if next card is higher or lower than current.

```
PLAY 1 or PLAY 2       Start game (1=High-Low, 2=Chess)
BET <amount>           Place bet (10-50% of pot, or "pot" for full pot)
1 or 2                 Declare Ace HIGH (2) or LOW (1)
[ENTER]                Continue to next hand
END / QUIT / RESIGN    End the game
READ SIGN              View game rules and current pot
```

### Chess (Location: 247, 248, 50)
Full chess game with algebraic notation.

```
PLAY 2                 Start chess game
<move>                 Enter move in algebraic (e.g., e2e4, d7d5)
HELP                   Show chess help and legal moves
END / QUIT / RESIGN    Resign from game
READ SIGN              View chess rules
```

---

## 🌤️ Weather & Forecast (Location: varies - Weather Station)

Get real-world weather and forecast data for any city.

```
WEATHER [city]         Get current weather for specified city
WEATHER                Get weather for your stored city preference
FORECAST [city]        Get 3-day forecast for specified city
FORECAST               Get forecast for your stored city preference
```

### How It Works
1. First use: `weather orlando` to set your city preference
2. Subsequent use: Just `weather` to get weather for your stored city
3. Change city anytime: `weather miami` updates your preference
4. Forecast works the same way

### City Format Examples
```
weather orlando
weather detroit,mi
weather new york,ny
forecast denver,co
```

---

## 🏤 Blacksmith Shop (Location: 254, 244, 50)

Buy weapons and armor, sell weapons/armor only.

```
SHOPS                  List nearby shops
BUY <item>             Purchase weapon or armor
SELL <item>            Sell weapon or armor (50% of value)
SELL ALL               Sell all weapons and armor
```

### Available Items
- **Weapons:** Dagger, Skinning Knife, Sword, Mace, and more
- **Armor:** Studded Leather, Iron Helmet, Iron Boots, Chainmail, Plate Armor
- **Prices:** Range from 15gp (knife) to 500gp+ (plate armor)

---

## 🛍️ Esperthertu Wares (Location: 251, 248, 50)

General merchandise store. Buy miscellaneous items and consumables.

```
SHOPS                  List nearby shops
BUY <item>             Purchase misc item
SELL <item>            Sell misc item (50% of value)
SELL ALL               Sell all misc items
```

### Available Items
- **General:** Torch, Rope, Waterskin, Bedroll, Lockpick Set
- **Consumables:** Herbs, Candles, Iron Spoon, Map Fragments
- **Prices:** Range from 1gp (candle) to 12gp (lockpick set)

---

## 🧙 Wizard Commands

Wizard-only commands available to players with wizard status or in `wizardExceptions` array.

### Teleportation & Movement
```
GOTO <x> <y> <z>      Teleport to specific world coordinates
GOTO <player>          Teleport to a specific player's location
SUMMON <player>        Bring a player to your current location
```

### Player Management & Abilities
```
HEAL <player>          Restore target player to full HP
KICK <player>          Disconnect a player's session
MAKE_WIZARD <player>   Grant wizard status to a player
BLIND <player>         Toggle blindness on target (cannot look/map/combat)
HOBBLE <player>        Toggle hobbling (movement every-other-step penalty)
LAME <player>          Toggle lameness (cannot wield weapons)
INVIS                  Toggle your personal invisibility
```

### World Management & Debugging
```
REBOOT                 Initiate graceful world reboot (6-hour default cycle)
RESETWORLDITEMS        Reset all item spawns to original locations
CLONE <item/npc>       Clone an item or NPC to your current room
CLONEGOLD <amount>     Spawn gold coins to your current room
DEBUG ITEMS            Dump all item definitions from items.vxd
DEBUG NPCS             List all NPC definitions with attributes
DEBUG SESSIONS         Show last 50 login/logout events (newest first)
DEBUG FLASHSPACE       Display LittleFS storage usage and stats
DEBUG PLAYERS          List all saved player files
DEBUG ONLINE           List all connected players with stats
DEBUG <player>         Dump a specific player's complete data
DEBUG YMODEM           Show YMODEM transfer log
```

### Customization
```
ENTERMSG <text>        Set custom arrival message (when you enter a room)
EXITMSG <text>         Set custom departure message (when you leave)
STATS                  Toggle wizard stats display on/off
QRCODE [message]       Generate QR code (displays locally or targets player)
```

### Quest Administration
```
QUESTINFO <id>         Show quest definition and all steps
```

### Help
```
WIZ                    Display all wizard commands and help
```

---

## 🎯 Item Attribute System

Items in the world have custom attributes defined in `items.vxd`. These affect behavior and display:

```
name              Display name (shown in rooms and inventory)
type              Item category (weapon, armor, misc, food, drink, etc.)
desc              Item description (shown when examined)
weight            Weight in units (affects carrying capacity)
ac                Armor class (for armor items - lower AC is better)
damage            Damage dice (for weapons - e.g., "1d6", "1d8+1")
can_contain       Container capacity (0 = not a container)
container_vol     Container volume capacity
stackable         Can items stack in inventory (1=yes, 0=no)
value             Resale value to shops (50% given to player)
mobile            Can item be picked up and moved (1=yes, 0=no)
```

---

## 📍 Location Quick Reference

| Name | Coordinates | Purpose |
|------|-------------|---------|
| Spawn Point | (250, 250, 50) | Starting location for new characters |
| Blacksmith | (254, 244, 50) | Buy weapons/armor |
| Bank | (254, 245, 50) | Deposit/withdraw gold safely |
| Tavern | (249, 242, 50) | Buy drinks (HP restore) |
| Post Office | (252, 248, 50) | Send/receive email |
| Game Parlor | (247, 248, 50) | High-Low card game & Chess |
| Wares | (251, 248, 50) | Buy misc items & consumables |
| Weather Station | (varies) | Current weather & forecast |
| Doctor's Office | (varies) | HP healing & injury treatment |

---

## ⚡ Death & Respawning

When your character dies:

- **XP Loss:** You lose 1/3 of your total experience points (may result in level loss)
- **Inventory Loss:** ALL items you're carrying are dropped in your death location
- **Coin Loss:** ALL on-hand coins are dropped as loot on the ground
- **Bank Safety:** ✅ Bank gold SURVIVES death and cannot be looted
- **Respawn:** You respawn at the spawn point (250, 250, 50)
- **Equipment Loss:** Wielded and worn items are cleared
- **Recovery:** Items stay in death location for other players to loot

**Strategy:** Keep valuable coins in the bank. Only carry what you can afford to lose.

---

## 🔑 Key Features

### Session Management
- **One session per player** - Duplicate logins disconnect your old session
- **Location persistence** - You respawn in your last known room (unless dead)
- **Session logging** - All logins/logouts recorded to `/session_log.txt` with timestamps
- **Time sync** - Device syncs time from NTP servers on WiFi (important for session logs)
- **Auto-save** - Character saves automatically after major actions

### Combat Features
- **NPC Aggression** - Some NPCs attack on sight, others are friendly
- **Combat Penalties** - Injuries (blindness, hobbling, lameness) affect combat
- **Injury Trigger** - 1/1000 chance per combat round to suffer injury
- **Fleeing** - Type `flee` to escape combat or toggle `wimp` for auto-flee
- **Death Recovery** - Respawn with full HP at spawn point

### Equipment System
- **Armor Class (AC)** - Lower AC = better protection, reduces damage taken
- **Weapon Damage** - Different weapons have different damage dice
- **Multiple Slots** - Equip head, feet, hands, body, and main hand
- **Stat Bonuses** - Equipment provides bonuses to damage and armor

### Inventory Management
- **Weight Limit** - Total weight of items cannot exceed carrying capacity
- **Container System** - Some items are containers (bags, chests)
- **Get/Drop** - Pick up and drop individual items or all items
- **Stackable Items** - Some items stack (coins, herbs, etc.)

---

## 💡 Tips for New Players

1. **Start with LOOK** - Examine your surroundings and read signs
2. **Build your inventory** - Equip armor and a weapon early
3. **Explore shops** - Buy items that suit your playstyle
4. **Use the bank early** - Start building a safe coin reserve
5. **Read signs** - Many locations have informational signs (READ SIGN)
6. **Avoid dangerous areas** - Some locations have tough NPCs
7. **Save often** - Type SAVE to manually save your character
8. **Check TIME** - Important for understanding session logs and events
9. **Watch drunkenness** - Avoid getting too drunk in combat
10. **Keep death-safe money** - Use the bank for long-term wealth

---

## 🎓 Tips for Wizards

1. **GOTO is powerful** - Use it to navigate quickly and test new areas
2. **DEBUG commands** - Use DEBUG to understand player/item/NPC states
3. **CLONE items** - Spawn test items without modifying data files
4. **Monitor SESSIONS** - Track player activity with DEBUG SESSIONS
5. **RESETWORLDITEMS** - Use to fix broken item spawns
6. **BLIND/HOBBLE/LAME** - Test injury systems without combat
7. **SUMMON players** - Quickly transport players for testing or events
8. **Track FLASHSPACE** - Monitor storage to prevent out-of-memory errors
9. **Use ENTERMSG/EXITMSG** - Add personality to your presence
10. **WIZ command** - Display all wizard commands when you forget syntax

---

## ❓ Frequently Asked Questions

**Q: Where do I start?**
A: You spawn at coordinates (250, 250, 50). Look around and read signs to get oriented.

**Q: How do I make money?**
A: Kill NPCs for loot or sell items to shops (50% of item value).

**Q: How do I prevent losing money on death?**
A: Use the BANK! Deposit your coins and they're safe from theft and death.

**Q: Can I get my items back after dying?**
A: They're dropped at your death location for ~30 minutes. Rush back and pick them up!

**Q: What's the best strategy in High-Low?**
A: Card counting helps. Bet more when odds are in your favor.

**Q: How do injuries happen?**
A: 1-in-1000 chance per combat round to suffer blindness, lameness, or hobbling.

**Q: Can wizards see me if I'm invisible?**
A: No, invisibility hides you from everyone including other wizards.

**Q: How long does drunkenness last?**
A: Decreases by 1 point every ~60 seconds. Avoid drinking before combat!

**Q: What's my carrying capacity?**
A: Item carrying is weight-based. Lighter items let you carry more.

**Q: How do I change my password?**
A: Type `password` and follow the prompts.

---

## 📚 Additional Documentation

For more detailed information, see:
- **[ESP32MUD_SYSTEM_REFERENCE.md](ESP32MUD_SYSTEM_REFERENCE.md)** - Complete system documentation
- **[README.md](README.md)** - Project overview and features
- **[MAIL_SYSTEM.md](MAIL_SYSTEM.md)** - Post Office email system details
- **[TAVERN_SYSTEM.md](TAVERN_SYSTEM.md)** - Drunkenness mechanics
- **[FILE_INDEX.md](FILE_INDEX.md)** - Data file formats
- **[COMPLETION_SUMMARY.txt](COMPLETION_SUMMARY.txt)** - Feature checklist

---

**Last Updated:** January 27, 2026  
**Version:** 1.2.0
