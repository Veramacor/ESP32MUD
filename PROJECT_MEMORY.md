# Project Memory - ESP32 MUD System

## Project Overview
- **Name**: Seeed XIAO ESP32C3 MUD System
- **Platform**: PlatformIO
- **Board**: Seeed XIAO ESP32C3
- **Purpose**: MUD (Multi-User Dungeon) game system on embedded device

## Current Issues Being Investigated

### Townmap Legend Issue
- **Status**: In Progress
- **Description**: There is an issue with the townmap legend functionality
- **Related Files**: 
  - [src/ESP32MUD.cpp](src/ESP32MUD.cpp) - Main source code
  - [TAVERN_SYSTEM.md](TAVERN_SYSTEM.md) - System documentation
  - [ESP32MUD_SYSTEM_REFERENCE.md](ESP32MUD_SYSTEM_REFERENCE.md) - System reference

## Key Project Files
- **Main Code**: [src/ESP32MUD.cpp](src/ESP32MUD.cpp)
- **Configuration**: [platformio.ini](platformio.ini)
- **Version Info**: [include/version.h](include/version.h)
- **System Docs**: [TAVERN_SYSTEM.md](TAVERN_SYSTEM.md), [ESP32MUD_SYSTEM_REFERENCE.md](ESP32MUD_SYSTEM_REFERENCE.md)
- **Data Files**: 
  - [data/items.vxd](data/items.vxd)
  - [data/credentials.txt](data/credentials.txt)

## Build & Deployment
- **Build Task**: PlatformIO build
- **GitHub Push**: `📤 Push to GitHub` task available
- **Backup**: `🔒 Create Backup (timestamped)` task available
- **Upload Script**: [upload-files.ps1](upload-files.ps1)

## Next Steps
1. Review the specific townmap legend issue
2. Examine related code sections
3. Implement fix
4. Test and validate
5. Push changes to GitHub

---
*Last Updated: January 22, 2026*
