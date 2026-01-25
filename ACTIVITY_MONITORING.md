# MUD Activity Monitoring System

## Overview
The MUD activity monitoring system tracks how long it's been since the player's last action. This information is used to dynamically adjust the chess engine's thinking time, allowing it more time to calculate moves when the player is idle.

## Architecture

### 1. Global State Variables (Lines 102-107)
```cpp
unsigned long lastActivityTime = 0;      // millis() of last player activity
unsigned long mudActivityStartTime = 0;  // when current quiet period started
```

These track when the player last performed an action and when the current quiet period began.

### 2. Core Functions (Lines 109-133)

#### `getMUDActivityDuration()` (Lines 109-112)
Returns the duration (in milliseconds) since the last player activity.
- Returns 0 if activity timer has never been set
- Otherwise returns `millis() - lastActivityTime`

#### `resetMUDActivityTimer()` (Lines 114-117)
Called whenever the player performs an action that should "reset" the activity counter.
- Sets `lastActivityTime` to current `millis()`
- Resets the quiet period start time

#### `getEngineThinkingTimeMs()` (Lines 119-133)
Calculates how long the chess engine should think based on current activity:
- 0-1 second quiet → 1 second thinking
- 1-2 seconds quiet → 2 seconds thinking
- 2-3 seconds quiet → 3 seconds thinking
- 3-4 seconds quiet → 4 seconds thinking
- 4+ seconds quiet → 5 seconds thinking

## Integration Points

### 1. Command Processing (Lines 13918-13937)

**Location:** `handleCommand()` function

The activity timer is reset for most commands, but chess moves are excluded:

```cpp
// Check if this is a chess move (shouldn't reset activity)
bool isChessMove = false;
if (index >= 0 && index < MAX_PLAYERS && chessGames[index].gameActive) {
    // Detect chess moves like "e4", "e2e4", "Nf3"
    // But NOT "board", "help", "status", "end", "quit"
}

if (!isChessMove) {
    resetMUDActivityTimer();  // Reset for normal commands
}
```

**Special handling for chess:**
- Piece notations: "Nf3", "Bf5", "Rh4", etc. → Don't reset
- Square moves: "e4", "d5", "e2e4" → Don't reset
- Commands: "board", "help", "status" → DO reset
- Exit: "end", "quit" → DO reset

### 2. Player Leaving Game Room (Lines 13845-13869)

**Location:** `handleCommand()` function, room check section

Added detection for players leaving the chess game room:
```cpp
// Check Chess game room
if (index >= 0 && index < MAX_PLAYERS && chessGames[index].gameActive) {
    if (p.roomX != 247 || p.roomY != 248 || p.roomZ != 50) {
        // Player moved out of Game Parlor - end chess game
        endChessGame(p, index);
        p.client.println("Your Chess game has ended because you left the Game Parlor.");
        p.client.println("");
    }
}
```

When a player moves away from Game Parlor (247, 248, 50), the game ends and activity is reset.

### 3. Engine Search Time (Lines 7570-7580)

**Location:** `processChessMove()` function

The engine now uses dynamic thinking time:
```cpp
int thinkTimeMs = getEngineThinkingTimeMs();
mcumax_move bestMove = mcumax_search_best_move(thinkTimeMs, searchDepth);
```

Previously hardcoded as 5000ms, now ranges from 1000-5000ms based on player activity.

### 4. Engine Move Delay (Lines 7611-7613)

**Location:** `processChessMove()` function

The delay after engine finds a move now uses the same dynamic calculation:
```cpp
if (!moveFromOpeningBook && foundEngineMove) {
    delay(getEngineThinkingTimeMs());  // Dynamic think time based on activity
}
```

This ensures the engine's behavior is realistic - it appears to "think" for the same duration as its actual search.

### 5. Game End Activity Reset (Lines 7737-7740)

**Location:** `endChessGame()` function

When the player ends a chess game manually, the activity timer is reset:
```cpp
// Reset activity timer when exiting game
resetMUDActivityTimer();
```

## Activity Timeline

Example scenario:
1. **T=0**: Player types a command → `resetMUDActivityTimer()` called
2. **T=0.5s**: Player idles
3. **T=1.5s**: `getMUDActivityDuration()` returns ~1500ms
4. **T=1.5s**: Engine gets 2 seconds to think (1-2s quiet period)
5. **T=2.5s**: Player types "e4" → Chess move detected, timer NOT reset
6. **T=3.5s**: Engine gets 4 seconds to think (2-3s quiet period)
7. **T=4.5s**: Player types "board" → Command (not chess move), timer IS reset
8. **T=4.5s**: Engine gets 1 second to think (0-1s quiet period)

## Benefits

1. **Responsive Feel**: Engine doesn't waste time thinking when player is actively playing
2. **Smart Thinking**: Engine gets maximum time (5 seconds) when player is idle
3. **Chess-Aware**: Recognizes chess moves and doesn't interrupt the flow
4. **Configurable**: Can adjust the thresholds in `getEngineThinkingTimeMs()` if needed

## Testing Notes

- Monitor the thinking time when playing chess in fast vs. slow gameplay
- Verify that chess moves don't reset the timer while other commands do
- Check that leaving the Game Parlor properly ends the game and resets activity
- Verify that engine appears to think longer during idle periods

## Future Improvements

1. Could add different activity patterns for different room types
2. Could extend activity monitoring to other NPCs/behaviors
3. Could add configurable thresholds via server settings
4. Could log activity statistics for game balance analysis
