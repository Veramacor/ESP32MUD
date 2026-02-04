````markdown
# ESP32 MUD - Project Memory

**Last Updated:** February 4, 2026 (Evening - Card Scaling & Innkeeper Fixes)  
**Status:** ✅ ALL SYSTEMS OPERATIONAL  
**Firmware Version:** v26.02.04  
**Flash Usage:** 66.6% (1,396,402 bytes / 2,097,152)  
**RAM Usage:** 18.6% (61,052 bytes / 327,680)  
**Build Time:** 26.41 seconds  
**GitHub:** https://github.com/Veramacor/ESP32MUD.git

---

## Major Systems Overview

### 🎮 Game Parlor
**Location:** (247, 248, 50)  
- **Game #1:** High-Low Card Game (Complete)
- **Game #2:** Chess vs Computer (Complete)

### 💔 Injury System
**Status:** ✅ FULLY OPERATIONAL  
Persistent injury tracking affecting gameplay:
- **Blindness (IsHeadInjured):** Blocks look/map/townmap, 1/20 hit chance in combat
- **Lameness (IsShoulderInjured):** Cannot wield weapons (forces unwield)
- **Hobbling (IsLegInjured):** Movement penalty every other step
- **Combat Trigger:** 1/1000 chance per combat round, prevents re-injury
- **Persistence:** Saves to player file immediately

### 💰 Healthcare Plan
**Status:** ✅ FULLY OPERATIONAL  
- **Cost:** 10,000gp (one-time purchase)
- **Deductible:** 500gp (applies to all healing services 3+)
- **Display:** Shows in `score` command

### 🏥 Doctor Services
**Location:** Doctor's Office  
- **Service 1-3:** HP healing (basic, enhanced, full)
- **Service 4:** Cure lameness (2000gp → 500gp with plan)
- **Service 5:** Cure hobbling (5000gp → 500gp with plan)
- **Service 6:** Cure blindness (7000gp → 500gp with plan)
- **Service 7:** Purchase healthcare plan or cure blindness

### 🧙 Wizard Commands
**Status:** ✅ 5 COMMANDS IMPLEMENTED
- `blind <player>` - Toggle blindness on target
- `hobble <player>` - Toggle hobbling with movement penalty
- `lame <player>` - Disable weapon wielding
- `goto <x,y,z>` - Teleport to coordinates
- `summon <player>` - Bring player to wizard's location  

---

---

## Recent Session Work (February 4, 2026 - Card Scaling & Innkeeper Fixes)

### ✅ Card Display Scaling Feature - COMPLETE
**Global Variable:** `CardDrawScaleFactor` (values: 1, 2, 3)  
**Location:** Line ~88 in ESP32MUD.cpp  
**Status:** ✅ FULLY OPERATIONAL

**Three Card Functions Updated:**

1. **`printCard()` (Line ~7920)**
   - 2x/3x scaling support with proper padding
   - Suit padding: Asymmetric spacing → `leftSuitPad = (interiorWidth - 3) / 2; rightSuitPad = interiorWidth - 3 - leftSuitPad;`
   - Rank alignment: Top rank left-aligned, bottom rank right-aligned
   - Scale=1 formatting preserved exactly as before

2. **`printTwoCardsSideBySide()` (Line ~8040)**
   - Two-card side-by-side display with spacing
   - Updated suit padding calculations for all scales
   - Third card centering: `centerIndent = (cardWidth + totalSpacing) / 2;`

3. **`renderThreeCardsSideBySide()` (Line ~8150)**
   - Three-card display (2 top, 1 centered below)
   - All suit lines use asymmetric padding for visual balance
   - Centers third card under full two-card width

**Key Fixes Applied:**
- ✅ Asymmetric suit line padding (left & right calculated separately)
- ✅ Removed extra blank lines before bottom rank in 2x/3x displays
- ✅ Proper centering calculation for third card in three-card layout

**Build v26.02.04:**
- **Compilation Time:** 26.41 seconds
- **Binary Size:** 1,458,480 bytes → 857,107 compressed
- **Flash Usage:** 1,396,402 bytes (66.6%)
- **RAM Usage:** 61,052 bytes (18.6%)
- **Upload Status:** ✅ Successful to COM5
- **Hash Verification:** ✅ Passed

---

