# MUD Activity Monitoring - Visual Guide

## System Architecture Diagram

```
┌─────────────────────────────────────────────────────────────┐
│                     GAME LOOP                               │
└─────────────────────────────────────────────────────────────┘
                            │
                            ↓
                    ┌───────────────┐
                    │ Player Input  │
                    └───────────────┘
                            │
                            ↓
                  ┌─────────────────────┐
                  │  handleCommand()    │
                  │  (Line 13840)       │
                  └─────────────────────┘
                            │
        ┌───────────────────┼───────────────────┐
        ↓                   ↓                   ↓
   Is chess move?    Is room exit?      Is game active?
   (Line 13920)      (Line 13857)       
        │                   │
    ┌───┴───┐           ┌───┴───┐
    │ YES   │ NO        │ YES   │ NO
    ↓       ↓           ↓       ↓
  Don't   Reset       End    Continue
  reset   timer       game
 timer  (resetMUD    (endChess
        ActivityT)    Game)
        │
        ├─→ resetMUDActivityTimer()
        │   (Line 114)
        │   Sets: lastActivityTime = millis()
        │
        ↓
  Process Command
        │
        ├─→ processChe.ssMove()
        │   (Line 7228)
        │
        ├─→ Calculate thinking time
        │   (Line 7580)
        │
        │   getEngineThinkingTimeMs()
        │   (Line 119)
        │   ├─→ getMUDActivityDuration()
        │   │   (Line 109)
        │   │   Returns: millis() - lastActivityTime
        │   │
        │   └─→ Map duration to 1-5 seconds
        │       0-1s   → 1000ms
        │       1-2s   → 2000ms
        │       2-3s   → 3000ms
        │       3-4s   → 4000ms
        │       4+s    → 5000ms
        │
        ├─→ mcumax_search_best_move(thinkTimeMs, depth)
        │   (Line 7580)
        │
        ├─→ delay(thinkTimeMs)
        │   (Line 7612)
        │
        └─→ Display move & return to game loop
```

## Timeline Example: Fast vs. Slow Play

### Fast Play Scenario
```
Time    Action                          Activity    Engine
                                        Duration    Thinking
─────   ──────────────────────────────  ────────    ────────
0.0s    Player: "e4"                    0s          Reset
0.1s    Engine searches...              0.1s        1 sec
1.1s    Engine: "e5"
        Player: "Nf3"                   0s (reset)  Reset
1.2s    Engine searches...              0.1s        1 sec
2.2s    Engine: "Nc6"
        Player: "Bc4"                   0s (reset)  Reset
2.3s    Engine searches...              0.1s        1 sec
3.3s    Engine plays...
```

### Slow Play Scenario
```
Time    Action                          Activity    Engine
                                        Duration    Thinking
─────   ──────────────────────────────  ────────    ────────
0.0s    Player: "e4"                    0s          Reset
0.1s    Engine searches...              0.1s        1 sec
1.1s    Engine: "e5"
        [Player thinking...]            1.1s → 2s
2.1s    Player: "Nf3"                   2.0s        Reset
2.2s    Engine searches...              0.1s        1 sec
3.2s    Engine: "Nc6"
        [Player thinking...]            1.2s → 2s
4.2s    Player: "Bc4"                   3.0s        Reset
4.3s    Engine searches...              0.1s        1 sec
5.3s    Engine: "Nxe4"
        [Player thinking...]            2.0s → 3s
        [Player thinking longer...]     3.5s → 4s
7.3s    Player: "Qh5"                   4.0s        Reset
7.4s    Engine searches...              0.1s        4 sec
11.4s   Engine: "Nf6+"
```

## Activity State Transitions

```
┌─────────────────┐
│  Initial State  │
│  No activity    │
│  lastActivity=0 │
└────────┬────────┘
         │
         │ Player makes move
         ↓
┌──────────────────────────────────┐
│ Activity Reset                   │
│ lastActivityTime = millis()      │
│ Duration = 0ms                   │
│ Engine gets: 1 second            │
└────────┬─────────────────────────┘
         │
         │ Time passes...
         ↓
┌──────────────────────────────────┐
│ Duration: 100-999ms              │
│ Engine gets: 1 second            │
│ (~0.1-1 second idle)             │
└────────┬─────────────────────────┘
         │
         │ More time passes...
         ↓
┌──────────────────────────────────┐
│ Duration: 1000-1999ms            │
│ Engine gets: 2 seconds           │
│ (~1-2 seconds idle)              │
└────────┬─────────────────────────┘
         │
         │ ... (similar pattern)
         ↓
┌──────────────────────────────────┐
│ Duration: 4000+ ms               │
│ Engine gets: 5 seconds           │
│ (4+ seconds idle)                │
│ Maximum thinking                 │
└────────┬─────────────────────────┘
         │
         │ Player makes move
         ↓
    RESET (back to top)
```

