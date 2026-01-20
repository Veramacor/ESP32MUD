# ESP32MUD - Getting Started Guide

**Welcome!** This guide walks you through setting up ESP32MUD from scratch.

## Prerequisites

Before starting, ensure you have:

- **Hardware**:
  - Seeed XIAO ESP32C3 microcontroller
  - USB-C cable
  - Computer with Windows, macOS, or Linux
  - WiFi network access (for provisioning)

- **Software**:
  - Python 3.8+ (for PlatformIO)
  - PlatformIO CLI or VS Code + PlatformIO extension
  - Telnet client (built-in on most systems)
  - Git (optional, for cloning)

- **Knowledge**:
  - Basic command-line usage
  - Serial port concepts
  - Telnet basics (connecting to remote servers)

## Step-by-Step Setup

### Step 1: Clone or Download the Repository

```bash
# Option A: Clone via Git
git clone https://github.com/Veramacor/ESP32MUD.git
cd ESP32MUD

# Option B: Download as ZIP and extract
# Navigate to repository folder in terminal
cd ESP32MUD-main
```

### Step 2: Install PlatformIO

If using command-line PlatformIO:

```bash
# Install pip packages
pip install platformio

# Verify installation
pio --version
```

If using VS Code extension:
1. Open VS Code
2. Go to Extensions (Ctrl+Shift+X)
3. Search "PlatformIO"
4. Click "Install" on official PlatformIO extension

### Step 3: Connect Your ESP32C3

1. **Connect via USB**: Plug in your Seeed XIAO ESP32C3 with USB-C cable
2. **Identify COM port**:
   - Windows: Check Device Manager → COM Ports
   - macOS/Linux: `ls /dev/tty.* | grep -i usb`
   - Note your port (e.g., COM5, /dev/ttyUSB0)

### Step 4: Build Firmware

From the repository root folder:

```bash
# Compile the firmware
platformio run --environment seeed_xiao_esp32c3

# Expected output:
# Processing seeed_xiao_esp32c3
# Building firmware...
# SPIFFS Image will be built...
# Flash size: 4MB
# RAM size: 320KB
# Exit code: 0
```

If compilation succeeds, you'll see: **Exit code: 0**

### Step 5: Upload Firmware

```bash
# Build and upload to device
platformio run --target upload --environment seeed_xiao_esp32c3 --upload-port COM5

# Replace COM5 with your actual port!
# Expected output:
# Uploading .pio/build/seeed_xiao_esp32c3/firmware.bin
# esptool.py v4.x
# Hash of data verified.
# Exit code: 0
```

### Step 6: Monitor Serial Output (First Boot)

Keep the device connected and open the serial monitor:

```bash
# Monitor serial output
platformio device monitor --environment seeed_xiao_esp32c3 --port COM5 --baud 115200

# Replace COM5 with your port!
```

You should see boot output:
```
[BOOT] ESP32MUD v1.1.0 starting...
[SETUP] LittleFS initialized
[SETUP] Waiting for WiFi credentials...
```

### Step 7: Provision WiFi Credentials (First Boot Only)

The device is now waiting for WiFi credentials. In the serial monitor, you'll see:

```
[SETUP] Enter WiFi SSID:
```

1. Type your WiFi network name (SSID) and press Enter
2. You'll see: `[SETUP] Enter WiFi Password:`
3. Type your WiFi password and press Enter
4. Device saves credentials to `/credentials.txt` on LittleFS

Expected success message:
```
[WIFI] Connected to MyNetwork
[WIFI] IP Address: 192.168.1.42
[TIME] Syncing time from NTP servers...
[TIME] Time synced: 2026-01-20 14:32:15
[SETUP] Ready for telnet on port 23
```

**Note**: Credentials are saved permanently; you won't be prompted again unless you reboot in provisioning mode.

### Step 8: Find Your Device IP Address

From the serial monitor output, note your device's IP address. In the example above, it's `192.168.1.42`.

Alternatively, find it on your router's device list or use:
```bash
# Linux/macOS - scan for ESP32 devices
nmap -p 23 192.168.1.0/24

# Windows PowerShell
Test-NetConnection -ComputerName 192.168.1.42 -Port 23
```

### Step 9: Connect via Telnet

Now you can connect to your MUD!

```bash
# Connect via telnet
telnet 192.168.1.42 23

# You should see:
# Connected to 192.168.1.42.
# Escape character is '^]'.
# Welcome to Esperthertu!
# Enter your name: _
```

### Step 10: Create Your First Character

At the login prompt:

```
Enter your name: Atew
Enter password: (type something)
Confirm password: (type same thing)
Choose your race (Human/Elf/Dwarf): Human
Welcome, Atew the Human!
You stand in a bustling town square...
```

Type `LOOK` to see your surroundings:
```
> look
You stand in a bustling town square. Merchants hawk their wares.
Exits: N S E W NE NW SE SW U D
Items here: torch, rope
NPCs here: Guard, Merchant
```

**Congratulations! You're now in Esperthertu!**

## Essential First Commands

```
LOOK            View current room
NORTH / N       Move north
INVENTORY / I   Check your items
SAY hello       Talk to others
SCORE           View character stats
SHOPS           List nearby shops
```

