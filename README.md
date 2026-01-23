# ESP32MUD

A complete **telnet-based Multi-User Dungeon (MUD)** game server running on the Seeed XIAO ESP32C3 microcontroller with real-time multiplayer support, dynamic commerce systems, and persistent world state.

## Features

### Core Gameplay
- 🎮 **Real-time multiplayer** - Up to 10 concurrent players via telnet
- 🗺️ **3D voxel world** - 10-directional movement (N, S, E, W, NE, NW, SE, SW, Up, Down) with unlimited coordinates
- ⚔️ **Combat system** - Turn-based 3-second combat rounds with NPC counter-attacks, armor absorption, and tactical positioning
- 🏹 **Equipment & items** - 11 body slots, nested containers, attribute system, stackable coins
- 📜 **Quest system** - Multi-step quest chains with event tracking, location triggers, and rewards
- 👹 **NPC system** - Dynamic spawning, aggression levels, dialog system, 10-minute respawn timers, loot drops
- 📊 **Leveling & progression** - 20 levels with race-specific titles and advancement requirements

### Recent Enhancements (Jan 2026)
- 🛒 **Dual shop system** - Blacksmith (weapons/armor) + Esperthertu Wares (misc items)
- 💰 **Sell all command** - Quickly liquidate all items from a shop type
- 🎯 **Item attribute system** - Items display with custom "name" attributes in rooms
- 🔐 **Session management** - One active session per player; duplicate logins disconnect old session and restore location
- 📸 **QR code generation** - Support for player-targeted QR codes (syntax: `qrcode playername message`)
- 📋 **Session logging** - Comprehensive LOGIN/LOGOUT audit trail to `/session_log.txt` with timestamps
- 🕐 **NTP time synchronization** - Automatic time sync from NTP servers on WiFi connection
- 📧 **Post Office email system** - Send/receive emails via `send` command; POP3 retrieval spawns letters in Post Office (location: 252,248,50)
- 🍻 **Tavern drink system** - Drunkenness tracking with HP restoration drinks (Giant's Beer, Honeyed Mead, Faery Fire)

### Infrastructure
- 💾 **Persistence** - All player data, world state, and item instances saved to LittleFS
- 🧙 **Wizard mode** - Comprehensive admin commands for world management, debugging, and administration
- 🔧 **Development tools** - Debug commands for item inspection, storage analysis, and NPC management
- 📡 **WiFi connectivity** - 802.11b/g/n @ 2.4GHz with automatic NTP time sync
- 🔌 **USB-CDC support** - Serial provisioning, debug output, and file upload capability

## Hardware

- **Seeed XIAO ESP32C3** - 160MHz single-core, 320KB SRAM, 4MB Flash
- **WiFi** - 802.11b/g/n @ 2.4GHz
- **USB-CDC** - For provisioning and serial monitoring
- **LittleFS** - Persistent filesystem (capped at ~1.8MB usable)

## Getting Started

### Prerequisites

- PlatformIO IDE or CLI (v2.0+)
- Seeed XIAO ESP32C3 board
- USB-C cable for programming
- Telnet client (Windows, macOS, Linux)

### Quick Installation

1. Clone this repository:
```bash
git clone https://github.com/Veramacor/ESP32MUD.git
cd ESP32MUD
```

2. Install PlatformIO:
```bash
pip install platformio
```

3. Build and upload firmware:
```bash
platformio run --target upload --environment seeed_xiao_esp32c3
```

4. Monitor serial output (first boot):
```bash
platformio device monitor --environment seeed_xiao_esp32c3 --port COM5
```

5. Provision WiFi credentials when prompted via serial
6. Connect via telnet:
```bash
telnet <YOUR_ESP32_IP> 23
```

See [START_HERE.md](START_HERE.md) for detailed setup instructions.

## Documentation

- **[START_HERE.md](START_HERE.md)** - First-time setup and provisioning guide
- **[ESP32MUD_SYSTEM_REFERENCE.md](ESP32MUD_SYSTEM_REFERENCE.md)** - Complete system documentation (2000+ lines)
- **[QUICK_COMMANDS.md](QUICK_COMMANDS.md)** - Player and wizard command reference
- **[MAIL_SYSTEM.md](MAIL_SYSTEM.md)** - Post Office email system (POP3 retrieval, letter spawning)
- **[TAVERN_SYSTEM.md](TAVERN_SYSTEM.md)** - Drunkenness tracking and drink system
- **[FILE_INDEX.md](FILE_INDEX.md)** - Data file formats and schema
- **[COMPLETION_SUMMARY.txt](COMPLETION_SUMMARY.txt)** - Feature checklist and implementation status

## File Structure

```
ESP32MUD/
├── src/
│   ├── ESP32MUD.cpp                    # Main game engine (11,400+ lines)
│   ├── GetSpawnRoom.txt                # Default spawn coordinates
│   └── YmodemBootloader.h              # Binary file upload handler
├── include/
│   └── version.h                       # Auto-generated version info
├── lib/
│   └── README
├── data/                               # LittleFS filesystem (uploaded to device)
│   ├── rooms.txt                       # World map (3D voxel coordinates & descriptions)
│   ├── items.vxd                       # Item definitions (format: id|type|attr=val|...)
│   ├── items.vxi                       # World item instances (placed in rooms)
│   ├── npcs.vxd                        # NPC definitions
│   ├── npcs.vxi                        # NPC world placement
│   ├── quests.txt                      # Quest definitions
│   ├── credentials.txt                 # WiFi SSID and password
│   ├── session_log.txt                 # Login/logout audit trail (auto-generated)
│   └── player_*.txt                    # Individual player save files
├── scripts/
│   └── version_generator.py            # Build-time version script
├── platformio.ini                      # PlatformIO build configuration
├── ESP32MUD_SYSTEM_REFERENCE.md        # Full system documentation
├── README.md                           # This file
├── LICENSE                             # MIT License
└── .gitignore
```

## Game Systems Overview

### Combat
- **3-second turn-based rounds** - No real-time action spam
- **To-hit calculation** - Attacker roll vs. defender armor class
- **Damage system** - Armor absorbs damage; health regenerates after combat
- **NPC responses** - Combat dialog, counter-attacks, loot drops on defeat

### Shops
- **Blacksmith** (254, 244, 50) - Weapons and armor
- **Esperthertu Wares** (251, 248, 50) - Miscellaneous consumables and tools
- **Dynamic inventory** - Shops track stock and prices per item
- **Sell/sell all** - Players can liquidate inventory with filtering by shop type

### Items & Equipment
- **Parent/child hierarchy** - Containers can hold other items
- **11 equipment slots** - Head, torso, hands, legs, feet, arms, left hand, right hand, neck, belt, back
- **Attribute system** - Custom properties (name, description, weight, container capacity, etc.)
- **Stackable coins** - Gold currency with no weight penalty

### Quests
- **Multi-step chains** - Sequential objectives
- **Event triggers** - Kill NPC, reach location, collect item
- **Rewards** - XP and gold on completion
- **NPC interaction** - Quest-related dialog and state tracking

### NPCs
- **Configurable stats** - HP, damage, armor, aggression, experience value
- **Combat AI** - Simple but effective targeting and engagement
- **Respawn system** - 10-minute timer between deaths (configurable)
- **Loot drops** - Gold and item rewards on NPC death
- **Dialog system** - Combat taunts and quest-related responses

### Player Sessions
- **Session enforcement** - Only one active session per player name
- **Duplicate login protection** - Old session disconnected; player restored to last room
- **Audit logging** - All logins/logouts recorded to `/session_log.txt` with timestamps
- **Location persistence** - Players respawn in their last known room

## Wizard Commands

See [QUICK_COMMANDS.md](QUICK_COMMANDS.md) for full command reference. Key admin features:

- `GOTO <x> <y> <z>` - Teleport to any coordinate
- `HEAL <player>` - Restore player health
- `INVIS` - Toggle invisibility
- `REBOOT` - Graceful world reboot (configurable 6-hour timer)
- `RESETWORLDITEMS` - Reset all item spawns
- `DEBUG ITEMS` - Dump all item definitions
- `DEBUG SESSIONS` - Show last 50 login/logout events
- `WIZ` - Display wizard command help
- Admin player creation during login

## Known Limitations

- **Max 10 concurrent players** - Hardware constraint
- **Max 50 living NPCs** - Memory limitation
- **32-item inventory limit** - Per-player cap
- **20 level cap** - Progression ceiling
- **4MB flash storage** - ~1.8MB usable after firmware
- **Telnet only** - No encryption; use on trusted networks only
- **Single-core ESP32** - Performance-sensitive with many concurrent operations

## Performance Notes

- **Flash usage**: ~45% with all features enabled
- **RAM usage**: ~17% baseline, increases with players/NPCs
- **WiFi**: Optional; NTP time sync requires WiFi connectivity
- **Telnet**: 23 TCP, unauthenticated (restrict to LAN via firewall)
- **Update frequency**: ~100ms game loop tick

## Troubleshooting

| Issue | Solution |
|-------|----------|
| Can't connect via telnet | Verify WiFi provisioning, check IP address with `pio device monitor` |
| Items not appearing in shops | Upload items.vxd data file via uploadfs; ensure itemIds match shop definitions |
| Session logs empty | Run game with players logging in; log file auto-creates on first login |
| NTP time not syncing | Verify WiFi is connected; check router allows NTP (port 123 UDP) |
| High flash usage | Reduce log file retention or compress data files |

## Contributing

Contributions are welcome! Please:

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Implement and test your changes
4. Commit with clear messages (`git commit -am 'Add amazing feature'`)
5. Push to your fork (`git push origin feature/amazing-feature`)
6. Open a Pull Request with description

## Version

**v1.1.0** - January 20, 2026
- Added session management with duplicate login prevention
- Implemented QR code player targeting
- Created comprehensive session audit logging
- Added NTP time synchronization
- Implemented dual shop system with Esperthertu Wares
- Fixed item attribute display in rooms
- Fixed wield command to use proper display names

**v1.0.0** - Initial release

## License

MIT License - See [LICENSE](LICENSE) file for details

## Support

For issues, feature requests, or documentation:

1. Check [ESP32MUD_SYSTEM_REFERENCE.md](ESP32MUD_SYSTEM_REFERENCE.md) for detailed info
2. Review [QUICK_COMMANDS.md](QUICK_COMMANDS.md) for command syntax
3. Check [FILE_INDEX.md](FILE_INDEX.md) for data file formats
4. Consult [SETUP_CHECKLIST.md](SETUP_CHECKLIST.md) for validation

## Author

Created for Seeed XIAO ESP32C3 by Veramacor

**Ready to play?** Connect via telnet to start your adventure in **Esperthertu**!
1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Commit changes (`git commit -am 'Add my feature'`)
4. Push to branch (`git push origin feature/my-feature`)
5. Open a Pull Request

## Support

For bugs, feature requests, or questions:
- Check `ESP32MUD_SYSTEM_REFERENCE.md` for documentation
- Review the issue tracker
- Contact the project maintainer

## Author

Created for Seeed XIAO ESP32C3

---

**Ready to play? Connect via telnet to start your adventure in Esperthertu!**
