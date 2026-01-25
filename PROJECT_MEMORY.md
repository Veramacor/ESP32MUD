# High-Low Card Game - Project Memory

**Last Updated:** January 24, 2026 (Late Evening - Rules & Display Updates)  
**Status:** ✅ FULLY IMPLEMENTED & TESTED  
**Firmware Version:** v26.01.24  
**Flash Usage:** 62.4% (1308274 bytes / 2097152)

---

## Quick Summary

**What:** Complete High-Low card game at Game Parlor (247,248,50)  
**How:** Type `play 1` to start. Receive 2 cards, declare Aces HIGH/LOW, bet 10-50% of coins, get 3rd card.  
**Win:** 3rd card strictly between first two → gain bet, pot decreases  
**Lose:** 3rd card outside range → lose bet, pot increases, new hand  
**Post:** 3rd card equals first or second → lose 2x bet, pot increases, new hand  
**End:** Type 'end', 'quit', or leave room; or press [Enter] after outcome  

---

## Game Rules

- **Minimum Bet:** 10gp
- **Maximum Bet:** coins/2 (must afford 2x loss for POST)
- **Ace Declaration:** First Ace shows alone, player declares HIGH(2) or LOW(1)
- **Double Ace:** Second card is Ace → automatically set to opposite of first declaration, then both shown for betting opportunity
- **Outcomes:**
  - WIN: 3rd card strictly between [lowerCard, higherCard] → pocket bet, pot -= bet
  - LOSE: 3rd card outside range → lose bet, pot += bet
  - POST: 3rd card equals 1st or 2nd card → lose 2x bet, pot += 2x bet
  - Double Ace allows full 1-14 range, but 3rd Ace triggers POST loss

---

## Global Pot

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

- **Flash:** 62.3% (1307164 bytes)
- **RAM:** 17.8% (58428 bytes)
- **Version:** v26.01.24
- **Status:** ✅ Tested & Working

---

## Testing Checklist (Current Session)

- ✅ Win logic: Card strictly between first two cards
- ✅ Lose logic: Card outside range (not equal to either card)
- ✅ Post logic: Card equals first or second card (including Ace)
- ✅ Double Ace: Both cards show with opposite values, betting allowed
- ✅ Card messages: Display actual card name (e.g., "Queen of Spades")
- ✅ Pot bets: Different message format than regular bets
- ✅ Screen clearing: ANSI escape sequence works
- ✅ Pot display: Shows on every card display with player gold count
- ✅ Continue prompt: Works with Enter key and 'end' command

---

## Latest Git Commits

Current session:
- Card-based WIN/LOSE/POST messages with card names
- Fixed range calculation (lowerCard/higherCard)
- ANSI screen clearing
- Pot/gold display on card shows
- Fixed double Ace logic (allow betting instead of auto-POST)

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

- **Room Location:** Fixed at (247, 248, 50)
- **Player Saves:** Automatic via savePlayerToFS()
- **Pot Persistence:** Shared global, resets to 50 on firmware restart
- **Max Players:** 10 simultaneous games
- **Deck Size:** 104 cards (2 × 52 standard deck)
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
