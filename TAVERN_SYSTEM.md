# Tavern Drink System Documentation

## Overview
A tavern system has been implemented at voxel **249,242,50** featuring the "Tavern Libations" establishment. Players can purchase drinks that restore HP and consume "drunkenness units" that track intoxication.

## Drink Inventory

### 1. Giant's Beer
- **Price:** 10 gold
- **HP Restoration:** +2 HP
- **Drunkenness Cost:** 3 units
- **Max Drinks Before Drunk:** 2 (6 units ÷ 3 = 2 drinks)

### 2. Honeyed Mead
- **Price:** 5 gold
- **HP Restoration:** +3 HP
- **Drunkenness Cost:** 2 units
- **Max Drinks Before Drunk:** 3 (6 units ÷ 2 = 3 drinks)

### 3. Faery Fire
- **Price:** 20 gold
- **HP Restoration:** +5 HP
- **Drunkenness Cost:** 5 units
- **Max Drinks Before Drunk:** 1 (6 units ÷ 5 = 1 drink, then player needs 1 more unit to reach 6)

## Drunkenness System

### Key Mechanics
- **Starting State:** Players begin with 6 drunkenness units (fully sober)
- **Drunk Threshold:** At 0 units, player cannot drink anymore (message: "You are too drunk to drink any more!")
- **Recovery Rate:** 1 unit per 60 seconds (1 minute)
- **Tavern Unlimited Inventory:** The tavern has an unlimited supply of drinks

### Drunkenness Display
- When reading the tavern sign with `read sign`, players see their current drunkenness level (e.g., "Drunkenness: 4/6")
- As they drink, they see feedback:
  - At 0: "You are now completely drunk and can't drink any more!"
  - At ≤2: "You're getting quite drunk..."

### Recovery Process
- Every 60 seconds, logged-in players recover 1 drunkenness unit
- When reaching 6 units again, they see: "You feel the fog clearing from your mind. You're sober again!"

## Implementation Details

### Data Structures
```cpp
struct Drink {
    String name;              // Drink name (e.g., "Giant's Beer")
    int price;                // Gold coin cost
    int hpRestore;            // HP restored when consumed
    int drunkennessCost;      // Units consumed from drunkenness counter
};

struct Tavern {
    int x, y, z;              // Room location (249, 242, 50)
    String tavernName;        // Display name
    std::vector<Drink> drinks; // Available drinks
    Drink* findDrink(const String &target);
    void displayMenu();
};
```

### Player Struct Addition
```cpp
int drunkenness = 6;                          // 0 = drunk, 6 = sober
unsigned long lastDrunkRecoveryCheck = 0;     // Recovery timer
```

### Key Functions
- `initializeTaverns()` - Sets up tavern and drinks at startup
- `getTavernForRoom(Player &p)` - Returns tavern for current room (or nullptr)
- `updateDrunkennessRecovery(Player &p)` - Handles per-minute recovery
- `showTavernSign(Player &p, Tavern &tavern)` - Displays tavern menu
- `cmdDrink(Player &p, const String &arg)` - Handles drink command

### Command Usage
```
drink giant's beer
drink honeyed mead
drink faery fire
read sign       // Shows current tavern menu and drunkenness level
```

## Gameplay Notes

1. **Strategic Drinking:** Players need to manage drunkenness units carefully. More expensive drinks (like Faery Fire) cost more drunkenness but restore more HP.

2. **Healing Without Combat:** The tavern provides alternative healing for non-combat situations or emergencies.

3. **Time-Based Recovery:** Encourages players to pace their drinking throughout gameplay sessions.

4. **Tavern Location:** Hard-coded at voxel 249,242,50 - future versions could make this configurable.

## Future Enhancements

- [ ] Add more taverns at different locations
- [ ] Add drunkenness status effects (slurred speech, reduced accuracy, etc.)
- [ ] Implement player-specific drink limits beyond drunkenness
- [ ] Add bartender NPC with ambient dialogue
- [ ] Create drink combo recipes or special events
- [ ] Persist drunkenness state in player save files

## Testing Checklist

- ✓ No compilation errors
- ✓ Tavern initialization at startup
- ✓ Sign display shows correct drinks and drunkenness level
- ✓ Drink purchase deducts correct gold
- ✓ HP restoration works correctly
- ✓ Drunkenness units decremented correctly
- ✓ "Too drunk" message appears at 0 units
- ✓ Recovery happens every 60 seconds
- ✓ Can drink inventory items alongside tavern drinks
