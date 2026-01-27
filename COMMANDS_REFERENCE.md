# ESP32 MUD - Commands Reference

## Player Commands

### Navigation
- `(n)orth, (s)outh, (e)ast, (w)est, (u)p, (d)own` - Move in a direction
- `map` - Toggle the Mapper Utility on/off
- `townmap` - Display Town Map of Espertheru

### Interaction
- `(l)ook` - Examine your current room
- `read <item>` - Read text on an object
- `examine <item>` / `exam <item>` - Inspect an item for details
- `search <item>` - Search an item for hidden contents

### Inventory & Items
- `(i)nventory` - List what you're carrying
- `get <item>` - Pick up an item
- `put <item> <container>` - Place item in a container
- `drop <item>` - Drop an item
- `drop all` - Drop everything
- `wear <item>` - Equip armor
- `remove <item>` - Remove worn armor
- `wield <item>` - Equip a weapon
- `unwield` - Stop wielding your weapon
- `give <item> <player>` - Give item to another player

### Commerce
- `buy <item>` - Purchase from vendor
- `sell <item>` - Sell item to vendor

### Consumption
- `eat <item>` - Consume food
- `drink <item>` - Consume drink

### Social & Communication
- `say <message>` - Speak to players in your room
- `shout <message>` - Broadcast to all players online
- `tell <player> <message>` - Send private message to a player
- `actions` - List available emotes
- `who` - List all players currently online

### Mail System
- `send <player>@<email>` - Send mail to player's email
- `checkmail` / `mail` - Check received messages

### Weather System
- `weather [city]` - Get current weather (weather station only)
- `forecast [city]` - Get 3-day forecast (weather station only)

### Games & Entertainment
- `play` - Play High-Low card game

### Character Info & Progression
- `(sc)ore` - Show your stats and inventory
- `levels` - Display level titles and XP ranges
- `advance` - Advance to next level (if eligible)
- `questlist` - Show available quests and completion status
- `quest <quest_number>` - Get details on a specific quest

### Account & System
- `password` - Change your password
- `rules` - Display server rules
- `wimp` - Toggle auto-flee when health is low
- `save` - Manually save character
- `qrcode [message]` - Generate QR code from text
- `help` - Display this help menu
- `(q)uit` - Save and disconnect

---

## Wizard Commands

Access with `wizhelp` command when logged in as a wizard.

### Character Manipulation
- `heal <player>` - Fully restore player HP
- `blind <player>` - Blind a player (eyes injured)
- `hobble <player>` - Reduce player movement speed
- `lame <player>` - Severe leg injury, heavy speed penalty

### World Management
- `resetworlditems` - Reset all world items to spawn locations