## Uploading Data Files

After initial setup, you can customize the MUD world by uploading data files:

```bash
# Upload all data files (rooms, items, NPCs, quests) from data/ folder
pio run --target uploadfs --environment seeed_xiao_esp32c3

# This syncs your data/ folder to device LittleFS
```

## Adding Item Definitions

To add items to shops (e.g., torch, rope, waterskin):

1. Edit `data/items.vxd`:
```
torch|misc|name=Torch|type=misc|desc=A wooden torch|weight=2|can_contain=0|stackable=1|value=5
rope|misc|name=Rope|type=misc|desc=A length of sturdy rope|weight=3|can_contain=0|stackable=1|value=8
```

2. Upload data files:
```bash
pio run --target uploadfs --environment seeed_xiao_esp32c3
```

3. Reconnect and test:
```
telnet 192.168.1.42 23
> go to shop
> buy torch
> inventory
```

## Troubleshooting

### Can't Connect to Telnet
**Problem**: `telnet: Unable to connect to remote host`

**Solutions**:
1. Verify device is connected to WiFi:
   - Check serial monitor for `[WIFI] Connected` message
2. Verify correct IP address:
   - Check `[WIFI] IP Address: xxx.xxx.xxx.xxx` in serial monitor
3. Ensure firewall allows port 23:
   - Windows: Check Windows Defender Firewall
   - macOS: System Preferences → Security & Privacy
4. Try pinging device:
   - `ping 192.168.1.42`

### Compilation Fails
**Problem**: `error: undefined reference to...`

**Solutions**:
1. Ensure all dependencies are installed:
   ```bash
   pio lib install
   ```
2. Check platformio.ini is in project root
3. Clean build:
   ```bash
   pio run --target clean --environment seeed_xiao_esp32c3
   pio run --environment seeed_xiao_esp32c3
   ```

### WiFi Won't Connect
**Problem**: Serial shows `[WIFI] Connection failed`

**Solutions**:
1. Verify credentials are correct (case-sensitive password)
2. Check WiFi network is 2.4GHz (ESP32 doesn't support 5GHz)
3. Check network has open DHCP (fixed IPs may not work)
4. Reset credentials by uploading empty credentials.txt:
   - Create `data/credentials.txt` with just blank lines
   - Run `pio run --target uploadfs`
   - Device will re-prompt for WiFi on next boot

### NTP Time Sync Fails
**Problem**: Serial shows `[TIME] Failed to sync time from NTP`

**Solutions**:
1. Verify WiFi is connected (runs after WiFi connection)
2. Check router allows UDP port 123 (NTP)
3. Try public WiFi (some networks block NTP)
4. Device will still function; just with incorrect timestamps

### Can't Find Device on Network
**Problem**: Can't determine IP address

**Solutions**:
1. Check router DHCP lease list (usually at 192.168.1.1)
2. Look for "ESP32" or device name in connected devices
3. Use network scanner:
   - Windows: Advanced IP Scanner
   - Linux: `sudo nmap -sn 192.168.1.0/24`

## Next Steps

Once connected:

1. **Read [QUICK_COMMANDS.md](QUICK_COMMANDS.md)** - Learn all game commands
2. **Explore the world** - Use N/S/E/W/NE/NW/SE/SW/U/D to navigate
3. **Visit shops** - Buy equipment and items
4. **Fight NPCs** - Learn combat system
5. **Accept quests** - Complete objectives for rewards
6. **Become a wizard** (if whitelisted) - Get admin commands

## Wizard Setup (Optional)

If you want wizard privileges:

1. Add your character name to `wizardExceptions` in `src/ESP32MUD.cpp`:
   ```cpp
   const char* wizardExceptions[] = {"Atew", "Ploi", "Dogbowl", "YourName"};
   ```

2. Rebuild and upload:
   ```bash
   pio run --target upload --environment seeed_xiao_esp32c3
   ```

3. Log in as that character - you'll automatically have wizard mode

## Performance Tuning

If you experience lag:

1. **Reduce item instances** - Fewer items = less memory use
2. **Limit active NPCs** - Set NPC spawn count lower
3. **Monitor flashspace** - Use `debug flashspace` command
4. **Archive session logs** - Compress `/session_log.txt` manually

## Documentation

For more information:

- **[QUICK_COMMANDS.md](QUICK_COMMANDS.md)** - All player/wizard commands
- **[ESP32MUD_SYSTEM_REFERENCE.md](ESP32MUD_SYSTEM_REFERENCE.md)** - Deep technical reference
- **[FILE_INDEX.md](FILE_INDEX.md)** - Data file formats
- **[COMPLETION_SUMMARY.txt](COMPLETION_SUMMARY.txt)** - Feature checklist

## Support

For issues:

1. Check relevant documentation above
2. Review error messages in serial monitor
3. Check GitHub issues: https://github.com/Veramacor/ESP32MUD/issues
4. Ensure device is on latest firmware

---

**Ready to explore Esperthertu?** Connect now and enjoy your MUD adventure!

**Happy Questing! ⚔️🗺️🎮**
