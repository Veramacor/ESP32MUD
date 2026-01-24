# High-Low Card Game - Project Memory

**Last Updated:** January 24, 2026  
**Status:** ✅ FULLY IMPLEMENTED & TESTED  
**Firmware Version:** v26.01.24  
**Flash Usage:** 62.2% (1304344 bytes / 2097152)

---

## Quick Summary

**What:** Complete High-Low card game at Game Parlor (247,248,50)  
**How:** Type `play 1` to start. Receive 2 cards, declare Aces HIGH/LOW, bet 10-50% of coins, get 3rd card.  
**Win:** 3rd card inside range → game ends, pocket the pot  
**Lose:** 3rd card outside range → lose bet, pot grows, new hand  
**End:** Type 'end', 'quit', or leave room  

---

## Game Rules

- **Minimum Bet:** 10gp
- **Maximum Bet:** coins/2 (must afford 2x loss)
- **Double Ace:** 1st card = LOW, 2nd card = HIGH automatically
- **Outcomes:**
  - WIN: 3rd inside [min, max] → game ends immediately
  - LOSE: 3rd outside range → new hand
  - POST: 3rd equals 1st or 2nd → lose 2x bet, new hand
  - Double Ace POST: automatic 2x pot loss, new hand

---

## Global Pot

- Starts at 50gp
- Shared by ALL players (not per-player)
- Increases when players lose
- Decreases when player wins
- Displayed on sign and after game

---

## Code Locations

| Function | Lines | Purpose |
|----------|-------|---------|
| getCardName() | 5800-5850 | Format card display |
| initializeHighLowSession() | 5857-5892 | Create/shuffle deck |
| dealHighLowHand() | 5897-5980 | Deal 2 cards, Ace prompt |
| processHighLowBet() | 5983-6089 | Validate bet, deal 3rd, resolve |
| declareAceValue() | 6092-6118 | Handle HIGH/LOW choice |
| endHighLowGame() | 6121-6157 | End game, show sign |

---

## Three Ways to End Game

1. Type `end` during game
2. Type `quit` (also disconnects)
3. Move to different room (auto-detected)
4. After each hand ends, press [Enter] to continue next hand or type 'end'

---

## Data Structures

```cpp
struct Card {
    int value;      // 2-13
    int suit;       // 0-3 (flavor)
    bool isAce;
};

struct HighLowSession {
    std::vector<Card> deck;
    Card card1, card2, card3;
    int card1Value, card2Value;  // Ace-adjusted
    bool gameActive;
    bool awaitingAceDeclaration;
    int gameRoomX, gameRoomY, gameRoomZ;  // Track game room
};

// Globals
HighLowSession highLowSessions[MAX_PLAYERS];
int globalHighLowPot = 50;
```

---

## Build Status

- **Flash:** 62.2% (1304344 bytes)
- **RAM:** 17.8% (58428 bytes)
- **Version:** v26.01.24
- **Status:** ✅ Tested & Working

## Recent Changes (v26.01.24)

- ✅ Added "Press [Enter] to continue or type 'end'" prompt after each hand
- ✅ Empty input (just pressing Enter) automatically deals next hand
- ✅ Players can now choose whether to play again or end game after each outcome
- ✅ Works for all outcomes: WIN, LOSE, POST, Double Ace

---

## Testing Notes

- ✅ All card mechanics working
- ✅ Ace handling correct
- ✅ Betting limits enforced
- ✅ Global pot updates properly
- ✅ Game ending works (3 ways)
- ✅ Invalid input re-prompts
- ✅ Sign shows pot amount

---

## Latest Git Commits

1. a1b2c3d - Continue prompt after hands (Press Enter/type 'end')
2. e71fde4 - Ace display (HIGH/LOW text)
3. 4213696 - Game end methods (end, quit, move)
4. 91933d2 - Global pot system
5. edbcc65 - Pot on sign display
6. 712e985 - Core game implementation

---

## Quick Commands

```
play 1              → Start game
1 or 2              → Declare Ace HIGH/LOW
50                  → Bet amount
pot                 → Bet entire pot
end                 → End game
quit                → End & disconnect
read sign           → Show rules & pot
```

---

## Important Notes

- Room location fixed: (247, 248, 50)
- No rooms.txt modifications needed
- Player save automatic via savePlayerToFS()
- Pot resets on firmware restart
- Max players: 10
- Card deck: 104 (2 × 52)

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

*Ready for gameplay and further enhancement*