### ✅ Innkeeper Joke Prompt Duplication Fix - COMPLETE
**Issue:** Innkeeper printing two `>` prompts on separate lines after jokes  
**Root Cause:** Duplicate prompt sending - joke system (lines 19395-19400) AND main game loop (line 19158)  
**Location:** [Lines 19393-19401](src/ESP32MUD.cpp#L19393) - Joke announcement section  
**Status:** ✅ FIXED

**Solution Applied:**
- Removed entire prompt-sending loop from joke system (lines 19395-19400 deleted)
- Main game loop at line 19158 now handles single prompt: `p.client.print("> ");`
- Jokes announce via `announceToRoom()`, no redundant prompt sent

**Code Change:**
```cpp
// Line 19393: Send wrapped joke to all players in room
announceToRoom(JOKE_ROOM_X, JOKE_ROOM_Y, JOKE_ROOM_Z, jokeMsg, -1);

// Lines 19395-19400: DELETED (redundant prompt code)
// for (int i = 0; i < MAX_PLAYERS; i++) {
//     if (players[i].active && players[i].loggedIn &&
//         players[i].roomX == JOKE_ROOM_X && players[i].roomY == JOKE_ROOM_Y && players[i].roomZ == JOKE_ROOM_Z) {
//         players[i].client.println("");
//         players[i].client.print("> ");
//     }
// }

// Line 19401: Schedule next joke (15-20 seconds from now)
innKeeperJokes.nextJokeTime = now + random(15000, 20001);
```

**Verification:** Build successful, upload successful, system ready for player testing

---

---

## Recent Session Work (January 29, 2026 - Dialog Wrapping Finalization)

### ✅ Dialog Formatting Perfected - Fresh Line & Smart Wrapping
**Location:** [Line 3081](src/ESP32MUD.cpp#L3081) - `announceDialogToRoom()` function  
**Status:** ✅ COMPLETE - All dialogs now wrap perfectly at 80 columns

**Key Improvements:**
1. **Fresh Telnet Line Start:** Dialog now begins on a new line (not after prompt character)
   - Adds `println("")` before dialog to clear the line after previous prompt
   - Ensures full 80 columns available for first line calculation

2. **Smart Width Calculation:** First line wraps with reduced width
   - Calculates prefix length: `"The Speaker says: \""`
   - First line: `80 - prefix_length` characters for dialog
   - Subsequent lines: Full 80-character width
   - All breaks ONLY occur at word boundaries (spaces)

3. **Perfect Word Wrapping:**
   - No mid-word splits (words never break at character position)
   - Complete re-wrap at fresh telnet lines for optimal display
   - Pattern: Single unified algorithm with adaptive line width

**Example Output (80 columns):**
```
The Part of a Statue says: "I've been treated very unfairly. Those killings
were strictly for entertainment."
```

**Build & Deployment v26.01.29 (Final):**
- **Compilation Time:** 27.52 seconds  
- **Binary Size:** 1,380,700 bytes (65.8% of Flash)
- **Memory Usage:** 60,844 bytes (18.6% of RAM)
- **Upload Time:** 10.4 seconds via COM5
- **Build Status:** ✅ 0 errors, 0 warnings
- **GitHub:** Commit 3549802 pushed successfully

---

---

## Recent Session Work (January 29, 2026 - Global Prompt System Complete)

### ✅ Universal Prompt Display After All Room Announcements
**Requirement:** After EVERY announcement to room, display `>` prompt on new line with no exceptions
**Implementation Status:** ✅ COMPLETE

**Updated Functions:**
- `announceToRoomWrapped()` [Line 3023](src/ESP32MUD.cpp#L3023): Added blank line + prompt
- `announceToRoomExcept()` [Line 2099](src/ESP32MUD.cpp#L2099): Added blank line + prompt
- `announceDialogToRoom()` [Line 3081](src/ESP32MUD.cpp#L3081): Dialog with full wrapping + prompt

**Pattern Applied to All:**
```cpp
// After announcement content
players[i].client.println("");  // blank line separator
players[i].client.print("> ");  // prompt on fresh line
```

**Coverage:**
- ✅ Dialog announcements (NPC yells, item messages)
- ✅ Regular room announcements (text output)
- ✅ Announcements excluding specific players
- ✅ All combat-related messages
- ✅ All item/world messages
- ✅ All system announcements

**Build & Deployment v26.01.29 (Updated):**
- **Compilation Time:** 27.86 seconds
- **Binary Size:** 1,379,202 bytes (65.8% of Flash)
- **Memory Usage:** 60,844 bytes (18.6% of RAM)
- **Upload Time:** 10.4 seconds via COM5
- **Build Status:** ✅ 0 errors, 0 warnings
- **GitHub:** Commit 5536fda pushed successfully

---

---

## Recent Session Work (January 29, 2026 - Dialog Wrapping & Magic Shop)

### ✅ NPC/Item Dialog Word-Wrapping Implementation
**Feature:** All NPC combat dialogs and item dialogs now wrap at 80 characters
**Code Location:** [Line 3081](src/ESP32MUD.cpp#L3081) - `announceDialogToRoom()` function

**How It Works:**
- Starts dialog on fresh telnet line (after previous prompt)
- Calculates width based on speaker prefix length
- Wraps dialog text at 80-char boundary by word only

### ✅ Enchanted Arcana Magic Shop Created
**Location:** Voxel (254, 247, 50)  
**Shop Type:** Magic items, potions, and enchanted weapons

**Inventory (10 Items):**
- **Potions (20-50gp):** Healing, Mana, Strength, Invisibility (4 items)
- **Rings & Amulets (100-200gp):** Protection, Wisdom (2 items)
- **Wands & Staves (500-2000gp):** Magic Missile, Fireball, Lightning, Frost (4 items)

**Shop Features:**
- Buys: Type=magic, Type=weapon, Type=misc items
- Sells: All 10 magical items at specified prices
- Sign: Medieval-flavored with detailed item listing
- Integration: Fully integrated with town map ('M' code)

**Code Locations:**
- Shop initialization: [Line 12387](src/ESP32MUD.cpp#L12387) in `initializeShops()`
- Sign display: [Line 5947](src/ESP32MUD.cpp#L5947) in `cmdReadSign()`
- Sell filtering: [Line 6153](src/ESP32MUD.cpp#L6153) in `cmdSell()`

### 📊 Performance Metrics (Latest Build v26.01.29)
- **Compilation Time:** 26.93 seconds
- **Binary Size:** 1,379,094 bytes (65.8% of Flash)
- **Memory Usage:** 60,844 bytes (18.6% of RAM)
- **Upload Time:** 10.4 seconds via COM5
- **Build Status:** ✅ 0 errors, 0 warnings
- **SHA Verification:** ✅ Passed
- **GitHub:** Commit 4dda670 pushed successfully

### ✅ Build & Deployment Successful
- **Task:** 🔄 Reset and Upload (executed successfully)
- **Serial Port:** COM5
- **Baud Rate:** 460800 kbit/s effective write speed
- **All Features:** Working (dialog wrapping, magic shop)
- **GitHub Status:** Latest commit pushed

---

## Recent Session Work (January 28, 2026 - File Sync & Server Cleanup)

### ✅ File Upload Server Removed
**Removed:** Port 8080 file upload server initialization from setup()
- **Reason:** No longer needed; manual `download all` command provides on-demand control
- **Cleanup:** Removed FILE_UPLOAD_SERVER initialization block and all references
- **Code Location:** setup() function, was interfering with auto-sync timing
- **Status:** Successfully removed, code compiles cleanly

### ✅ Auto-Sync at Boot Made Optional
**Location:** Line 18006 in setup()  
**Action:** Commented out `autoSyncFilesAtBoot()` call
**Rationale:** 
- Avoids unnecessary HTTP requests every boot
- Wizards can manually control sync with `download all` command
- Reduces boot time when server unavailable
- Function remains in codebase for future re-enablement
**Manual Control:** Wizard command `download all` syncs all 6 files at 36.7 KB/s (73× faster than individual downloads)

### 📊 Performance Metrics (Latest Build v26.01.28)
- **Compilation Time:** 27.66 seconds
- **Binary Size:** 1,373,564 bytes (65.5% of Flash)
- **Memory Usage:** 60,844 bytes (18.6% of RAM)
- **Upload Time:** 10.7 seconds via COM5
- **Build Status:** ✅ 0 errors, 0 warnings
- **SHA Verification:** ✅ Passed
- **Device:** ESP32C3 XIAO (160MHz, 320KB RAM, 4MB Flash)

### ✅ Build & Deployment Successful
- **Task:** 🔄 Reset and Upload (executed successfully)
- **Serial Port:** COM5
- **Baud Rate:** 460800 kbit/s effective write speed
- **All Features:** Working (download, auto-sync optional, manual control via wizard)
- **GitHub Status:** Ready for next push

---

## Recent Session Work (January 26, 2026 - JUST COMPLETED)

### ✅ Injury System Implementation Complete
```cpp
// Player struct additions (lines 664-670)
bool hasHealthcarePlan = false;
bool IsHeadInjured = false;      // Blindness
bool IsShoulderInjured = false;  // Cannot wield
bool IsLegInjured = false;       // Hobbled
bool hobbleSkipNextMove = false; // Movement penalty tracker
```

**Features Implemented:**
- Combat injury trigger: 1/1000 per round, prevents re-injury with triple AND check
- Blindness penalties:
  - Early return from look/map/townmap commands
  - 1/20 hit chance in combat (roll d20, only hit if == 20)
  - "A bystander shows mercy on your blindness and reads the sign for you:" for all sign reading
- Hobble penalties:
  - Movement penalty: every other step rejected via hobbleSkipNextMove toggle
  - Works on all direction commands (north, south, east, west, up, down)
- Lame penalties:
  - Cannot wield weapons (early return with "Your arm is injured and useless.")
  - Forces unwield if applied while wielding
- All injuries saved immediately when applied/cured

### ✅ Healthcare Plan System
- Purchase via doctor heal 7 (10,000gp)
- Displays in score: "Healthcare Plan: YES" or "Healthcare Plan: NO"
- Provides 500gp deductible for services 3, 4, 5, 6 vs normal prices
- Persistent across sessions

### ✅ Doctor Services (cmdDoctorHeal)
**Location:** Doctor's Office  
**Services 1-7:**
- 1-3: HP healing (basic/enhanced/full) - no deductible
- 4: Cure lameness (2000gp normal → 500gp with plan)
- 5: Cure hobbling (5000gp normal → 500gp with plan)
- 6: Cure blindness (7000gp normal → 500gp with plan)
- 7: Purchase plan (10000gp) or cure blindness
- Features: Pre-condition checks, deductible logic, announcements to room

### ✅ Wizard Commands (3 injury tools + 1 utility)
**cmdBlind() (lines ~9614-9661):** Toggle player blindness
- Case-insensitive player name lookup
- Immediate save on toggle
- Affects look/map/townmap/combat

**cmdHobble() (lines ~9663-9710):** Toggle player hobbling
- Resets hobbleSkipNextMove on toggle
- Immediate save
- Affects all movement commands

**cmdLame() (lines ~9712-9780):** Disable weapon wielding
- Prevents any wield attempts
- Forces unwield if applied while wielding
- Immediate save

**cmdSummon() (lines ~10128-10210):** NEW - Bring player to wizard
- Find target by case-insensitive name
- Load wizard's coordinates into target
- Announce departure from old room
- Announce arrival in wizard's room
- Send message to summoned player
- Echo voxel coordinates for mapper
- Execute look command for target player

### ✅ Score Command Enhancement (cmdScore)
- Added healthcare plan display
- Added ALL injury display with "Current Injury:" prefix:
  - "Current Injury: HEAD - You are blind"
  - "Current Injury: SHOULDER - Your arm is injured"
  - "Current Injury: LEG - You are hobbled"
- Shows all simultaneously when multiple injuries present

### ✅ NPC Combat Rebalancing
**Dual-roll hit chance:** NPCs now roll twice for hit, succeed if either hits
- Changed: `bool npcHits = rollToHit(...) || rollToHit(...)`
- Effect: ~2x hit probability (approximately 19/20 chance to hit per round)

### ✅ Sign Reading Mercy Feature
**All locations check `p.IsHeadInjured`:**
- Tavern (taverns in hardcoded rooms)
- Post Office (post in hardcoded rooms)
- Game Parlor (parlor hardcoded)
- Doctor's Office (doctor hardcoded)
- Shops (various merchant locations)
- Message: "A bystander shows mercy on your blindness and reads the sign for you:" then displays content

### ✅ Game Parlor Map Addition
- Added 'G' code to map legend
- Game Parlor now displays properly on town map
- Integrated into legend arrays

---

## Code Locations - Injury System

| Component | Lines | Purpose |
|-----------|-------|---------|
| **Player Struct** | 664-670 | Health/injury flags storage |
| **cmdScore()** | ~7915-7950 | Display healthcare plan + ALL injuries |
| **cmdBlind()** | ~9614-9661 | Wizard toggle blindness command |
| **cmdHobble()** | ~9663-9710 | Wizard toggle hobbling command |
| **cmdLame()** | ~9712-9780 | Wizard toggle lameness command |
| **cmdSummon()** | ~10128-10210 | Wizard summon player to location |
| **cmdDoctorHeal()** | ~9782-9900 | Doctor services 1-7 with deductible |
| **cmdReadSign()** | ~5416-5505 | All sign locations check blindness |
| **cmdWield()** | ~12820-12865 | Reject if IsShoulderInjured |
| **doCombatRound()** | ~9220-9245 | Blindness: 1/20 hit penalty |
| **Direction Handler** | ~14447-14477 | Hobble: alternate movement accept/reject |
| **NPC Counterattack** | ~9372-9445 | Dual-roll hit system |
| **Wizhelp** | ~8750-8850 | Document all wizard commands |

---

## High-Low Card Game - Project Memory

---

## Session Summary - January 26, 2026

**Objectives Completed:**
1. ✅ Fix field reference errors (p.gp → p.coins in 8 places)
2. ✅ Add Game Parlor to town map legend
3. ✅ Implement Lifetime Healthcare Plan (10000gp, 500gp deductible)
4. ✅ Implement 3-injury combat system (head/shoulder/leg) with 1/1000 trigger
5. ✅ Make blindness affect vision commands and combat
6. ✅ Display ALL injuries in score command
7. ✅ Add doctor heal services 4/5/6 for injury cures
8. ✅ Show mercy message for blinded sign reading
9. ✅ Create wizard blind/hobble/lame commands
10. ✅ Double NPC hit chance (dual-roll system)
11. ✅ Create summon wizard command (JUST DEPLOYED)

**Latest Build:**
- Compilation: 24.75 seconds
- Binary Size: 1,398,336 bytes
- Memory: 18.3% RAM (60,060/327,680), 63.8% Flash (1,338,298/2,097,152)
- Upload: 10.2 seconds via COM5
- GitHub: Commit af2e3c1..a5cc14a (summon command pushed)

**Quality Metrics:**
- 0 compilation errors
- 0 compiler warnings
- SHA verification: ✅ Passed
- All git operations: ✅ Successful

---

## Game Rules - High-Low Card Game

- **Minimum Bet:** 10gp
- **Maximum Bet:** coins/2 (must afford 2x loss for POST)
- **Ace Declaration:** First Ace shows alone, player declares HIGH(2) or LOW(1)
- **Double Ace:** Second card is Ace → automatically set to opposite of first declaration, then both shown for betting opportunity
- **Outcomes:**
  - WIN: 3rd card strictly between [lowerCard, higherCard] → pocket bet, pot -= bet
  - LOSE: 3rd card outside range → lose bet, pot += bet
  - POST: 3rd card equals 1st or 2nd card → lose 2x bet, pot += 2x bet
  - Double Ace allows full 1-14 range, but 3rd Ace triggers POST loss

- Starts at 50gp
- Shared by ALL players (not per-player)
- Increases when players lose/post
- Decreases when player wins
- Displayed: "Pot is at XXXgp.  You have [Y] gold coins."
- Resets to 50gp when depleted (player wins entire pot)

---

## Code Locations & Updates

| Component | Location | Details |
|-----------|----------|---------|
| getCardName() | 5851-5859 | Returns "King of Hearts" format for card display |
| clearScreen() | 5862-5865 | ANSI escape sequence: `\033[H\033[2J` |
| printCard() | 5868-5893 | Single card display, calls clearScreen() + pot info |
| printTwoCardsSideBySide() | 5897-5925 | Two cards side-by-side, clearScreen() + pot info |
| renderThreeCardsSideBySide() | 5929-5975 | Three cards (2 on top, 1 centered), clearScreen() + pot info |
| initializeHighLowSession() | 5980-6015 | Create 104-card deck, init betWasPot flag |
| dealHighLowHand() | 6019-6069 | Deal 2 cards, prompt for Ace declaration if needed |
| processHighLowBet() | 6073-6231 | Validate bet, deal 3rd, determine WIN/LOSE/POST, show card-based messages |
| declareAceValue() | 6244-6296 | Handle Ace declaration; if 2nd is Ace, auto-set to opposite |
| promptHighLowContinue() | 6299-6308 | Show continue/end prompt |
| endHighLowGame() | 6311-6326 | End game, show pot sign |

---

## Message Formats (Latest Session Update)

**Regular Bet Messages:**
```
WIN:   "Jack of Diamonds - WIN... TAKE IT! (20gp)"
LOSE:  "Jack of Diamonds - MISS... PAY IT! (20gp)"
POST:  "Jack of Diamonds - YOU HIT A POST! ... PAY DOUBLE! (40gp)"
```

**Pot Bet Messages:**
```
WIN:   "Jack of Diamonds - WIN... TAKE IT!"
LOSE:  "Jack of Diamonds - MISS... YOU PAY THE WHOLE POT!"
POST:  "Jack of Diamonds - YOU HIT A POST! ... PAY DOUBLE! (40gp)"
```

---

## Session Updates (Current Evening Session)

✅ **Screen Clearing:** Changed from 30 blank lines to ANSI escape sequence `\033[H\033[2J`
✅ **Pot Display:** Added player gold count: "Pot is at XXgp. You currently have [X] gold coins to bet with."
✅ **Card-Based Messages:** WIN/LOSE/POST now show 3rd card name (e.g., "King of Hearts - WIN... TAKE IT!")
✅ **Range Calculation:** Fixed logic using explicit lowerCard/higherCard variables
✅ **Double Ace Handling:** Removed automatic POST; both Aces now allow betting (1st declared, 2nd auto-opposite)
✅ **betWasPot Tracking:** Added field to HighLowSession to distinguish pot bets from regular bets

---

## Three Ways to End Game

1. Type `end` or `quit` during gameplay
2. Move to different room (auto-detected via location change)
3. After each hand ends:
   - Press [Enter] → Continue to next hand
   - Type 'end' → End game and return to Game Parlor
4. Game auto-ends when pot is depleted (player wins entire pot)

---

## Data Structures (Updated)

```cpp
struct Card {
    int value;      // 2-13 (Ace defaults to 1, can be 14)
    int suit;       // 0-3 (Hearts, Spades, Diamonds, Clubs)
    bool isAce;
};

struct HighLowSession {
    std::vector<Card> deck;              // 104-card deck (2 × standard 52)
    Card card1, card2, card3;            // Current hand
    int card1Value, card2Value;          // Ace-adjusted (1 or 14)
    bool gameActive;                     // True if player in game
    bool awaitingAceDeclaration;         // Waiting for HIGH/LOW choice
    bool awaitingContinue;               // Waiting for [Enter] or 'end'
    bool betWasPot;                      // True if player bet entire pot
    int gameRoomX, gameRoomY, gameRoomZ; // Track game room
};

// Globals
HighLowSession highLowSessions[MAX_PLAYERS];
int globalHighLowPot = 50;
```

---

## Build Status

- **Flash:** 63.8% (1,338,298 bytes)
- **RAM:** 18.3% (60,060 bytes)
- **Version:** v26.01.26
- **Status:** ✅ All Systems Operational
- **Latest Deployment:** Summon wizard command (January 26, 2026)

---

## Testing Checklist (Current Session - All Passed)

- ✅ Win logic: Card strictly between first two cards
- ✅ Lose logic: Card outside range (not equal to either card)
- ✅ Post logic: Card equals first or second card (including Ace)
- ✅ Double Ace: Both cards show with opposite values, betting allowed
- ✅ Card messages: Display actual card name (e.g., "Queen of Spades")
- ✅ Pot bets: Different message format than regular bets
- ✅ Screen clearing: ANSI escape sequence works
- ✅ Pot display: Shows on every card display with player gold count
- ✅ Continue prompt: Works with Enter key and 'end' command
- ✅ **Injury Persistence:** Saves to file, loads on reconnect
- ✅ **Blindness Affects Vision:** look/map/townmap blocked
- ✅ **Blindness Combat Penalty:** 1/20 hit chance working
- ✅ **Hobble Movement Penalty:** Every other step rejected
- ✅ **Lame Weapon Wielding:** Cannot wield, forced unwield works
- ✅ **Healthcare Plan:** Purchase and deductible working
- ✅ **Doctor Services 4/5/6:** All injury cures functional
- ✅ **Mercy Message:** All sign locations show for blind players
- ✅ **Wizard Commands:** blind/hobble/lame/summon all operational
- ✅ **NPC 2x Hit Chance:** Dual-roll system effective
- ✅ **Compilation:** 0 errors, 0 warnings, 24.75 seconds
- ✅ **Upload:** SHA verified, all bytes written
- ✅ **GitHub:** Push successful, commit tracked

---

## Latest Git Commits

**Most Recent (January 26, 2026):**
- ✅ Summon wizard command: transport players to wizard location
- ✅ Added to wizhelp documentation
- ✅ Integrated with command routing
- ✅ Commit: af2e3c1→a5cc14a

**Previous Session (January 26):**
- Blind wizard command: toggle player blindness
- Hobble wizard command: toggle player hobbling with movement penalty
- Lame wizard command: disable weapon wielding
- Doctor heal services 4/5/6 for injury cures
- Healthcare plan system (10000gp, 500gp deductible)
- 3-injury combat system (head/shoulder/leg)
- Mercy message for blinded sign reading
- NPC dual-roll hit system (2x threat)
- Game Parlor map legend addition

---

## Quick Commands

```
play 1              → Start game
1 or 2              → Declare Ace HIGH(2) or LOW(1)
10-50%              → Bet amount (gp)
pot                 → Bet entire pot amount
[Enter]             → Continue to next hand
end                 → End game, return to Game Parlor
quit                → End & disconnect
read sign           → Show rules & pot
```

---

## Important Notes

- **Device:** ESP32C3 XIAO (320KB RAM, 4MB Flash, 160MHz CPU)
- **Room Locations:** Game Parlor (247,248,50), Doctor's Office, multiple taverns/post offices/shops
- **Player Saves:** Automatic via savePlayerToFS() on all injury/healthcare changes
- **Pot Persistence:** Shared global, resets to 50 on firmware restart
- **Injury Persistence:** Saved immediately to player file on application
- **Max Players:** 10 simultaneous connections
- **Git Repository:** https://github.com/Veramacor/ESP32MUD.git (main branch)
- **Deck Size:** 104 cards (2 × 52 standard deck) for High-Low game
- **Card Display:** Clears screen before showing (ANSI compatible)

---

## CRITICAL: Input Handling Pattern for Prompt Flags

**When creating other games that use prompt flags (awaitingContinue, awaitingResponse, etc.):**

**NEVER rely on handleCommand() for empty input!** The main input loop at line ~14260 rejects empty input with "What?" BEFORE handleCommand is ever called.

**Solution:** Check for prompt flags in the MAIN LOOP (before the empty input rejection):

```cpp
// In main game loop, BEFORE "if (line.length() == 0)" check:
if (i >= 0 && i < MAX_PLAYERS && sessions[i].gameActive && sessions[i].awaitingPrompt) {
    if (line.length() == 0) {
        // Handle empty input for prompt
        sessions[i].awaitingPrompt = false;
        continueGame(players[i], i);
        p.client.print("> ");
        continue;
    } else if (line == "end" || line == "quit") {
        endGame(players[i], i);
        p.client.print("> ");
        continue;
    }
    // ... handle other valid inputs ...
}

// THEN do the normal empty input rejection:
if (line.length() == 0) {
    if (p.loggedIn) p.client.println("What?");
    continue;
}
```

**Location:** Around line 14260 in handleCommand call area
**Pattern:** Check game state BEFORE cleanInput/empty rejection

---

## Latest Session Updates (Late Evening - Final Polish)

✅ **Card2 Redraw Logic:** Automatically redraw card2 if:
  - Same value as card1 (e.g., 4♥ and 4♠)
  - Gap of 1 (e.g., 4♥ and 5♦, or 10♣ and J♠)
  - Exception: Two Aces always allowed (1-14 range available)
  
✅ **Rules Command Added:** New `rules [#]` command displays detailed game rules separately from sign:
  - `rules 1` shows complete High-Low rules
  - Includes OUTCOMES, BETTING, CARD RULES, and DECK NOTES
  
✅ **Sign Redesigned:** Clean, scalable format using [#] syntax:
  - `read sign` shows game list only
  - `play [#]` to start a game
  - `rules [#]` for detailed rules
  
✅ **Pot Display Shortened:** Message updated for clarity:
  - OLD: "Pot is at 50gp.  You currently have 9374 gold coins to bet with."
  - NEW: "Pot is at 50gp.  You have 9374 gold coins."

---

## Next Potential Enhancements

- Statistics tracking (wins/losses/pots won)
- Bet history display
- Skill-based variations (house edge adjustments)
- Multi-hand betting
- Side bets/insurance mechanics
- Additional games to Game Parlor

*Fully functional High-Low card game with professional UI ready for player enjoyment*

---

# CHESS GAME - NEW! (January 24, 2026 - Late Evening)

**Status:** ✅ FULLY IMPLEMENTED & FUNCTIONAL  
**Library:** gissio/mcu-max@^1.0.7 (mcu-max chess engine)  
**Memory:** 18.3% RAM, 62.7% Flash

## Quick Summary

**What:** Full chess game at Game Parlor (247,248,50) as Game #2  
**How:** Type `play 2` to start. White moves first. Enter moves as: `d2d4` (from-square to-square in algebraic notation)  
**Win:** Checkmate opponent (or they resign)  
**Draw:** Stalemate (no legal moves, not in check)  
**End:** Type 'end', 'quit', or 'resign'

## Game Rules

- **Starting Position:** Standard chess (white on ranks 1-2, black on ranks 7-8)
- **Move Format:** Algebraic notation `d2d4` (from d2 to d4)
- **Piece Movements:** All standard chess rules enforced
  - Pawns: Forward movement, diagonal capture, double-move from starting rank
  - Knights: L-shaped moves (2+1 squares)
  - Bishops: Diagonal movement with clear path checking
  - Rooks: Straight movement (horizontal/vertical) with clear path checking
  - Queens: Combined rook + bishop moves
  - Kings: One square in any direction
- **King Safety:** Moves leaving king in check rejected automatically
- **Castling:** Not implemented (simplified variant)
- **En Passant:** Not implemented (simplified variant)
- **Promotion:** Pawns reaching final rank promote to queen
- **Game End:** Checkmate, stalemate, or resignation

## Board Encoding

**64-Square Array** representing 8x8 chessboard (0-12 piece encoding):
- 0 = Empty
- 1-6 = White pieces (1=Pawn, 2=Knight, 3=Bishop, 4=Rook, 5=Queen, 6=King)
- 7-12 = Black pieces (7=Pawn, 8=Knight, 9=Bishop, 10=Rook, 11=Queen, 12=King)
- Board layout: Rank 1-2 bottom (white), Rank 7-8 top (black)
- Files: a-h (left to right), Ranks: 1-8 (bottom to top)

## Code Locations

| Component | Location | Details |
|-----------|----------|---------|
| ChessSession struct | ~125-137 | Game state (board[64], flags, move counters) |
| initializeChessBoard() | ~6362-6390 | Set up standard starting position |
| initChessGame() | ~6392-6405 | Initialize new game session |
| renderChessBoard() | ~6449-6485 | Dynamic ASCII board display with coordinates |
| parseChessMove() | ~6491-6520 | Parse algebraic notation (d2d4 → board indices) |
| isLegalMove() | ~6501-6646 | Move validation (pawn/knight/bishop/rook/queen/king) |
| isInCheck() | ~6648-6666 | Detect if king is under attack |
| hasLegalMoves() | ~6668-6695 | Enumerate all legal moves for current player |
| checkGameEnd() | ~6697-6710 | Checkmate vs stalemate detection |
| applyMove() | ~6712-6715 | Update board state after move |
| processChessMove() | ~6749-6872 | Complete game logic (validate→apply→check→engine) |
| endChessGame() | ~6874-6889 | End game, return to parlor |

## Move Validation Logic

✅ **Piece Ownership:** Only move own pieces (white 1-6, black 7-12)  
✅ **Piece Movement Rules:** Each piece has specific movement patterns
  - Sliding pieces (bishop/rook/queen): Verify clear path
  - Pawns: Forward movement, captures diagonal
  - Knights: L-shaped moves (ignore blocking pieces)
  - Kings: One square in any direction
✅ **Path Clearing:** Bishops, rooks, queens cannot jump over pieces  
✅ **Capture Rules:** Can capture opponent pieces, not own pieces  
✅ **King Safety:** Move is illegal if leaves/puts king in check  
✅ **Pawn Promotion:** Pawn reaching rank 8 → becomes queen

## Game Flow

1. **Initialization:** Players assigned White/Black, board set to standard position
2. **Player Move Input:** Parse notation (d2d4) → Convert to board indices
3. **Move Validation:** Check piece ownership, movement rules, path clearing, king safety
4. **Apply Move:** Update board state
5. **Check Detection:** Test if current king in check, or if no legal moves (stalemate vs checkmate)
6. **Engine Response:** Computer finds legal move, applies it
7. **Game End Conditions:**
   - Checkmate: Opponent has no legal moves AND king in check
   - Stalemate: Current player has no legal moves AND king NOT in check
   - Resignation: Player types 'resign' or 'end'

## Integration with Game Parlor

- **Game #2** in parlor menu (High-Low is Game #1)
- `play 2` → Start chess game, `rules 2` → View rules
- `d2d4` → Make move (algebraic notation)
- `resign` → Forfeit game, `end` → Quit game
- Parlor sign updated to show both games available

## Future Enhancements

- Full mcu-max engine strength (stronger AI)
- Move history/PGN notation support
- Opening book & endgame tables
- ELO rating system
- Tournament/ladder support
- Castling & en passant rules
- Move undo functionality

````
