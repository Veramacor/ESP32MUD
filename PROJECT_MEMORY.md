# High-Low Card Game - Project Memory

**Last Updated:** January 23, 2026  
**Status:** ✅ FULLY IMPLEMENTED & TESTED  
**Firmware Version:** v26.01.23  
**Flash Usage:** 62.1% (1301568 bytes / 2097152)

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

- **Flash:** 62.1% (1301568 bytes)
- **RAM:** 17.8% (58428 bytes)
- **Version:** v26.01.23
- **Status:** ✅ Tested & Working

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

1. e71fde4 - Ace display (HIGH/LOW text)
2. 4213696 - Game end methods (end, quit, move)
3. 91933d2 - Global pot system
4. edbcc65 - Pot on sign display
5. 712e985 - Core game implementation

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

*Ready for gameplay and further enhancement*
