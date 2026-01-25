# MUD Activity Monitoring - Quick Reference

## What Was Added

A system that tracks **how long since the player last acted** and uses this to adjust the chess engine's thinking time dynamically.

## The Problem It Solves

- When player is actively making moves, engine shouldn't waste time thinking
- When player goes idle, engine should have maximum time to calculate the best move
- Creates better gameplay flow: fast response during active play, thoughtful responses during idle periods

## How to Use (Player Perspective)

Just play normally! The system works automatically:

1. **During active play** (making moves in quick succession):
   - Engine gets 1-2 seconds to think
   - Responds quickly to keep the game flowing

2. **During idle periods** (thinking before next move):
   - Engine gets 3-5 seconds to think
   - Has time to calculate deeper strategies

## Key Files Modified

**src/ESP32MUD.cpp**

### Lines Added:
1. **Lines 102-133**: New functions for activity monitoring
   - Global variables for tracking
   - Three utility functions

2. **Lines 13857-13866**: Chess room exit detection
   - Checks if player left Game Parlor
   - Auto-ends game if so

3. **Lines 13920-13937**: Chess move detection
   - Identifies if command is a chess move
   - Prevents chess moves from resetting activity timer

4. **Line 7580**: Dynamic engine search time
   - Uses `getEngineThinkingTimeMs()` instead of hardcoded 5000ms

5. **Line 7612**: Dynamic move delay
   - Engine appears to think for the same time it actually searches

6. **Line 7739**: Activity reset on game end
   - Timer resets when exiting chess game

## The Three Key Functions

```cpp
// Get how many milliseconds player has been idle
unsigned long getMUDActivityDuration();

// Reset activity timer (called when player acts)
void resetMUDActivityTimer();

// Calculate engine thinking time (1-5 seconds based on activity)
int getEngineThinkingTimeMs();
```

## Activity Thresholds

```
Player idle duration → Engine thinking time
0-1 seconds         → 1 second
1-2 seconds         → 2 seconds
2-3 seconds         → 3 seconds
3-4 seconds         → 4 seconds
4+ seconds          → 5 seconds
```

## Chess Move vs. Command

**Chess moves** (don't reset timer):
- `e4`, `d5`, `e2e4` (square notation)
- `Nf3`, `Bf5`, `Rh4` (piece notation)

**Commands** (reset timer):
- `board`, `help`, `status` (during game)
- `end`, `quit` (leaving game)
- `n`, `s`, `e`, `w` (movement)
- Any other command

## Testing the Feature

1. Play chess normally - should feel responsive
2. Wait 5 seconds without moving - next engine move should take full 5 seconds
3. Watch for faster responses during active play, slower during idle periods

## Architecture Diagram

```
Player Action
    ↓
handleCommand()
    ↓
Is it a chess move? 
    ├─ YES → Don't reset timer
    └─ NO → Reset timer with resetMUDActivityTimer()
    ↓
processChessMove()
    ↓
Engine needs to think
    ↓
getEngineThinkingTimeMs()
    ↓
Return thinking duration based on getMUDActivityDuration()
    ↓
mcumax_search_best_move(thinkTimeMs, depth)
    ↓
Display move after delay(thinkTimeMs)
```

## Performance Impact

- **Minimal CPU overhead**: Just tracks one timestamp
- **No memory overhead**: Uses 16 bytes of globals
- **No network impact**: All local calculations
- **Improves responsiveness**: Engine adapts to player's pace

## Edge Cases Handled

1. **Game ends normally** → Activity resets
2. **Player leaves room** → Game auto-ends, activity resets
3. **Fast consecutive moves** → Engine gets less time
4. **Long idle periods** → Engine gets full time
5. **Rapid commands** → Timer continuously resets

## Customization

Want different thresholds? Edit `getEngineThinkingTimeMs()`:

```cpp
// Currently: 1s, 2s, 3s, 4s, 5s
// Could be: 500ms, 1s, 2s, 3s, 4s (faster responses)
// Or: 2s, 3s, 4s, 5s, 6s (more thinking)
```

Just modify the return values for different behavior.

## Q&A

**Q: Does this affect High-Low card game?**
A: No, only chess engine thinking time.

**Q: What if a player never makes a move?**
A: Activity timer starts at game beginning, so after 5 seconds idle, engine gets full 5 seconds to think.

**Q: Can chess moves reset the activity timer?**
A: No, intentionally excluded. Chess moves don't reset it; other commands do.

**Q: What happens if game disconnects?**
A: Timer state is lost (resets when player reconnects/logs in again).

**Q: Can I see activity duration in real-time?**
A: Not exposed to players, but could add debug command if needed.