## Chess Move vs. Command Distinction

```
Player enters: "e4"
    │
    ├─→ Is it in game? YES
    ├─→ Does cmd start with a-h? YES
    ├─→ Cmd length ≤ 4? YES
    │
    └─→ **CHESS MOVE** (don't reset)


Player enters: "board"
    │
    ├─→ Is it in game? YES
    ├─→ Matches "board"? YES
    │
    └─→ **COMMAND** (RESET timer)


Player enters: "Nf3"
    │
    ├─→ Is it in game? YES
    ├─→ Cmd[0] is A-Z? YES
    ├─→ Cmd[1] is a-h? YES
    │
    └─→ **CHESS MOVE** (don't reset)


Player enters: "n"
    │
    ├─→ Is it in game? NO
    │
    └─→ **MOVEMENT** (RESET timer)
```

## Thinking Time Visualization

```
Duration │  Engine Thinking Duration
─────────┼────────────────────────────
0-1 sec  │  ████ 1 second
1-2 sec  │  ████████ 2 seconds
2-3 sec  │  ████████████ 3 seconds
3-4 sec  │  ████████████████ 4 seconds
4+ sec   │  ████████████████████ 5 seconds (MAX)
```

## Game Flow with Activity Monitoring

```
START GAME
    │
    ├─→ lastActivityTime = millis()
    │
PLAYER TURN
    │
    ├─→ Player enters move "e4"
    │   └─→ resetMUDActivityTimer()
    │
    ├─→ HUMAN plays "e4"
    │
ENGINE TURN
    │
    ├─→ getMUDActivityDuration() → ~0.5s
    ├─→ getEngineThinkingTimeMs() → 1000ms
    │
    ├─→ mcumax_search_best_move(1000ms, depth)
    │   └─→ Searches for 1 second
    │
    ├─→ delay(1000ms)
    │   └─→ Waits 1 second (realistic thinking)
    │
    ├─→ Display: "Engine plays: e5"
    │
PLAYER TURN (again)
    │
    ├─→ Player enters move "Nf3" (CHESS MOVE)
    │   └─→ Timer NOT reset (still counting)
    │   └─→ Activity duration ≈ 1.5 seconds
    │
ENGINE TURN
    │
    ├─→ getEngineThinkingTimeMs() → 2000ms
    ├─→ mcumax_search_best_move(2000ms, depth)
    └─→ Takes 2 seconds to think

... game continues ...

GAME ENDS
    │
    └─→ endChessGame()
        └─→ resetMUDActivityTimer()
```

## Configuration: How to Customize

To change thinking time thresholds, modify this function:

```cpp
// Location: Line 119
int getEngineThinkingTimeMs() {
    unsigned long activityMs = getMUDActivityDuration();
    
    // CHANGE THESE VALUES:
    if (activityMs < 1000) return 1000;      // Change: 1000
    else if (activityMs < 2000) return 2000; // Change: 2000
    else if (activityMs < 3000) return 3000; // Change: 3000
    else if (activityMs < 4000) return 4000; // Change: 4000
    else return 5000;                         // Change: 5000
}
```

### Example: More Aggressive (Less Thinking)
```cpp
if (activityMs < 1000) return 500;       // 0.5 second
else if (activityMs < 2000) return 1000; // 1 second
else if (activityMs < 3000) return 1500; // 1.5 seconds
else if (activityMs < 4000) return 2000; // 2 seconds
else return 3000;                         // 3 seconds (MAX)
```

### Example: More Patient (More Thinking)
```cpp
if (activityMs < 1000) return 2000;       // 2 seconds
else if (activityMs < 2000) return 3000;  // 3 seconds
else if (activityMs < 3000) return 4000;  // 4 seconds
else if (activityMs < 4000) return 5000;  // 5 seconds
else return 8000;                          // 8 seconds (MAX)
```

---

**Note**: All times are in milliseconds. 1 second = 1000 milliseconds.
