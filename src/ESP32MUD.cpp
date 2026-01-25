#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <LittleFS.h>
#include <vector>
#include <map>
#include <set>
#include <algorithm>
#include <cmath>
#include <qrcode.h>
#include <ESP_Mail_Client.h>
#include "YmodemBootloader.h"
#include "version.h"  // Auto-generated at build time  VERSION INFO Auto generated version Number
#include "chess_game.h"



// =============================
// Core limits and constants
// =============================

#define MAX_PLAYERS    10
#define MAX_INVENTORY 32
#define MAX_WEIGHT 10
#define MAX_NPCS       50
#define NPC_RESPAWN_SECONDS 600
#define MAX_OUTPUT_WIDTH 80   // Word wrap for descriptions and long text (MUD convention)
#define MAX_QRCODE_WIDTH 100  // QR code display width for better reliability with longer messages

// Path for WiFi provisioning credentials
const char* credPath = "/credentials.txt";

//Check for Serial connection if there is none we MUST skip provisioning
bool NoSerial = false;  

// =====================================================
// TIMEZONE OFFSET (in hours, fetched from time server or hardcoded)
// =====================================================
int timezoneOffsetSeconds = -5 * 3600;  // Default: EST (UTC-5), will be updated from time server

// =====================================================
// GLOBAL REBOOT INTERVAL 
// =====================================================
unsigned long nextGlobalRespawn = 0;

// ⭐ TEST MODE — 2‑minute reboot cycle
// const unsigned long GLOBAL_RESPAWN_INTERVAL = 2UL * 60UL * 1000UL; // 2 minutes

// ⭐ PRODUCTION MODE — 6‑hour reboot cycle
const unsigned long GLOBAL_RESPAWN_INTERVAL = 6UL * 60UL * 60UL * 1000UL; // 6 hours

// Forward declarations
struct ShopInventoryItem;
struct Shop;

// =============================================================
// SHOP SYSTEM - Room-Based Shops (Definition)
// =============================================================

struct ShopInventoryItem {
    int itemNumber;           // 1, 2, 3, ...
    String itemId;            // e.g., "Dagger", "rusty_shortsword" - used to look up in itemDefs
    String itemName;          // display name (may differ from itemId)
    int quantity;             // how many in stock
    int price;                // gold coins (selling price)
};

struct Shop {
    int x, y, z;              // room location
    String shopName;          // e.g., "Blacksmith"
    String shopType;          // e.g., "blacksmith"
    std::vector<ShopInventoryItem> inventory;

    ShopInventoryItem* findItem(const String &target);
    void displayInventory(Client &client);
    void addItem(const String &itemId, const String &displayName, int qty, int price);
    void addOrUpdateItem(const String &itemId, const String &displayName, int price);
};

// =============================================================
// TAVERN DRINK SYSTEM
// =============================================================

struct Drink {
    String name;              // e.g., "Giant's Beer"
    int price;                // gold coins
    int hpRestore;            // HP restored when drunk
    int drunkennessCost;      // drunkenness units consumed
};

struct Tavern {
    int x, y, z;              // room location
    String tavernName;        // e.g., "The Rusty Griffin"
    std::vector<Drink> drinks;

    Drink* findDrink(const String &target);
    void displayMenu();
};

// =============================================================
// POST OFFICE SYSTEM
// =============================================================

struct PostOffice {
    int x, y, z;              // room location
    String postOfficeName;    // e.g., "The Post Office"
};

// High-Low card game structures
struct Card {
    int value;          // 2-13 (2-10 are numeric, 11=Jack, 12=Queen, 13=King)
    int suit;           // 0=Hearts, 1=Spades, 2=Diamonds, 3=Clubs (flavor only)
    bool isAce;         // true if value is Ace (1 or 14 depending on declaration)
};

struct HighLowSession {
    std::vector<Card> deck;      // 104-card deck (double deck)
    Card card1, card2, card3;    // current hand
    int card1Value, card2Value;  // may differ from card.value if Ace is involved
    bool gameActive;             // true if player is actively playing
    bool awaitingAceDeclaration; // waiting for player to declare Ace high/low
    bool awaitingContinue;       // waiting for player to press Enter or type 'end'
    bool betWasPot;              // true if player bet the entire pot
    int gameRoomX, gameRoomY, gameRoomZ;  // track which room the game started in
};

struct ChessSession {
    bool gameActive;             // true if player is actively playing
    unsigned char board[64];     // 64 squares: 0=empty, 1-6=white pieces, 7-12=black pieces
                                 // Piece encoding: 1=P(pawn), 2=N(knight), 3=B(bishop), 4=R(rook), 5=Q(queen), 6=K(king)
                                 // Add 6 for black pieces
    bool playerIsWhite;          // true if player plays white
    int moveCount;               // moves made (increments after both sides move)
    bool isBlackToMove;          // whose turn it is
    int gameRoomX, gameRoomY, gameRoomZ;  // track which room the game started in
    String lastEngineMove;       // last move made by engine
    String lastPlayerMove;       // last move made by player
    bool gameEnded;              // true if game has ended
    String endReason;            // why game ended (checkmate, stalemate, resignation)
};

// Letter system for mail retrieval
struct Letter {
    String to;               // recipient email
    String from;             // sender email
    String subject;          // email subject
    String body;             // email body text
    String messageId;        // unique ID for tracking
    String displayName;      // display name (playername if found, or sender email part)
};

bool warned5min  = false;
bool warned2min  = false;
bool warned1min  = false;
bool warned30sec = false;
bool warned5sec  = false;



unsigned long lastShopRestock = 0;

// Global shops vector
std::vector<Shop> shops;

// Global tavern vector
std::vector<Tavern> taverns;

// Global post office vector
std::vector<PostOffice> postOffices;

// High-Low game sessions (one per player)
HighLowSession highLowSessions[MAX_PLAYERS];

// Chess game sessions (one per player)
ChessSession chessSessions[MAX_PLAYERS];

// Global High-Low pot (shared by all players)
int globalHighLowPot = 50;

bool g_inYmodem = false;

bool receivingFile = false;
File currentFile;
String currentFilename = "";
int expectedSize = 0;
int receivedSize = 0;

String pendingSerialCommand = "";

// Global welcome messages
const char* GLOBAL_MUD = "Welcome to Esperthertu!";

const char* GLOBAL_WELCOME_LINES[] = {
    "You feel the veil of the mundane world slip away...",
    "A hush falls over your senses as ancient powers stir.",
    "Somewhere in the distance, a bell tolls -- once, twice --",
    "as if marking your arrival in a land shaped by myth and memory.",
    nullptr
};

// Combat verbs
const char* combatVerbs[] = {
    "slash",
    "stab",
    "crush",
    "strike",
    "smash",
    "cut",
    "bash"
};

const char* npcDeathMsgs[] = {
    "falls to the ground lifeless.",
    "collapses in a heap.",
    "lets out a final gasp.",
    "crumples to the floor.",
    "is slain instantly.",
    "drops with a dull thud."
};



// =============================
// Direction indices and names
// =============================

enum Direction {
    DIR_NORTH = 0,
    DIR_EAST  = 1,
    DIR_SOUTH = 2,
    DIR_WEST  = 3,
    DIR_UP    = 4,
    DIR_DOWN  = 5,
    DIR_NE    = 6,
    DIR_NW    = 7,
    DIR_SE    = 8,
    DIR_SW    = 9
};

const char* dirNames[10] = {
    "north","east","south","west","up","down",
    "northeast","northwest","southeast","southwest"
};

// =============================
// Body slots for armor/equipment
// =============================

enum BodySlot {
    SLOT_HEAD = 0,
    SLOT_CHEST = 1,     // outer armor
    SLOT_CHEST2 = 2,    // underarmor (chainmail, gambeson)
    SLOT_LEGS = 3,
    SLOT_FEET = 4,
    SLOT_HANDS = 5,
    SLOT_ARMS = 6,
    SLOT_BACK = 7,      // cloaks, capes
    SLOT_WAIST = 8,     // belts
    SLOT_NECK = 9,      // amulets
    SLOT_FINGER = 10,   // rings

    SLOT_COUNT
};

// =============================
// Forward declarations
// =============================

struct Player;
struct Room;
struct WorldItem;
struct NpcInstance;
struct ItemDefinition;

// Core functions
void cmdGet(Player &p, const char* targetStr);
void cmdGetFrom(Player &p, const char* targetStr, const char* containerStr);
void broadcastToRoom(int x, int y, int z, const String &msg, Player *exclude = nullptr);
void broadcastToAll(const String &msg);
void broadcastRoomExcept(Player &p, const String &msg, Player &exclude);
String wordWrap(const String &text, int width);
String ensurePunctuation(const String &text);
void printWrappedLines(Client &client, const String &text, int width);
void announceToRoomWrapped(int x, int y, int z, const String &msg, int excludeIndex);
void announceToRoom(int x, int y, int z, const String &msg, int excludeIndex);
void announceToRoomExcept(int x, int y, int z, const String &msg, int excludeA, int excludeB);

// Debug and logging
void debugDumpItemsToSerial();
void debugDumpFilesToSerial();
void debugDumpPlayersToSerial();
void debugDumpNPCsToSerial();
void debugDumpSinglePlayerToSerial(const String &name);
void debugListAllFiles(Player &p);
void debugPrint(Player &p, const String &msg);
void debugPrintNoNL(Player &p, const String &msg);

// World state
void resetWorldState();
void loadItemDefinitions(String line);
void loadNPCDefinitions(String line);
void loadWorldItemsFromSave();
void loadNPCInstance(const String &line);
void applyEquipmentBonuses(Player &p);

// Item and room management
void savePlayerToFS(Player &p);
String addArticle(const String &text);
void cmdLook(Player &p);
void cmdLookAt(Player &p, const String &input);
void cmdLookIn(Player &p, const String &input);
void showItemDescription(Player &p, WorldItem &wi);
void showItemDescriptionNormal(Player &p, WorldItem &wi);
void mergeCoinPilesAt(int x, int y, int z);
void spawnGoldAt(int x, int y, int z, int amount);

// Item resolution
int resolveItem(Player &p, const String &raw);
int resolveItemForSearch(Player &p, const String &raw);
int resolveItemInContainer(Player &p, WorldItem &container, const String &raw);
WorldItem* getResolvedWorldItem(Player &p, int resolvedIndex);
WorldItem* findShopSign(Player &p);
int findItemInShop(WorldItem &sign, const String &target);
int findInventoryItemIndexForShop(Player &p, const String &targetRaw);
bool nameMatches(const String &input, const String &display);

// String utilities
String capFirst(const char *name);
String cleanInput(const String &in);
String unescapeNewlines(const String &s);
String sanitizeMsg(const String &in);
bool npcNameMatches(const String &npcName, const String &arg);
bool isValidPassword(const String &s);

// Combat and death
void handlePlayerDeath(Player &p);

// Command wrappers
void cmdDrop(Player &p, const char *input);
void cmdDrop(Player &p, const String &input);
void cmdDropAll(Player &p, const char *unused);
void cmdDropAll(Player &p);
void cmdQrCode(Player &p, const String &input);
void drawPlayerMap(Player &p);
void cmdMap(Player &p);
void cmdTownMap(Player &p);
void cmdReadSign(Player &p, const String &input);
void cmdBuy(Player &p, const String &arg);
void cmdSell(Player &p, const String &arg);
void cmdSellAll(Player &p);
void cmdDrink(Player &p, const String &arg);

// Shop functions
Shop* getShopForRoom(Player &p);
void initializeShops();

// Tavern functions
Tavern* getTavernForRoom(Player &p);
void initializeTaverns();
void updateDrunkennessRecovery(Player &p);
void updateFullnessRecovery(Player &p);
void showTavernSign(Player &p, Tavern &tavern);

// Post Office functions
PostOffice* getPostOfficeForRoom(Player &p);
void initializePostOffices();
void cmdSend(Player &p, const String &input);
void cmdSendMail(Player &p, const String &input);
void showPostOfficeSign(Player &p, PostOffice &po);

// High-Low card game functions
void initializeHighLowSession(int playerIndex);
void startHighLowGame(Player &p, int playerIndex);
void dealHighLowHand(Player &p, int playerIndex);
void processHighLowBet(Player &p, int playerIndex, int betAmount, bool potBet = false);
void declareAceValue(Player &p, int playerIndex, int aceValue);
void promptHighLowContinue(Player &p, int playerIndex);
void endHighLowGame(Player &p, int playerIndex);
String getCardName(const Card &card);

// Chess game function declarations
void initChessGame(ChessSession &session, bool playerIsWhite);
void renderChessBoard(Player &p, ChessSession &session);
String formatTime(unsigned long ms);
bool parseChessMove(String moveStr, int &fromCol, int &fromRow, int &toCol, int &toRow);
void startChessGame(Player &p, int playerIndex, ChessSession &session);
void processChessMove(Player &p, int playerIndex, ChessSession &session, String moveStr);
void endChessGame(Player &p, int playerIndex);

bool checkAndSpawnMailLetters(Player &p);  // Returns true if mail was found and letters spawned
bool fetchMailFromServer(const String &playerName, std::vector<Letter> &letters);
String extractPlayerNameFromEmail(const String &emailBody);


// File upload handler
void handleFileUploadRequest(WiFiClient &client);

// =============================
// Item and NPC definition system
// =============================

struct ItemDefinition {
    std::string type;
    std::map<std::string, std::string> attributes;  // key -> value
};


struct NpcDefinition {
    std::string type;
    std::map<std::string, std::string> attributes;
};

struct NpcInstance {
    int x, y, z;                // current position
    int spawnX, spawnY, spawnZ; // original spawn point
    String npcId;
    String parentId;
    int hp;
    int gold;
    bool alive;
    unsigned long respawnTime = 0;  // 0 = not scheduled
    bool hostileTo[MAX_PLAYERS];   // per-player hostility
    int targetPlayer;              // index of the player it's actively fighting

    unsigned long nextDialogTime = 0;
    int dialogIndex = 0;
    int dialogOrder[3] = {0,1,2};
    int combatDialogCounter = 0;

    bool suppressDeathMessage;

};

std::vector<NpcInstance*> getNPCsAt(int x, int y, int z);

// =============================================================
// Core item engine: Parent/Child + Ownership Model
// =============================================================
//
// WorldItem model:
//   ownerName == ""        → item is in the world
//   ownerName == player    → item is in that player's inventory
//   parentName == ""       → top-level item (floor or inventory)
//   parentName == ItemID   → child item inside that parent


struct WorldItem {
    int x, y, z;
    String name;
    String ownerName;
    String parentName;
    int value;
    int nextWorldItemId = 1;
    int dialogIndex = 0;
    int dialogOrder[3] = {0, 1, 2};


    // SAFE: std::string inside std::map
    std::map<std::string, std::string> attributes;

    // Children are indices into worldItems, NOT pointers
    std::vector<int> children;

    String getAttr(const String& key,
                   std::map<std::string, ItemDefinition>& defs) const
    {
        std::string skey = key.c_str();

        // 1. Instance override
        auto it2 = attributes.find(skey);
        if (it2 != attributes.end())
            return String(it2->second.c_str());

        // 2. Template fallback
        auto it = defs.find(std::string(name.c_str()));
        if (it != defs.end()) {
            const auto &attrMap = it->second.attributes;
            auto jt = attrMap.find(skey);
            if (jt != attrMap.end())
                return String(jt->second.c_str());
        }

        return "";
    }
};




// =============================
// Emotes
// =============================

struct Emote {
    const char* base;
    const char* verb;
    const char* preposition;   // "to", "at", or "" (ignored if needsPreposition == false)
    const char* adverb;
    bool needsPreposition;     // true = "wave to", false = "thank"
};

// =============================
// Portal and Room definition
// =============================

struct Room {
    // Voxel coordinates
    int x, y, z;

    char name[32];
    char description[512];

    // Exit flags (1 = exit present, 0 = blocked)
    int exit_n, exit_s, exit_e, exit_w;
    int exit_ne, exit_nw, exit_se, exit_sw;
    int exit_u, exit_d;

    String exitList;   // replaces old exit_s STRING

    // Portal data (optional)
    bool hasPortal;
    int px, py, pz;             // destination voxel for portal
    char portalCommand[32];     // command player types
    char portalText[128];       // flavor text when using portal
};

// =============================
// Player representation
// =============================

enum DebugDestination {
    DEBUG_NONE      = 0,
    DEBUG_TO_SERIAL = 1,
    DEBUG_TO_TELNET = 2
};

struct Player {
    WiFiClient client;
    bool active;
    bool loggedIn;
    bool IsWizard;
    bool showStats = true;   // default ON for wizards
    DebugDestination debugDest = DEBUG_TO_SERIAL;   // default
    bool IsInvisible = false;
    bool isDuplicateLogin = false;  // true if disconnected old session to reconnect

    //wizard entrance and exit message overrides
    String EnterMsg;   // Custom arrival message
    String ExitMsg;    // Custom departure message

    char name[32];
    char password[32];
    char storedPassword[32];

    int raceId;
    int hp;
    int maxHp;
    int coins;
    int xp;
    int level;
    int questsCompleted;

    bool wimpMode;
    int  carryCapacity;

    // Each player has their own current Room instance
    Room currentRoom;

    // Also track explicit voxel coordinates for convenience
    int roomX;
    int roomY;
    int roomZ;

    // Inventory as indices into worldItems (ownership via parentName = player.name)
    // This is a cache for quick access; source of truth is worldItems.
    int invIndices[32];
    int invCount;

    // Equipment tracked by worldItems index
    int wieldedItemIndex;
    int wornItemIndices[SLOT_COUNT];

    int attack = 1;       // base damage
    int weaponBonus = 0;
    int armorBonus = 0;
    int baseDefense = 9;  // classic AC baseline

    // Healing timer
    unsigned long lastHealCheck;

    // questStepDone[questIndex][stepIndex]
    // questIndex: 0–9 for questid 1–10
    // stepIndex : 0–9 for step 1–10
    bool questStepDone[10][10];
    bool questCompleted[10];

    // Voxel debug flag
    bool sendVoxel;

    bool inCombat = false;
    unsigned long nextCombatTime = 0;
    NpcInstance* combatTarget = nullptr;

    // Drunkenness system (0 = drunk, 6 = sober)
    int drunkenness = 6;
    unsigned long lastDrunkRecoveryCheck = 0;

    // Fullness/satiation system (0 = too full, 6 = hungry)
    int fullness = 6;
    unsigned long lastFullnessRecoveryCheck = 0;

    // Visited voxels tracker for mapper utility (max 500 visited rooms)
    struct VisitedVoxel {
        int x, y, z;
    };
    std::vector<VisitedVoxel> visitedVoxels;
    static const int MAX_VISITED_VOXELS = 500;
    
    // Map tracker toggle
    bool mapTrackerEnabled = false;
};

// =============================
// Quest system types
// =============================

struct QuestStep {
    int questId = 0;     // 1–10
    int step    = 0;     // 1–10

    String task;         // "give", "kill", "say", "reach", "pickup", etc.
    String item;         // item id (for give/pickup/drop/use)
    String target;       // npc id (for give/kill/escort/etc.)
    String phrase;       // for say

    int targetX = 0;     // for reach/say/drop/use/etc.
    int targetY = 0;
    int targetZ = 0;
};

struct QuestDef {
    int questId = 0;

    String name;
    String description;
    String difficulty;
    String completionDialog;
    String unloadNpcId;    // npc id from defs; optional

    int rewardXp = 0;      // ⭐ NEW
    int rewardGold = 0;    // ⭐ NEW

    std::vector<QuestStep> steps;
};

// =============================
// Global instances
// =============================

// Global item and NPC definitions and world placements
std::map<std::string, ItemDefinition> itemDefs;
std::vector<WorldItem> worldItems;

std::map<std::string, NpcDefinition> npcDefs;
std::vector<NpcInstance> npcInstances;


// Global quest registry
std::map<int, QuestDef> quests;

WiFiServer* server = nullptr;

// =====================================================
// FILE UPLOAD SERVER (WiFi-based, runs on port 8080)
// =====================================================
WiFiServer* fileUploadServer = nullptr;
const int FILE_UPLOAD_PORT = 8080;

Player players[MAX_PLAYERS];
int    npcCount = 0;

// Per-NPC attack cooldown (3s rounds like players)
unsigned long npcNextAttack[MAX_NPCS] = {0};

// =============================
// Voxel index helpers for rooms.txt
// =============================

struct VoxelResult {
    String line;
    int    elapsed;
};

struct IndexRecord {
    uint64_t key;
    uint32_t offset;
};

// Used for indexing a voxel uniquely in 64 bits
uint64_t packVoxelKey(int x, int y, int z) {
    return ((uint64_t)(x & 0x1FFFFF) << 42) |
           ((uint64_t)(y & 0x1FFFFF) << 21) |
           ((uint64_t)(z & 0x1FFFFF));
}

// =============================
// Room index creation (IDX + BIN)
// =============================

struct RoomIndex {
    uint64_t key;
    uint32_t offset;
};



//SAFE PROCEDURES
int strToInt(const std::string &s) {
    return atoi(s.c_str());
}
void safePrintln(WiFiClient &c, const std::string &s) {
    c.println(s.c_str());
}
void safePrint(const std::string &s) {
    Serial.print(s.c_str());
}
void safePrintln(const std::string &s) {
    Serial.println(s.c_str());
}

// Forward declaration
void saveWorldItems();

// =============================================================
// SIMPLE XOR ENCRYPTION FOR PASSWORDS
// =============================================================

// Simple XOR cipher - NOT cryptographically secure, but obfuscates plaintext
String encryptPassword(const String &plaintext) {
    const char* key = "Esperthertu";  // Simple key
    String encrypted = "";
    for (int i = 0; i < plaintext.length(); i++) {
        char c = plaintext[i] ^ key[i % strlen(key)];
        // Convert to hex for safe storage
        encrypted += String(c, HEX);
    }
    return encrypted;
}

String decryptPassword(const String &encrypted) {
    const char* key = "Esperthertu";
    String decrypted = "";
    for (int i = 0; i < encrypted.length(); i += 2) {
        String hexPair = encrypted.substring(i, i + 2);
        char c = (char)strtol(hexPair.c_str(), NULL, 16);
        c = c ^ key[(i / 2) % strlen(key)];
        decrypted += c;
    }
    return decrypted;
}

// =============================================================
// POST OFFICE SMTP PASSWORD (Hard-coded)
// =============================================================
const char* POST_OFFICE_SMTP_PASSWORD = "VXN_jhn8bfe5cve7dve";
// =============================================================
// SMTP EMAIL SENDING
// =============================================================

// ISP SMTP Configuration
#define SMTP_HOST "mail.storyboardacs.com"
#define SMTP_PORT 587
#define SMTP_USERNAME "esperthertu_post_office@storyboardacs.com"

// Forward declaration for email callback
void smtpCallback(SMTP_Status status);

// Global SMTP session object
SMTPSession smtp;

// Global email body buffer - keeps content in memory during send
static String g_emailBody = "";

void smtpCallback(SMTP_Status status) {
    // Callback for SMTP status updates (can be used for debugging)
}

// Helper function to escape JSON strings
String escapeJsonString(const String &input) {
    String output = "";
    for (int i = 0; i < input.length(); i++) {
        char c = input[i];
        if (c == '"') {
            output += "\\\"";
        } else if (c == '\\') {
            output += "\\\\";
        } else if (c == '\r') {
            output += "\\r";
        } else if (c == '\n') {
            output += "\\n";
        } else if (c == '\t') {
            output += "\\t";
        } else if (c < 32) {
            // Control characters
            output += "\\u00";
            if (c < 16) output += "0";
            output += String(c, HEX);
        } else {
            output += c;
        }
    }
    return output;
}

bool sendEmailViaSMTP(const String &recipientEmail, const String &message, const String &playerName, const String &playerTitle, const String &password) {
    // Build email body in global variable to ensure it persists
    // Capitalize first letter of player name
    String capitalizedName = playerName;
    if (capitalizedName.length() > 0) {
        capitalizedName[0] = toupper(capitalizedName[0]);
    }
    g_emailBody = "A message from " + capitalizedName + " - The " + playerTitle + ":\r\n\r\n";
    g_emailBody += "Here Ye, Here Ye.  A message from the realm of Esperthertu has been delivered to you!\r\n\r\n";
    g_emailBody += message;
    
    // Use HTTP API instead of direct SMTP
    Serial.println("[EMAIL] Sending via API to " + recipientEmail);
    Serial.println("[EMAIL] Body length: " + String(g_emailBody.length()) + " bytes");
    
    // Create HTTP client for API call
    HTTPClient http;
    String apiUrl = "https://www.storyboardacs.com/sendESP32mail.php";
    
    // Build JSON payload with proper escaping
    String escapedBody = escapeJsonString(g_emailBody);
    String escapedTo = escapeJsonString(recipientEmail);
    
    String jsonPayload = "{";
    jsonPayload += "\"to\":\"" + escapedTo + "\",";
    jsonPayload += "\"subject\":\"Esperthertu Post Office Delivery!\",";
    jsonPayload += "\"from\":\"esperthertu_post_office@storyboardacs.com\",";
    jsonPayload += "\"body\":\"" + escapedBody + "\"";
    jsonPayload += "}";
    
    Serial.println("[EMAIL] JSON size: " + String(jsonPayload.length()) + " bytes");
    
    // Make HTTP POST request
    http.begin(apiUrl);
    http.addHeader("Content-Type", "application/json");
    http.setConnectTimeout(5000);  // 5 second connection timeout
    http.setTimeout(10000);         // 10 second total timeout
    
    unsigned long startTime = millis();
    const unsigned long HTTP_TIMEOUT = 15000;  // 15 second total timeout
    
    Serial.println("[EMAIL] Sending POST request to " + apiUrl);
    int httpResponseCode = http.POST(jsonPayload);
    
    // Check timeout
    if (millis() - startTime > HTTP_TIMEOUT) {
        Serial.println("[EMAIL] HTTP request exceeded timeout");
        http.end();
        return false;
    }
    
    Serial.println("[EMAIL] HTTP Response Code: " + String(httpResponseCode));
    
    if (httpResponseCode > 0) {
        String response = http.getString();
        Serial.println("[EMAIL] Response: " + response);
        
        if (httpResponseCode == 200) {
            http.end();
            return true;
        } else {
            Serial.println("[EMAIL] HTTP error code: " + String(httpResponseCode));
            http.end();
            return false;
        }
    } else {
        String errorMsg = http.errorToString(httpResponseCode);
        Serial.println("[EMAIL] HTTP request failed! Error: " + errorMsg);
        http.end();
        return false;
    }
}

void safeReboot() {
    delay(50);
    ESP.restart();
}



void checkGlobalRebootCountdown(unsigned long now) {
    long remaining = (long)(nextGlobalRespawn - now);

    if (remaining <= 0) {
        broadcastToAll("The world collapses in blinding light!");
        delay(200);
        saveWorldItems();  // Save world state before reboot
        safeReboot();   // ESP.restart() inside here
        return;
    }

    if (!warned5min && remaining <= 5L * 60L * 1000L) {
        warned5min = true;
        broadcastToAll("The world trembles ominously... a great change approaches in 5 minutes.");
    }

    if (!warned2min && remaining <= 2L * 60L * 1000L) {
        warned2min = true;
        broadcastToAll("The air crackles with unstable magic... 2 minutes remain.");
    }

    if (!warned1min && remaining <= 1L * 60L * 1000L) {
        warned1min = true;
        broadcastToAll("Reality distorts violently... 1 minute until world reformation.");
    }

    if (!warned30sec && remaining <= 30L * 1000L) {
        warned30sec = true;
        broadcastToAll("The world begins to unravel... brace yourself!");
    }

    if (!warned5sec && remaining <= 5L * 1000L) {
        warned5sec = true;
        broadcastToAll("The world collapses in blinding light!");
    }
}




bool onQuestEvent(Player &p,
                  const String &task,
                  const String &item,
                  const String &target,
                  const String &phrase,
                  int x, int y, int z)
{
    // Loop all quests (1–10)
    for (int q = 0; q < 10; q++) {

        // Skip completed quests
        if (p.questCompleted[q])
            continue;

        int questId = q + 1;

        // Skip if quest not loaded
        if (!quests.count(questId))
            continue;

        QuestDef &quest = quests[questId];

        // Loop all steps
        for (int s = 0; s < (int)quest.steps.size(); s++) {

            // Skip already completed steps
            if (p.questStepDone[q][s])
                continue;

            QuestStep &step = quest.steps[s];

            // ----------------------------------------------------
            // GIVE
            // ----------------------------------------------------
            if (step.task == "give") {
                if (item == step.item && target == step.target) {

                    p.questStepDone[q][s] = true;

                    if (p.IsWizard)
                        debugPrint(p, "Quest step complete: gave " + step.item + " to " + step.target);

                    goto CHECK_COMPLETE;
                }
            }

            // ----------------------------------------------------
            // KILL
            // ----------------------------------------------------
            if (step.task == "kill") {
                if (target == step.target) {

                    p.questStepDone[q][s] = true;

                    if (p.IsWizard)
                        debugPrint(p, "Quest step complete: killed " + step.target);

                    goto CHECK_COMPLETE;
                }
            }

            // ----------------------------------------------------
            // SAY
            // ----------------------------------------------------
            if (step.task == "say") {
                if (phrase.equalsIgnoreCase(step.phrase)) {

                    p.questStepDone[q][s] = true;

                    if (p.IsWizard)
                        debugPrint(p, "Quest step complete: said \"" + step.phrase + "\"");

                    goto CHECK_COMPLETE;
                }
            }

            // ----------------------------------------------------
            // REACH
            // ----------------------------------------------------
            if (step.task == "reach") {
                if (x == step.targetX &&
                    y == step.targetY &&
                    z == step.targetZ) {

                    p.questStepDone[q][s] = true;

                    if (p.IsWizard)
                        debugPrint(p, "Quest step complete: reached " +
                                     String(step.targetX) + "," +
                                     String(step.targetY) + "," +
                                     String(step.targetZ));

                    goto CHECK_COMPLETE;
                }
            }
        }

        continue; // next quest

        // --------------------------------------------------------
        // CHECK IF QUEST IS NOW COMPLETE
        // --------------------------------------------------------
CHECK_COMPLETE:

        {
            bool allDone = true;

            for (int s = 0; s < (int)quest.steps.size(); s++) {
                if (!p.questStepDone[q][s]) {
                    allDone = false;
                    break;
                }
            }

            if (allDone) {
                p.questCompleted[q] = true;
                savePlayerToFS(p);

                // --------------------------------------------------------
                // SHOW COMPLETION DIALOG (FIXED MULTI-LINE PRINT)
                // --------------------------------------------------------
                if (quest.completionDialog.length() > 0) {

                    String dialog = unescapeNewlines(quest.completionDialog);

                  // Print line-by-line so WiFiClient doesn't truncate
                    int start = 0;
                    while (true) {
                        int nl = dialog.indexOf('\n', start);

                        if (nl == -1) {
                            p.client.println(dialog.substring(start));
                            delay(2000);   // ⭐ 2‑second pause after final line
                            break;
                        }

                        p.client.println(dialog.substring(start, nl));
                        delay(2000);       // ⭐ 2‑second pause between lines

                        start = nl + 1;
                    }

                } else {
                    p.client.println("You have completed this quest! ->  " + quest.name);
                }

                // --------------------------------------------------------
                // AWARD XP AND GOLD
                // --------------------------------------------------------
                if (quest.rewardXp > 0) {
                    p.xp += quest.rewardXp;
                }
                if (quest.rewardGold > 0) {
                    p.coins += quest.rewardGold;
                }

                p.questsCompleted++;
                savePlayerToFS(p);

                // --------------------------------------------------------
                // SHOW REWARD MESSAGE (unified, friendly)
                // --------------------------------------------------------
                if (quest.rewardXp > 0 || quest.rewardGold > 0) {
                    p.client.println("Quest complete: " + quest.name + 
                        ". You feel more experienced and wealthy.");
                } else {
                    p.client.println("Quest complete: " + quest.name + ".");
                }

                // --------------------------------------------------------
                // AUTO-KILL NPC IF SPECIFIED
                // --------------------------------------------------------
                if (quest.unloadNpcId.length() > 0) {

                    for (auto &n : npcInstances) {

                        if (!n.alive) continue;
                        if (n.hp <= 0) continue;

                        // Must be in same room
                        if (n.x != p.roomX || n.y != p.roomY || n.z != p.roomZ)
                            continue;

                        // Match by NPC ID (not name)
                        if (n.npcId != quest.unloadNpcId)
                            continue;

                        // ⭐ Silent vanish
                        n.suppressDeathMessage = true;
                        n.hp = 0;
                        n.alive = false;

                        // Schedule respawn
                        n.respawnTime = millis() + (30UL * 60UL * 1000UL);

                        // Clear hostility
                        for (int i = 0; i < MAX_PLAYERS; i++)
                            n.hostileTo[i] = false;

                        n.targetPlayer = -1;

                        // ⭐ Reset immediately so next combat death is normal
                        n.suppressDeathMessage = false;

                        break;
                    }
                }

                // --------------------------------------------------------
                // Return to caller (quest step was completed)
                // --------------------------------------------------------

            }
        }

        return true; // event handled
    }

    return false; // no quest step matched
}


bool createRoomsIDX() {
    File txt = LittleFS.open("/rooms.txt", "r");
    if (!txt) return false;

    File idx = LittleFS.open("/rooms.idx", "w");
    if (!idx) {
        txt.close();
        return false;
    }

    // ---- Skip header ----
    txt.readStringUntil('\n');

    std::vector<RoomIndex> entries;

    while (txt.available()) {
        uint32_t offset = txt.position();   // EXACT byte offset

        String line = txt.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        int c1 = line.indexOf(',');
        int c2 = line.indexOf(',', c1 + 1);
        int c3 = line.indexOf(',', c2 + 1);

        if (c1 < 0 || c2 < 0 || c3 < 0) continue;

        int x = line.substring(0, c1).toInt();
        int y = line.substring(c1 + 1, c2).toInt();
        int z = line.substring(c2 + 1, c3).toInt();

        uint64_t key = packVoxelKey(x, y, z);
        entries.push_back({ key, offset });
    }

    txt.close();

    // ---- SORT FOR BINARY SEARCH ----
    std::sort(entries.begin(), entries.end(),
        [](const RoomIndex& a, const RoomIndex& b) {
            return a.key < b.key;
        });

    // ---- WRITE IDX ----
    for (auto& e : entries) {
        idx.printf("%llu,%u\n",
                   (unsigned long long)e.key,
                   e.offset);
    }

    idx.close();

    Serial.printf("[IDX] %d records written\n", entries.size());
    return true;
}

// =============================
// Quest helpers and core logic
// =============================

// Map questId (1–10) to array index (0–9)
int questIdToIndex(int questId) {
    if (questId < 1 || questId > 10) return -1;
    return questId - 1;
}

// Map step number (1–10) to array index (0–9)
int stepToIndex(int step) {
    if (step < 1 || step > 10) return -1;
    return step - 1;
}

void checkQuestCompletion(Player &p, int questId);  // forward

void unloadQuestNpc(const String &npcId) {
    if (npcId.length() == 0) return;

    for (auto &npc : npcInstances) {
        if (npc.npcId == npcId && npc.alive) {
            npc.alive = false;
            npc.hp = 0;
            npc.x = npc.y = npc.z = -9999;   // remove from world
        }
    }
}

void markQuestStep(Player &p, const QuestStep &step) {
    int qIdx = questIdToIndex(step.questId);
    int sIdx = stepToIndex(step.step);

    if (qIdx < 0 || qIdx >= 10) return;
    if (sIdx < 0 || sIdx >= 10) return;

    // Only mark if not already done
    if (!p.questStepDone[qIdx][sIdx]) {
        p.questStepDone[qIdx][sIdx] = true;
    }

    // After marking, check if the quest is now complete
    checkQuestCompletion(p, step.questId);
}

void checkQuestCompletion(Player &p, int questId) {
    auto it = quests.find(questId);
    if (it == quests.end()) return;

    int qIdx = questIdToIndex(questId);
    if (qIdx < 0 || qIdx >= 10) return;

    if (p.questCompleted[qIdx]) return; // already done

    QuestDef &qd = it->second;

    if (qd.steps.empty()) return;

    bool allDone = true;
    for (const QuestStep &step : qd.steps) {
        int sIdx = stepToIndex(step.step);
        if (sIdx < 0 || sIdx >= 10) continue;
        if (!p.questStepDone[qIdx][sIdx]) {
            allDone = false;
            break;
        }
    }

    if (!allDone) return;

    // Mark quest complete
    p.questCompleted[qIdx] = true;
    p.questsCompleted++;

    // ⭐ QUEST REWARDS
    if (qd.rewardXp > 0) {
        p.xp += qd.rewardXp;
        p.client.println("You gain " + String(qd.rewardXp) + " experience points!");
    }

    if (qd.rewardGold > 0) {
        p.coins += qd.rewardGold;
        if (qd.rewardGold == 1)
            p.client.println("You receive 1 gold coin!");
        else
            p.client.println("You receive " + String(qd.rewardGold) + " gold coins!");
    }

    // Show completion dialog
    if (qd.completionDialog.length()) {
        p.client.println(qd.completionDialog);
    } else {
        p.client.println("You have completed a quest!");
    }

    // Unload NPC if needed
    if (qd.unloadNpcId.length()) {
        unloadQuestNpc(qd.unloadNpcId);
    }
}

// =============================
// Telnet debug broadcast
// =============================

void telnetDebug(const String &msg) {
    // Send debug output to ALL connected players
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (players[i].active && players[i].client.connected()) {
            players[i].client.println("[DBG] " + msg);
        }
    }
}

// =============================
// Binary room index builder
// =============================

void generateBinaryIndex() {
    Serial.println("[IDX] Generating Binary Index from sorted source...");

    File fSource = LittleFS.open("/rooms.txt", "r");
    // Open for fresh writing, no sorting logic needed here anymore
    File fBin = LittleFS.open("/rooms.bin", "w");

    if (!fSource || !fBin) {
        Serial.println("[ERROR] Could not open files for indexing!");
        return;
    }

    // Skip Header Line (x,y,z,name...)
    if (fSource.available()) fSource.readStringUntil('\n');

    int count = 0;
    while (fSource.available()) {
        unsigned long pos = fSource.position(); // Byte offset for FindVoxel
        String line = fSource.readStringUntil('\n');
        line.trim();

        if (line.length() < 10) continue;

        // Parse coordinates to create the key
        int c1 = line.indexOf(',');
        int c2 = line.indexOf(',', c1 + 1);
        int c3 = line.indexOf(',', c2 + 1);

        if (c3 != -1) {
            IndexRecord rec;
            // Matches your VB.NET (z << 32) | (y << 16) | x
            rec.key = packVoxelKey(
                line.substring(0, c1).toInt(),
                line.substring(c1 + 1, c2).toInt(),
                line.substring(c2 + 1, c3).toInt()
            );
            rec.offset = (uint32_t)pos;

            fBin.write((uint8_t*)&rec, sizeof(IndexRecord));

            count++;
            if (count % 500 == 0) {
                Serial.printf("Indexed %d rooms...\n", count);
                yield();
            }
        }
    }

    fBin.close();
    fSource.close();
    Serial.printf("[IDX] Complete. Total records: %d\n", count);
}

void buildRoomIndexesIfNeeded() {
    if (!LittleFS.exists("/rooms.txt")) {
        Serial.println("[FATAL] rooms.txt missing");
        return;
    }

    // Always rebuild (your request)
    LittleFS.remove("/rooms.idx");
    LittleFS.remove("/rooms.bin");

    Serial.println("[IDX] Building rooms.idx...");
    if (!createRoomsIDX()) {
        Serial.println("[IDX] Failed!");
        return;
    }

    Serial.println("[BIN] Building rooms.bin...");
    generateBinaryIndex();   // existing routine

    Serial.println("[IDX/BIN] Complete");
}






// =============================
// File upload handler
// =============================

String readLineFromSerial() {
    String line = "";
    unsigned long start = millis();

    while (millis() - start < 2000) {
        if (Serial.available()) {
            char c = Serial.read();
            if (c == '\n') {
                line.trim();
                return line;
            }
            line += c;
        }
        delay(1);
    }

    return "";
}



void startBinaryFileTransfer() {
    telnetDebug("startBinaryFileTransfer() ENTERED");

    // Read filename
    currentFilename = readLineFromSerial();
    telnetDebug("Filename read: '" + currentFilename + "'");

    // Read size
    String sizeStr = readLineFromSerial();
    telnetDebug("Size string read: '" + sizeStr + "'");
    expectedSize = sizeStr.toInt();
    telnetDebug("Parsed expected size: " + String(expectedSize));

    // Open file
    currentFile = LittleFS.open("/" + currentFilename, "w");
    if (!currentFile) {
        telnetDebug("ERROR: Could not open file for writing!");
        Serial.println("ERR_OPEN");
        return;
    }

    receivedSize = 0;
    receivingFile = true;

    Serial.println("READY");
    telnetDebug("READY sent to PC");
}

void processBinaryTransfer() {
    if (!receivingFile) return;

    telnetDebug("processBinaryTransfer() called (receivingFile=true)");
    telnetDebug("processBinaryTransfer() ENTERED");

    // Need at least 2 bytes for length prefix
    if (Serial.available() < 2) return;

    // Peek for debug
    int peekVal = Serial.peek();
    telnetDebug("Peek byte: " + String(peekVal));

    // Read 2‑byte big‑endian length
    int lenHi = Serial.read();
    int lenLo = Serial.read();
    int chunkLen = (lenHi << 8) | lenLo;

    telnetDebug("Chunk length: " + String(chunkLen));

    // Wait for full chunk
    unsigned long start = millis();
    while (Serial.available() < chunkLen && millis() - start < 2000) {
        delay(1);
    }

    if (Serial.available() < chunkLen) {
        telnetDebug("ERROR: Timeout waiting for chunk payload");
        return;
    }

    // Read chunk
    uint8_t buffer[512];
    int bytesRead = Serial.readBytes(buffer, chunkLen);
    telnetDebug("Bytes read: " + String(bytesRead));

    // Write to file
    int bytesWritten = currentFile.write(buffer, bytesRead);
    telnetDebug("Bytes written: " + String(bytesWritten));

    receivedSize += bytesWritten;
    telnetDebug("Received so far: " + String(receivedSize));

    // ACK to PC
    Serial.println("OK");
    telnetDebug("Sent OK to PC");

    // Check if complete
    if (receivedSize >= expectedSize) {
        telnetDebug("File transfer complete. Closing file.");
        currentFile.close();
        receivingFile = false;

        Serial.println("DONE");
        telnetDebug("Sent DONE to PC");
    }
}




// Search rooms.bin / rooms.txt for a voxel line
VoxelResult FindVoxel(int x, int y, int z) {
  VoxelResult res;
  res.line = "NOT_FOUND";
  uint64_t targetKey = packVoxelKey(x, y, z);
  unsigned long start = millis();

  File bin = LittleFS.open("/rooms.bin", "r");
  long foundOffset = -1;

  if (bin) {
    size_t recSize = sizeof(IndexRecord);
    long count = bin.size() / recSize;
    long low = 0, high = count - 1;

    while (low <= high) {
      long mid = (low + high) / 2;
      bin.seek(mid * recSize);
      IndexRecord rec;
      bin.read((uint8_t*)&rec, recSize);

      if (rec.key == targetKey) {
        foundOffset = rec.offset;
        break;
      }
      if (targetKey > rec.key) low = mid + 1;
      else high = mid - 1;
    }
    bin.close();
  }

  if (foundOffset == -1) {
    // Fallback to linear search of rooms.txt
    File f = LittleFS.open("/rooms.txt", "r");
    if (f) {
      if (f.available()) f.readStringUntil('\n'); // skip header
      while (f.available()) {
        unsigned long currentPos = f.position();
        String line = f.readStringUntil('\n');
        line.trim();

        int c1 = line.indexOf(',');
        int c2 = line.indexOf(',', c1 + 1);
        int c3 = line.indexOf(',', c2 + 1);
        if (c3 != -1) {
          int lx = line.substring(0, c1).toInt();
          int ly = line.substring(c1 + 1, c2).toInt();
          int lz = line.substring(c2 + 1, c3).toInt();
          uint64_t lineKey = packVoxelKey(lx, ly, lz);
          if (lineKey == targetKey) {
            foundOffset = currentPos;
            res.line = line;
            break;
          }
        }
      }
      f.close();
    }
  } else {
    File f = LittleFS.open("/rooms.txt", "r");
    if (f) {
      f.seek(foundOffset);
      res.line = f.readStringUntil('\n');
      f.close();
    }
  }

  res.line.trim();
  res.elapsed = millis() - start;
  return res;
}

// =============================
// CSV parsing for a Room line
// =============================
Room parseRoomCSV(const String &line) {
    Room r;   // DO NOT MEMSET — Room contains String members

    // Explicit initialization of all fields
    r.x = r.y = r.z = 0;
    r.name[0] = '\0';
    r.description[0] = '\0';

    r.exit_n  = 0;
    r.exit_s  = 0;
    r.exit_e  = 0;
    r.exit_w  = 0;
    r.exit_ne = 0;
    r.exit_nw = 0;
    r.exit_se = 0;
    r.exit_sw = 0;
    r.exit_u  = 0;
    r.exit_d  = 0;

    r.hasPortal = false;
    r.px = r.py = r.pz = 0;
    r.portalCommand[0] = '\0';
    r.portalText[0] = '\0';

    r.exitList = "";   // String initializes safely

    // -----------------------------
    // Helper: extract CSV field
    // -----------------------------
    auto getField = [&](int fieldIndex) -> String {
        int count = 0;
        int start = 0;
        for (int i = 0; i <= line.length(); i++) {
            if (i == line.length() || line[i] == ',') {
                if (count == fieldIndex) {
                    return line.substring(start, i);
                }
                start = i + 1;
                count++;
            }
        }
        return "";
    };

    // -----------------------------
    // Coordinates
    // -----------------------------
    r.x = getField(0).toInt();
    r.y = getField(1).toInt();
    r.z = getField(2).toInt();

    // -----------------------------
    // Name + Description
    // -----------------------------
    String name = getField(3);
    String desc = getField(4);

    strncpy(r.name, name.c_str(), sizeof(r.name) - 1);
    r.name[sizeof(r.name) - 1] = '\0';

    strncpy(r.description, desc.c_str(), sizeof(r.description) - 1);
    r.description[sizeof(r.description) - 1] = '\0';

    // -----------------------------
    // Exit flags (5..14)
    // -----------------------------
    r.exit_n  = getField(5).toInt();
    r.exit_s  = getField(6).toInt();
    r.exit_e  = getField(7).toInt();
    r.exit_w  = getField(8).toInt();
    r.exit_ne = getField(9).toInt();
    r.exit_nw = getField(10).toInt();
    r.exit_se = getField(11).toInt();
    r.exit_sw = getField(12).toInt();
    r.exit_u  = getField(13).toInt();
    r.exit_d  = getField(14).toInt();

    // -----------------------------
    // Portal (optional)
    // -----------------------------
    String spx = getField(15);
    if (spx.length() > 0) {
        r.hasPortal = true;
        r.px = spx.toInt();
        r.py = getField(16).toInt();
        r.pz = getField(17).toInt();

        String pCmd = getField(18);
        strncpy(r.portalCommand, pCmd.c_str(), sizeof(r.portalCommand) - 1);
        r.portalCommand[sizeof(r.portalCommand) - 1] = '\0';

        String pTxt = getField(19);
        strncpy(r.portalText, pTxt.c_str(), sizeof(r.portalText) - 1);
        r.portalText[sizeof(r.portalText) - 1] = '\0';
    }

    // -----------------------------
    // Build human-readable exitList
    // -----------------------------
    bool any = false;

    auto addExit = [&](const char* label, int flag) {
        if (flag) {
            if (any) r.exitList += ", ";
            r.exitList += label;
            any = true;
        }
    };

    addExit("north",     r.exit_n);
    addExit("south",     r.exit_s);
    addExit("east",      r.exit_e);
    addExit("west",      r.exit_w);
    addExit("northeast", r.exit_ne);
    addExit("northwest", r.exit_nw);
    addExit("southeast", r.exit_se);
    addExit("southwest", r.exit_sw);
    addExit("up",        r.exit_u);
    addExit("down",      r.exit_d);

    if (!any) r.exitList = "none";

    return r;
}

// =============================
// Room loading for a specific player
// =============================

//Room Helpers:
String oppositeDir(const String &dir) {
    if (dir == "north") return "south";
    if (dir == "south") return "north";
    if (dir == "east")  return "west";
    if (dir == "west")  return "east";
    if (dir == "northeast") return "southwest";
    if (dir == "northwest") return "southeast";
    if (dir == "southeast") return "northwest";
    if (dir == "southwest") return "northeast";
    if (dir == "up") return "below";
    if (dir == "down") return "above";
    return "somewhere";
}


void announceToRoom(int x, int y, int z, const String &msg, int excludeIndex) {
    announceToRoomWrapped(x, y, z, msg, excludeIndex);
}

void announceToRoomExcept(int x, int y, int z, const String &msg, int excludeA, int excludeB) {
    String cleaned = ensurePunctuation(msg);
    String wrappedMsg = wordWrap(cleaned, MAX_OUTPUT_WIDTH);
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!players[i].active || !players[i].loggedIn) continue;
        if (i == excludeA || i == excludeB) continue;

        if (players[i].roomX == x &&
            players[i].roomY == y &&
            players[i].roomZ == z) {
            // Print each line separately to avoid client-side indentation
            int start = 0;
            for (int j = 0; j <= wrappedMsg.length(); j++) {
                if (j == wrappedMsg.length() || wrappedMsg[j] == '\n') {
                    String line = wrappedMsg.substring(start, j);
                    players[i].client.println(line);
                    start = j + 1;
                }
            }
        }
    }
}


bool loadRoomForPlayer(Player &p, int x, int y, int z) {
  VoxelResult vr = FindVoxel(x, y, z);
  if (vr.line == "NOT_FOUND") {
    return false;
  }

  Room r = parseRoomCSV(vr.line);

  // Assign to player's current room
  p.currentRoom = r;
  p.roomX = r.x;
  p.roomY = r.y;
  p.roomZ = r.z;

  // Track this voxel as visited
  uint64_t voxelKey = packVoxelKey(x, y, z);
  bool alreadyVisited = false;
  for (const auto &visited : p.visitedVoxels) {
    if (visited.x == x && visited.y == y && visited.z == z) {
      alreadyVisited = true;
      break;
    }
  }
  
  if (!alreadyVisited && p.visitedVoxels.size() < Player::MAX_VISITED_VOXELS) {
    p.visitedVoxels.push_back({x, y, z});
  }

  return true;
}

// =============================
// Simple LittleFS diagnostics
// =============================

void checkSpace() {
  size_t total = LittleFS.totalBytes();
  size_t used  = LittleFS.usedBytes();
  Serial.printf("Total space: %d bytes\n", total);
  Serial.printf("Used space:  %d bytes\n", used);
  Serial.printf("Free space:  %d bytes\n", total - used);
}

// =============================
// Item definition loaders
// =============================


void resetPlayer(Player &p) {
    memset(&p, 0, sizeof(Player));

    p.active = false;
    p.loggedIn = false;
    p.invCount = 0;
    p.wieldedItemIndex = -1;

    for (int s = 0; s < SLOT_COUNT; s++)
        p.wornItemIndices[s] = -1;

    for (int i = 0; i < 32; i++)
        p.invIndices[i] = -1;

    p.EnterMsg = "";
    p.ExitMsg = "";
}

// =============================
// Timezone Detection
// =============================

// Fetch timezone offset from worldtimeapi.org
// Returns true if successful, false if failed (will use hardcoded EST)
bool fetchTimezoneFromServer() {
    Serial.println("[TIMEZONE] Attempting to fetch timezone from time server...");
    
    WiFiClient client;
    if (!client.connect("worldtimeapi.org", 80)) {
        Serial.println("[TIMEZONE] Failed to connect to worldtimeapi.org");
        return false;
    }
    
    // Request timezone info based on IP address
    client.print("GET /api/ip HTTP/1.1\r\n");
    client.print("Host: worldtimeapi.org\r\n");
    client.print("Connection: close\r\n");
    client.print("\r\n");
    
    // Read response
    String response = "";
    unsigned long timeout = millis() + 5000;  // 5 second timeout
    
    while (client.connected() && millis() < timeout) {
        if (client.available()) {
            char c = client.read();
            response += c;
        }
    }
    
    client.stop();
    
    // Parse JSON response for utc_offset
    // Look for: "utc_offset":"-05:00"
    int offsetIdx = response.indexOf("\"utc_offset\"");
    if (offsetIdx < 0) {
        Serial.println("[TIMEZONE] Could not find utc_offset in response");
        return false;
    }
    
    // Find the value after the colon
    int colonIdx = response.indexOf(":", offsetIdx);
    if (colonIdx < 0) {
        Serial.println("[TIMEZONE] Malformed offset value");
        return false;
    }
    
    // Extract offset string like "-05:00" or "+02:00"
    int quoteIdx = response.indexOf("\"", colonIdx);
    if (quoteIdx < 0) {
        Serial.println("[TIMEZONE] Malformed offset value");
        return false;
    }
    
    String offsetStr = response.substring(colonIdx + 2, quoteIdx);
    
    // Parse offset: "-05:00" -> -5 hours
    int sign = (offsetStr[0] == '-') ? -1 : 1;
    int hours = offsetStr.substring(1, 3).toInt();
    int minutes = offsetStr.substring(4, 6).toInt();
    
    timezoneOffsetSeconds = sign * (hours * 3600 + minutes * 60);
    
    Serial.print("[TIMEZONE] Successfully fetched timezone offset: ");
    Serial.print(offsetStr);
    Serial.print(" (");
    Serial.print(timezoneOffsetSeconds / 3600);
    Serial.println(" hours)");
    
    return true;
}

// Initialize timezone on startup
void initializeTimezone() {
    // Try to fetch from time server
    if (!fetchTimezoneFromServer()) {
        // Fall back to hardcoded EST
        timezoneOffsetSeconds = -5 * 3600;
        Serial.println("[TIMEZONE] Using hardcoded EST timezone (UTC-5)");
    }
}

// Format compile time with timezone info
// Takes compile time string (HH:MM:SS) and returns "HH:MM:SS UTC (HH:MM:SS AM/PM EST)"
String formatCompileTimeWithTimezone(const char* timeStr) {
    // Parse compile time from the __TIME__ macro format (HH:MM:SS)
    // __TIME__ is in the compiler's local timezone (EST in this case)
    int hours = 0, minutes = 0, seconds = 0;
    sscanf(timeStr, "%d:%d:%d", &hours, &minutes, &seconds);
    
    // Create a time_t for today at the compile time (LOCAL timezone from __TIME__)
    time_t now = time(nullptr);
    struct tm* timeinfo = gmtime(&now);
    timeinfo->tm_hour = hours;
    timeinfo->tm_min = minutes;
    timeinfo->tm_sec = seconds;
    time_t compileTimeLocal = mktime(timeinfo);
    
    // Convert local compile time to UTC by subtracting the timezone offset
    // Since timezoneOffsetSeconds is negative for EST, subtracting it adds hours
    time_t compileTimeUTC = compileTimeLocal - timezoneOffsetSeconds;
    
    // Format UTC
    char utcStr[16];
    strftime(utcStr, sizeof(utcStr), "%H:%M:%S", gmtime(&compileTimeUTC));
    
    // Format local time by converting UTC back to local: local = UTC + offset
    // For EST with offset -18000: local = UTC + (-18000) = UTC - 5 hours
    time_t compileTimeLocalFormatted = compileTimeUTC + timezoneOffsetSeconds;
    struct tm* localinfo = gmtime(&compileTimeLocalFormatted);
    char localStr[32];
    strftime(localStr, sizeof(localStr), "%I:%M:%S %p", localinfo);
    
    // Get timezone abbreviation
    String tzName = "EST";
    int offsetHours = timezoneOffsetSeconds / 3600;
    if (offsetHours != -5) {
        tzName = "TZ" + String(offsetHours);
    }
    
    return String(utcStr) + " UTC (" + String(localStr) + " " + tzName + ")";
}

// =============================
// Timezone and Time Formatting
// =============================

// Convert UTC time to local time string with timezone abbreviation
// Returns formatted string like "11:29:48 UTC (11:29:48 AM EST)"
String formatTimeWithTimezone(time_t utcTime) {
    // UTC time
    struct tm* gmtimeinfo = gmtime(&utcTime);
    char utcStr[16];
    strftime(utcStr, sizeof(utcStr), "%H:%M:%S", gmtimeinfo);
    
    // Calculate local time using global timezone offset
    time_t localTime = utcTime + timezoneOffsetSeconds;
    struct tm* localinfo = gmtime(&localTime);
    char localStr[32];
    strftime(localStr, sizeof(localStr), "%I:%M:%S %p", localinfo);
    
    // Get timezone abbreviation
    String tzName = "EST";
    int offsetHours = timezoneOffsetSeconds / 3600;
    if (offsetHours != -5) {
        tzName = "TZ" + String(offsetHours);
    }
    
    return String(utcStr) + " UTC (" + String(localStr) + " " + tzName + ")";
}

// Format UTC date and time with local timezone
String formatDateTimeWithTimezone(time_t utcTime) {
    // UTC date and time
    struct tm* gmtimeinfo = gmtime(&utcTime);
    char utcStr[32];
    strftime(utcStr, sizeof(utcStr), "%Y-%m-%d %H:%M:%S", gmtimeinfo);
    
    // Calculate local time using global timezone offset: local = UTC + offset
    // For EST with offset -18000: local = UTC + (-18000) = UTC - 5 hours
    time_t localTime = utcTime + timezoneOffsetSeconds;
    struct tm* localinfo = gmtime(&localTime);
    char timeStr[32];
    strftime(timeStr, sizeof(timeStr), "%I:%M:%S %p", localinfo);
    
    // Get timezone abbreviation
    String tzName = "EST";
    int offsetHours = timezoneOffsetSeconds / 3600;
    if (offsetHours != -5) {
        tzName = "TZ" + String(offsetHours);
    }
    
    return String(utcStr) + " UTC (" + String(timeStr) + " " + tzName + ")";
}

// =============================
// Session Logging
// =============================

void logSessionLogin(const char* playerName) {
    File logFile = LittleFS.open("/session_log.txt", "a");
    if (!logFile) {
        Serial.println("[ERROR] Could not open session_log.txt for writing");
        return;
    }
    
    time_t now = time(nullptr);
    String timestamp = formatDateTimeWithTimezone(now);
    
    String logEntry = String(timestamp) + " | LOGIN  | " + String(playerName) + "\n";
    logFile.print(logEntry);
    logFile.close();
}

void logSessionLogout(const char* playerName) {
    File logFile = LittleFS.open("/session_log.txt", "a");
    if (!logFile) {
        Serial.println("[ERROR] Could not open session_log.txt for writing");
        return;
    }
    
    time_t now = time(nullptr);
    String timestamp = formatDateTimeWithTimezone(now);
    
    String logEntry = String(timestamp) + " | LOGOUT | " + String(playerName) + "\n";
    logFile.print(logEntry);
    logFile.close();
}

// =============================
// Time Synchronization
// =============================

void syncTimeFromNTP() {
    // Configure time with NTP servers
    // Using Google's NTP servers and fallback to pool.ntp.org
    configTime(0, 0, "time.google.com", "time.cloudflare.com", "pool.ntp.org");
    
    // Set timezone to UTC (you can change this if needed)
    // For EST: "EST5EDT,M3.2.0,M11.1.0"
    // For PST: "PST8PDT,M3.2.0,M11.1.0"
    // For UTC: "UTC"
    setenv("TZ", "UTC", 1);
    tzset();
    
    Serial.println("[TIME] Syncing time from NTP servers...");
    
    // Wait up to 10 seconds for time to sync
    unsigned long start = millis();
    while (millis() - start < 10000) {
        time_t now = time(nullptr);
        struct tm* timeinfo = localtime(&now);
        
        // Check if time is reasonable (after year 2020)
        if (timeinfo->tm_year > 120) {  // 120 = 2020 - 1900
            char timestamp[32];
            strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", timeinfo);
            Serial.println("[TIME] Time synced: " + String(timestamp));
            return;
        }
        delay(100);
    }
    
    Serial.println("[TIME] Failed to sync time from NTP (timeout)");
}

void resetWorldState() {
    // Remove all world item instances
    worldItems.clear();

    // Remove all NPC instances
    npcInstances.clear();
}


void loadAllItemDefinitions() {
    File f = LittleFS.open("/items.vxd", "r");
    if (!f) {
        Serial.println("[ERROR] Cannot open /items.vxd");
        return;
    }

    int count = 0;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        if (line.startsWith("#")) continue;

        loadItemDefinitions(line);
        count++;
    }

    f.close();

    Serial.printf("[ITEMS] Loaded %d item definitions. itemDefs.size()=%d\n",
                  count, itemDefs.size());
}


void loadAllNPCDefinitions() {
    File f = LittleFS.open("/npcs.vxd", "r");
    if (!f) {
        Serial.println("[ERROR] Cannot open /npcs.vxd");
        return;
    }

    int count = 0;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;
        if (line.startsWith("#")) continue;

        loadNPCDefinitions(line);
        count++;
    }

    f.close();

    Serial.printf("[NPCS] Loaded %d NPC definitions. npcDefs.size()=%d\n",
                  count, npcDefs.size());
}



void loadItemDefinitions(String line) {
    line.trim();
    if (line.length() == 0) return;
    if (line.startsWith("#")) return;

    int firstPipe  = line.indexOf('|');
    int secondPipe = line.indexOf('|', firstPipe + 1);
    if (firstPipe < 0) return;

    String itemNameStr = line.substring(0, firstPipe);
    itemNameStr.trim();
    std::string itemName = itemNameStr.c_str();

    ItemDefinition def;

    // Parse type into a String first, then assign to std::string
    String typeStr;
    if (secondPipe < 0) {
        typeStr = line.substring(firstPipe + 1);
    } else {
        typeStr = line.substring(firstPipe + 1, secondPipe);
    }
    typeStr.trim();
    def.type = typeStr.c_str();

    // --- Parse attributes ---
    if (secondPipe != -1) {
        String attrPart = line.substring(secondPipe + 1);
        int start = 0;

        while (start < attrPart.length()) {
            int end = attrPart.indexOf('|', start);
            if (end == -1) end = attrPart.length();

            String pair = attrPart.substring(start, end);
            pair.trim();

            if (pair.length() > 0) {
                int eq = pair.indexOf('=');
                if (eq > 0 && eq < pair.length() - 1) {
                    String keyStr = pair.substring(0, eq);
                    String valStr = pair.substring(eq + 1);

                    keyStr.trim();
                    valStr.trim();

                    keyStr.toLowerCase();

                    std::string key = keyStr.c_str();
                    std::string val = valStr.c_str();

                    def.attributes[key] = val;
                }
            }

            start = end + 1;
        }
    }

    itemDefs[itemName] = def;
}


void loadNPCDefinitions(String line) {
    line.trim();
    if (line.length() == 0) return;
    if (line.startsWith("#")) return;

    int firstPipe  = line.indexOf('|');
    int secondPipe = line.indexOf('|', firstPipe + 1);
    if (firstPipe < 0) return;

    String npcNameStr = line.substring(0, firstPipe);
    npcNameStr.trim();
    std::string npcName = npcNameStr.c_str();

    NpcDefinition def;

    // Parse type into a String first, then assign to std::string
    String typeStr;
    if (secondPipe < 0) {
        typeStr = line.substring(firstPipe + 1);
    } else {
        typeStr = line.substring(firstPipe + 1, secondPipe);
    }
    typeStr.trim();
    def.type = typeStr.c_str();

    // --- Parse attributes ---
    if (secondPipe != -1) {
        String attrPart = line.substring(secondPipe + 1);
        int start = 0;

        while (start < attrPart.length()) {
            int end = attrPart.indexOf('|', start);
            if (end == -1) end = attrPart.length();

            String pair = attrPart.substring(start, end);
            pair.trim();

            if (pair.length() > 0) {
                int eq = pair.indexOf('=');
                if (eq > 0 && eq < pair.length() - 1) {
                    String keyStr = pair.substring(0, eq);
                    String valStr = pair.substring(eq + 1);

                    keyStr.trim();
                    valStr.trim();

                    keyStr.toLowerCase();

                    std::string key = keyStr.c_str();
                    std::string val = valStr.c_str();

                    def.attributes[key] = val;
                }
            }

            start = end + 1;
        }
    }

    npcDefs[npcName] = def;
}



// =============================================================
// LOAD WORLD ITEMS (VXI)
// =============================================================

void linkWorldItemParents() {
    // Clear children
    for (auto &wi : worldItems) {
        wi.children.clear();
    }

    // Build name → indices map
    std::map<std::string, std::vector<int>> byName;
    for (int i = 0; i < (int)worldItems.size(); i++) {
        std::string key = worldItems[i].name.c_str();
        byName[key].push_back(i);
    }

    // Link children based on parentName
    for (int i = 0; i < (int)worldItems.size(); i++) {
        WorldItem &wi = worldItems[i];
        if (wi.parentName.length() == 0) continue;

        std::string parentKey = wi.parentName.c_str();
        auto it = byName.find(parentKey);
        if (it == byName.end()) continue;

        for (int parentIndex : it->second) {
            if (parentIndex < 0 || parentIndex >= (int)worldItems.size()) continue;
            worldItems[parentIndex].children.push_back(i);
        }
    }

    Serial.println("World item parent/child links rebuilt.");
}

void loadWorldItemLine(const String &line) {
    // Expected formats:
    //   x,y,z,itemName,value
    //   x,y,z,childName->parentName,value

    int p1 = line.indexOf(',');
    int p2 = line.indexOf(',', p1 + 1);
    int p3 = line.indexOf(',', p2 + 1);
    int p4 = line.lastIndexOf(',');

    if (p1 < 0 || p2 < 0 || p3 < 0 || p4 <= p3) return;

    int x = line.substring(0, p1).toInt();
    int y = line.substring(p1 + 1, p2).toInt();
    int z = line.substring(p2 + 1, p3).toInt();

    String itemPart = line.substring(p3 + 1, p4);
    String valuePart = line.substring(p4 + 1);

    itemPart.trim();
    valuePart.trim();

    int value = valuePart.toInt();

    String itemName;
    String parentName;

    int arrow = itemPart.indexOf("->");
    if (arrow >= 0) {
        itemName = itemPart.substring(0, arrow);
        parentName = itemPart.substring(arrow + 2);
    } else {
        itemName = itemPart;
        parentName = "";
    }

    itemName.trim();
    parentName.trim();

    // Create world item
    WorldItem wi;
    wi.name = itemName;
    wi.x = x;
    wi.y = y;
    wi.z = z;
    wi.ownerName = "";
    wi.parentName = parentName;
    wi.value = value;

    worldItems.push_back(wi);
}



void loadFileIntoItems() {
    File f = LittleFS.open("/items.vxi", "r");
    if (!f) {
        Serial.println("No items.vxi found — starting with empty world.");
        return;
    }

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        loadWorldItemLine(line);
    }

    f.close();
}



void loadAllWorldItems() {
    worldItems.clear();

    // 1. Load static items from items.vxi
    loadFileIntoItems();

    // 2. Overlay saved world state
    loadWorldItemsFromSave();

    // 3. Rebuild parent/child links
    linkWorldItemParents();

    Serial.print("Loaded world items: ");
    Serial.println(worldItems.size());
}







void loadAllNPCInstances() {
    File f = LittleFS.open("/npcs.vxi", "r");
    if (!f) {
        Serial.println("ERROR: Could not open /npcs.vxi");
        return;
    }

    Serial.println("Loading NPC instances from /npcs.vxi...");

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        loadNPCInstance(line);
    }

    f.close();

    Serial.printf("Loaded %d NPC instances.\n", npcInstances.size());
}


String resolveDisplayName(const WorldItem &wi) {
    auto it = itemDefs.find(std::string(wi.name.c_str()));
    if (it != itemDefs.end()) {
        auto ait = it->second.attributes.find("name");
        if (ait != it->second.attributes.end()) {
            String disp(ait->second.c_str());
            disp.trim();
            if (disp.length() > 0)
                return disp;
        }
    }
    return wi.name;
}

int findWorldItemByName(const String &name) {
    String n = name;
    n.toLowerCase();

    for (int i = 0; i < (int)worldItems.size(); i++) {
        String base = worldItems[i].name;
        base.toLowerCase();

        if (base == n)
            return i;
    }
    return -1;
}


String getItemDisplayName(const WorldItem &wi) {
    // GOLD PILES - special case
    String nameCheck = wi.name;
    nameCheck.toLowerCase();
    if (nameCheck == "gold_coin" || nameCheck == "gold coins" || nameCheck == "gold coin") {
        int value = wi.value;
        if (value == 1) return "1 gold coin";
        return String(value) + " gold coins";
    }

    // First check if item has a "name" attribute in itemDefs
    String attrName = wi.getAttr("name", itemDefs);
    if (attrName.length() > 0) {
        return attrName;
    }

    // Fallback: format the raw item name (convert underscores to spaces, capitalize)
    String name = wi.name;
    name.trim();
    name.toLowerCase();

    String out = "";
    bool cap = true;

    for (int i = 0; i < name.length(); i++) {
        char c = name[i];

        if (c == '_') {
            out += ' ';
            cap = true;
            continue;
        }

        if (cap) {
            out += (char)toupper(c);
            cap = false;
        } else {
            out += c;
        }
    }

    return out;
}

String getItemDisplayName(const String &raw) {
    String name = raw;
    name.trim();
    name.toLowerCase();

    // GENERIC ITEM NAME FORMATTING ONLY
    String out = "";
    bool cap = true;

    for (int i = 0; i < name.length(); i++) {
        char c = name[i];

        if (c == '_') {
            out += ' ';
            cap = true;
            continue;
        }

        if (cap) {
            out += (char)toupper(c);
            cap = false;
        } else {
            out += c;
        }
    }

    return out;
}

/**
 * Word-wrap text to MAX_OUTPUT_WIDTH columns without breaking words
 * Preserves existing newlines and handles multiple spaces
 * Trims trailing spaces from each wrapped line
 */
String wordWrap(const String &text, int width = MAX_OUTPUT_WIDTH) {
    if (width <= 0) width = MAX_OUTPUT_WIDTH;
    
    String result = "";
    String currentLine = "";
    String word = "";
    
    for (int i = 0; i < text.length(); i++) {
        char c = text[i];
        
        // Handle newlines - preserve them
        if (c == '\n') {
            // Finish current word if any
            if (!word.isEmpty()) {
                if (currentLine.isEmpty()) {
                    currentLine = word;
                } else if ((int)(currentLine.length() + 1 + word.length()) <= width) {
                    currentLine += " " + word;
                } else {
                    // Trim trailing spaces and add line
                    while (currentLine.length() > 0 && currentLine[currentLine.length() - 1] == ' ') {
                        currentLine = currentLine.substring(0, currentLine.length() - 1);
                    }
                    result += currentLine + "\n";
                    currentLine = word;
                }
                word = "";
            }
            
            // Trim trailing spaces before adding newline
            while (currentLine.length() > 0 && currentLine[currentLine.length() - 1] == ' ') {
                currentLine = currentLine.substring(0, currentLine.length() - 1);
            }
            result += currentLine + "\n";
            currentLine = "";
            continue;
        }
        
        // Handle spaces - they mark word boundaries
        if (c == ' ') {
            if (!word.isEmpty()) {
                if (currentLine.isEmpty()) {
                    // Start of line - just add the word
                    currentLine = word;
                } else if ((int)(currentLine.length() + 1 + word.length()) <= width) {
                    // Word fits on current line with space
                    currentLine += " " + word;
                } else {
                    // Word doesn't fit - start new line (trim trailing spaces first)
                    while (currentLine.length() > 0 && currentLine[currentLine.length() - 1] == ' ') {
                        currentLine = currentLine.substring(0, currentLine.length() - 1);
                    }
                    result += currentLine + "\n";
                    currentLine = word;
                }
                word = "";
            }
            continue;
        }
        
        // Accumulate characters into word
        word += c;
    }
    
    // Handle remaining word
    if (!word.isEmpty()) {
        if (currentLine.isEmpty()) {
            currentLine = word;
        } else if ((int)(currentLine.length() + 1 + word.length()) <= width) {
            currentLine += " " + word;
        } else {
            // Trim trailing spaces before adding line
            while (currentLine.length() > 0 && currentLine[currentLine.length() - 1] == ' ') {
                currentLine = currentLine.substring(0, currentLine.length() - 1);
            }
            result += currentLine + "\n";
            currentLine = word;
        }
    }
    
    // Add final line (trim trailing spaces)
    while (currentLine.length() > 0 && currentLine[currentLine.length() - 1] == ' ') {
        currentLine = currentLine.substring(0, currentLine.length() - 1);
    }
    if (!currentLine.isEmpty()) result += currentLine;
    
    return result;
}

/**
 * Ensure text ends with punctuation (.?!")
 * If it doesn't, add a period (unless it ends with a quote)
 */
String ensurePunctuation(const String &text) {
    String trimmed = text;
    // Trim trailing spaces
    while (trimmed.length() > 0 && trimmed[trimmed.length() - 1] == ' ') {
        trimmed = trimmed.substring(0, trimmed.length() - 1);
    }
    
    if (trimmed.length() == 0) return trimmed;
    
    char lastChar = trimmed[trimmed.length() - 1];
    if (lastChar == '.' || lastChar == '?' || lastChar == '!' || lastChar == '"') {
        return trimmed;
    }
    
    return trimmed + ".";
}

/**
 * Unified function to print wrapped text line-by-line to a client
 * Handles wrapping and ensures no leading spaces on continuation lines
 */
void printWrappedLines(Client &client, const String &text, int width = MAX_OUTPUT_WIDTH) {
    String cleaned = ensurePunctuation(text);
    String wrappedMsg = wordWrap(cleaned, width);
    
    int start = 0;
    for (int j = 0; j <= wrappedMsg.length(); j++) {
        if (j == wrappedMsg.length() || wrappedMsg[j] == '\n') {
            String line = wrappedMsg.substring(start, j);
            client.println(line);
            start = j + 1;
        }
    }
}

/**
 * Unified function to announce text to a room with proper wrapping
 * Ensures no leading spaces on continuation lines
 */
void announceToRoomWrapped(int x, int y, int z, const String &msg, int excludeIndex = -1) {
    String cleaned = ensurePunctuation(msg);
    String wrappedMsg = wordWrap(cleaned, MAX_OUTPUT_WIDTH);
    
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!players[i].active || !players[i].loggedIn) continue;
        if (i == excludeIndex) continue;

        if (players[i].roomX == x &&
            players[i].roomY == y &&
            players[i].roomZ == z) {
            // Print each line separately to avoid client-side indentation
            int start = 0;
            for (int j = 0; j <= wrappedMsg.length(); j++) {
                if (j == wrappedMsg.length() || wrappedMsg[j] == '\n') {
                    String line = wrappedMsg.substring(start, j);
                    players[i].client.println(line);
                    start = j + 1;
                }
            }
        }
    }
}

/**
 * Print wrapped text to client, properly handling each wrapped line
 */
void printWrapped(Client &client, const String &text, int width = MAX_OUTPUT_WIDTH) {
    String wrapped = wordWrap(text, width);
    int pos = 0;
    
    while (pos < wrapped.length()) {
        int newlinePos = wrapped.indexOf('\n', pos);
        
        if (newlinePos == -1) {
            // No more newlines, print rest
            String line = wrapped.substring(pos);
            client.println(line);
            break;
        } else {
            // Print line up to newline
            String line = wrapped.substring(pos, newlinePos);
            client.println(line);
            pos = newlinePos + 1;
        }
    }
}
void recalcBonuses(Player &p) {
    applyEquipmentBonuses(p);
}



void applyEquipmentBonuses(Player &p) {
    p.armorBonus = 0;
    p.weaponBonus = 0;

    // -----------------------------
    // ARMOR BONUSES (all worn slots)
    // -----------------------------
    for (int s = 0; s < SLOT_COUNT; s++) {
        int idx = p.wornItemIndices[s];
        if (idx < 0 || idx >= (int)worldItems.size()) continue;

        WorldItem &wi = worldItems[idx];

        // Armor value from itemDefs
        String armorStr = wi.getAttr("armor", itemDefs);
        if (armorStr.length() > 0) {
            p.armorBonus += armorStr.toInt();
        }
    }

    // -----------------------------
    // WEAPON BONUS (single wielded item)
    // -----------------------------
    if (p.wieldedItemIndex >= 0 &&
        p.wieldedItemIndex < (int)worldItems.size()) {

        WorldItem &wi = worldItems[p.wieldedItemIndex];

        String dmgStr = wi.getAttr("damage", itemDefs);
        if (dmgStr.length() > 0) {
            p.weaponBonus += dmgStr.toInt();
        }
    }
}



// =============================================================
// LOAD WORLD ITEMS FROM SAVE FILE (world_items.vxi)
// =============================================================

void loadWorldItemsFromSave() {
    File f = LittleFS.open("/world_items.vxi", "r");
    if (!f) return;

    WorldItem temp;
    bool inItem = false;

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        if (line == "[ITEM]") {
            if (inItem) {
                bool found = false;
                for (auto &wi : worldItems) {
                    if (wi.name.equalsIgnoreCase(temp.name)) {
                        wi.x = temp.x;
                        wi.y = temp.y;
                        wi.z = temp.z;
                        wi.ownerName = temp.ownerName;
                        wi.parentName = temp.parentName;
                        wi.value = temp.value;
                        found = true;
                        break;
                    }
                }
                if (!found) worldItems.push_back(temp);
            }
            temp = WorldItem();
            inItem = true;
            continue;
        }

        int eq = line.indexOf('=');
        if (eq == -1) continue;

        String key = line.substring(0, eq);
        String val = line.substring(eq + 1);
        key.trim();
        val.trim();

        if (key == "name") temp.name = val;
        else if (key == "x") temp.x = val.toInt();
        else if (key == "y") temp.y = val.toInt();
        else if (key == "z") temp.z = val.toInt();
        else if (key == "ownerName") temp.ownerName = val;
        else if (key == "parentName") temp.parentName = val;
        else if (key == "value") temp.value = val.toInt();
    }

    if (inItem) {
        bool found = false;
        for (auto &wi : worldItems) {
            if (wi.name.equalsIgnoreCase(temp.name)) {
                wi.x = temp.x;
                wi.y = temp.y;
                wi.z = temp.z;
                wi.ownerName = temp.ownerName;
                wi.parentName = temp.parentName;
                wi.value = temp.value;
                found = true;
                break;
            }
        }
        if (!found) worldItems.push_back(temp);
    }

    f.close();
    
    // After loading, populate attributes from itemDefs
    for (auto &wi : worldItems) {
        auto it = itemDefs.find(std::string(wi.name.c_str()));
        if (it != itemDefs.end()) {
            wi.attributes = it->second.attributes;
        }
    }
}

void saveWorldItems() {
    File f = LittleFS.open("/world_items.vxi", "w");
    if (!f) return;

    for (auto &wi : worldItems) {

        // Save items that are:
        // 1. In the world (ownerName empty, valid coords), OR
        // 2. In containers (ownerName set, parentName set to container name)
        
        bool inWorld = (wi.ownerName.length() == 0) && 
                       (wi.x >= 0 && wi.y >= 0 && wi.z >= 0);
        
        bool inContainer = (wi.ownerName.length() > 0) && 
                           (wi.parentName.length() > 0);

        if (!inWorld && !inContainer) {
            continue;  // Skip top-level inventory items and invalid items
        }

        f.println("[ITEM]");
        f.println("name=" + wi.name);
        f.println("x=" + String(wi.x));
        f.println("y=" + String(wi.y));
        f.println("z=" + String(wi.z));
        f.println("ownerName=" + wi.ownerName);
        f.println("parentName=" + wi.parentName);
        f.println("value=" + String(wi.value));
        f.println();
    }

    f.close();
}


void cmdResetWorldItems(Player &p, const String &args) {

    // Wizard-only safety
    if (!p.IsWizard) {
        p.client.println("What?");
        return;
    }

    p.client.println("Resetting world items to default...");

    // ---------------------------------------------------------
    // 1. CLEAR ALL NON-INVENTORY WORLD ITEMS
    // ---------------------------------------------------------
    for (int i = 0; i < (int)worldItems.size(); ) {
        WorldItem &wi = worldItems[i];

        // Keep items owned by players
        if (wi.ownerName.length() > 0) {
            i++;
            continue;
        }

        // Remove everything else
        worldItems.erase(worldItems.begin() + i);
    }

    // ---------------------------------------------------------
    // 2. DELETE THE WORLD-ITEM SAVE FILE (LittleFS)
    // ---------------------------------------------------------
    if (LittleFS.exists("/world_items.vxi")) {
        LittleFS.remove("/world_items.vxi");
    }

    // ---------------------------------------------------------
    // 3. RELOAD DEFAULT WORLD ITEMS
    // ---------------------------------------------------------
    loadAllWorldItems();

    // ---------------------------------------------------------
    // 4. MERGE COIN PILES IN ALL ROOMS
    // ---------------------------------------------------------
    for (int i = 0; i < (int)worldItems.size(); i++) {
        WorldItem &wi = worldItems[i];
        if (wi.name == "gold_coin" && wi.ownerName.length() == 0) {
            mergeCoinPilesAt(wi.x, wi.y, wi.z);
        }
    }

    // ---------------------------------------------------------
    // 5. DONE
    // ---------------------------------------------------------
    broadcastToAll("The world shimmers and reforms as it returns to its original state.");
}




std::vector<NpcInstance*> getNPCsAt(int x, int y, int z) {
    std::vector<NpcInstance*> result;

    for (auto &npc : npcInstances) {
        if (!npc.alive) continue;
        if (npc.x == x && npc.y == y && npc.z == z) {
            result.push_back(&npc);
        }
    }

    return result;
}



void loadNPCInstance(const String &line) {
    // Expected format:
    // x,y,z,npcId,mobileFlag

    int parts[5];
    String fields[5];

    int idx = 0;
    int start = 0;

    // Split by commas
    for (int i = 0; i < line.length() && idx < 5; i++) {
        if (line[i] == ',') {
            fields[idx++] = line.substring(start, i);
            start = i + 1;
        }
    }
    // Last field
    if (idx < 5) {
        fields[idx++] = line.substring(start);
    }

    if (idx < 4) {
        Serial.println("[WARN] Bad NPC instance line: " + line);
        return;
    }

    int x = fields[0].toInt();
    int y = fields[1].toInt();
    int z = fields[2].toInt();
    String npcId = fields[3];
    int mobileFlag = (idx >= 5) ? fields[4].toInt() : 0;

    // Must have a definition (std::string key)
    std::string key = npcId.c_str();
    if (!npcDefs.count(key)) {
        Serial.println("[WARN] Unknown NPC ID in instance: " + npcId);
        return;
    }

    auto &def = npcDefs[key];

    NpcInstance npc;
    npc.npcId = npcId;

    // Position from instance file
    npc.x = x;
    npc.y = y;
    npc.z = z;

    npc.spawnX = x;
    npc.spawnY = y;
    npc.spawnZ = z;

    // Stats from definition (std::string → String → int)
    if (def.attributes.count("hp")) {
        npc.hp = String(def.attributes.at("hp").c_str()).toInt();
    } else {
        npc.hp = 1;
    }

    if (def.attributes.count("gold")) {
        npc.gold = String(def.attributes.at("gold").c_str()).toInt();
    } else {
        npc.gold = 0;
    }

    npc.alive = true;
    npc.respawnTime = 0;
    npc.suppressDeathMessage = false;
    npc.targetPlayer = -1;

    for (int i = 0; i < MAX_PLAYERS; i++)
        npc.hostileTo[i] = false;

    // Dialog setup
    npc.dialogIndex = 0;
    npc.dialogOrder[0] = 0;
    npc.dialogOrder[1] = 1;
    npc.dialogOrder[2] = 2;

    for (int i = 0; i < 3; i++) {
        int r = random(0, 3);
        int tmp = npc.dialogOrder[i];
        npc.dialogOrder[i] = npc.dialogOrder[r];
        npc.dialogOrder[r] = tmp;
    }

    npc.nextDialogTime = millis() + random(3000, 30001);

    npcInstances.push_back(npc);
}



// =============================================================
// MOBILE FLAG (for PUT)
// =============================================================

bool isMobileItem(const String &itemID) {
    auto it = itemDefs.find(std::string(itemID.c_str()));
    if (it == itemDefs.end()) return true; // default mobile

    auto &attrs = it->second.attributes;
    auto jt = attrs.find("mobile");
    if (jt == attrs.end()) return true;

    // std::string → String → int
    return (String(jt->second.c_str()).toInt() == 1);
}


// =============================================================
// CHILDREN OF AN ITEM
// =============================================================

void getChildrenOf(const String &parentID, std::vector<int> &out) {
    out.clear();
    for (int i = 0; i < (int)worldItems.size(); i++) {
        if (worldItems[i].parentName == parentID) {
            out.push_back(i);
        }
    }
}

// =============================================================
// MOVE ITEM + CHILDREN TO PLAYER INVENTORY
// =============================================================

void moveItemTreeToPlayer(Player &p, int wiIndex) {
    if (wiIndex < 0 || wiIndex >= (int)worldItems.size()) return;

    WorldItem &wi = worldItems[wiIndex];
    wi.ownerName = p.name;
    wi.x = wi.y = wi.z = 0;

    // Move children recursively
    std::vector<int> kids;
    getChildrenOf(wi.name, kids);
    for (int idx : kids) {
        worldItems[idx].ownerName = p.name;
        worldItems[idx].x = worldItems[idx].y = worldItems[idx].z = 0;
        moveItemTreeToPlayer(p, idx);
    }
}

// =============================================================
// MOVE ITEM + CHILDREN TO WORLD FLOOR
// =============================================================

void moveItemTreeToWorld(int wiIndex, int x, int y, int z) {
    if (wiIndex < 0 || wiIndex >= (int)worldItems.size()) return;

    WorldItem &wi = worldItems[wiIndex];
    wi.ownerName = "";
    wi.x = x;
    wi.y = y;
    wi.z = z;

    // Move children recursively
    std::vector<int> kids;
    getChildrenOf(wi.name, kids);
    for (int idx : kids) {
        worldItems[idx].ownerName = "";
        worldItems[idx].x = x;
        worldItems[idx].y = y;
        worldItems[idx].z = z;
        moveItemTreeToWorld(idx, x, y, z);
    }
}

// =============================================================
// FIND TOP-LEVEL ITEM ON FLOOR
// =============================================================

int findFloorItemIndex(Room &r, const char* search) {
    String s = String(search);
    s.toLowerCase();

    for (int i = 0; i < (int)worldItems.size(); i++) {
        WorldItem &wi = worldItems[i];
        if (wi.ownerName.length() != 0) continue;      // not in world
        if (wi.parentName.length() != 0) continue;     // child item
        if (wi.x != r.x || wi.y != r.y || wi.z != r.z) continue;

        String dName = wi.getAttr("name", itemDefs);
        if (dName.length() == 0) dName = wi.name;
        dName.toLowerCase();

        if (dName.indexOf(s) != -1) return i;
    }
    return -1;
}



// =============================
// Direction parsing and voxel movement
// =============================

String normalizeDir(const String &d) {
    if (d == "n")  return "north";
    if (d == "s")  return "south";
    if (d == "e")  return "east";
    if (d == "w")  return "west";
    if (d == "ne") return "northeast";
    if (d == "nw") return "northwest";
    if (d == "se") return "southeast";
    if (d == "sw") return "southwest";
    if (d == "u")  return "up";
    if (d == "d")  return "down";
    return d;
}

int dirFromString(const char *d) {
  if (!d) return -1;

  // Normalize input: ignore case and support short forms
  if (!strcasecmp(d,"north")     || !strcasecmp(d,"n"))  return DIR_NORTH;
  if (!strcasecmp(d,"south")     || !strcasecmp(d,"s"))  return DIR_SOUTH;
  if (!strcasecmp(d,"east")      || !strcasecmp(d,"e"))  return DIR_EAST;
  if (!strcasecmp(d,"west")      || !strcasecmp(d,"w"))  return DIR_WEST;

  if (!strcasecmp(d,"northeast") || !strcasecmp(d,"ne")) return DIR_NE;
  if (!strcasecmp(d,"northwest") || !strcasecmp(d,"nw")) return DIR_NW;
  if (!strcasecmp(d,"southeast") || !strcasecmp(d,"se")) return DIR_SE;
  if (!strcasecmp(d,"southwest") || !strcasecmp(d,"sw")) return DIR_SW;

  if (!strcasecmp(d,"up")        || !strcasecmp(d,"u"))  return DIR_UP;
  if (!strcasecmp(d,"down")      || !strcasecmp(d,"d"))  return DIR_DOWN;

  return -1;
}

void voxelDelta(int d, int &dx, int &dy, int &dz) {
  dx = dy = dz = 0;

  switch (d) {
    case DIR_NORTH: dy = -1; break;
    case DIR_SOUTH: dy =  1; break;
    case DIR_EAST:  dx =  1; break;
    case DIR_WEST:  dx = -1; break;

    case DIR_NE: dx =  1; dy = -1; break;
    case DIR_NW: dx = -1; dy = -1; break;
    case DIR_SE: dx =  1; dy =  1; break;
    case DIR_SW: dx = -1; dy =  1; break;

    case DIR_UP:   dz =  1; break;
    case DIR_DOWN: dz = -1; break;
  }
}

// =============================
// Utility: case-insensitive partial match
// =============================

bool isMatch(const char* itemDisplayName, const char* playerInput) {
  if (!itemDisplayName || !playerInput) return false;
  String item = String(itemDisplayName);
  String input = String(playerInput);
  item.toLowerCase();
  input.toLowerCase();

  return (item.indexOf(input) != -1);
}

// =============================
// Player inventory synchronization with worldItems
// =============================

void removeFromInventory(Player &p, int worldIndex) {
    for (int i = 0; i < p.invCount; i++) {
        if (p.invIndices[i] == worldIndex) {

            // Shift everything down one slot
            for (int j = i; j < p.invCount - 1; j++) {
                p.invIndices[j] = p.invIndices[j + 1];
            }

            p.invCount--;
            return;
        }
    }
}


// Rebuild p.invIndices[] from worldItems based on parentName = p.name
// This includes items inside containers in the player's inventory
void rebuildPlayerInventory(Player &p) {
  p.invCount = 0;
  for (int i = 0; i < (int)worldItems.size(); i++) {
    // Direct inventory item (not in a container)
    if (worldItems[i].parentName == p.name) {
      // NEVER allow gold coins in inventory - they should only go to p.coins
      if (worldItems[i].name == "gold_coin") continue;
      
      if (p.invCount < (int)(sizeof(p.invIndices)/sizeof(p.invIndices[0]))) {
        p.invIndices[p.invCount++] = i;
      }
    }
  }
}

// Find a WorldItem index in a player's inventory by partial display name
int findInventoryItemIndex(Player &p, const char* search) {
  for (int i = 0; i < p.invCount; i++) {
    int wiIndex = p.invIndices[i];
    if (wiIndex < 0 || wiIndex >= (int)worldItems.size()) continue;

    WorldItem &wi = worldItems[wiIndex];
    String displayName = wi.getAttr("name", itemDefs);
    if (displayName.length() == 0) displayName = wi.name;

    if (isMatch(displayName.c_str(), search)) {
      return wiIndex;
    }
  }
  return -1;
}

int findItemInRoomOrInventory(Player &p, const String &targetRaw) {
    // Normalize target
    String t = targetRaw;
    t.trim();
    t.toLowerCase();

    if (t.startsWith("at ")) {
        t = t.substring(3);
        t.trim();
    }

    // -----------------------------------------
    // Helper: full-word match (captures only t)
    // -----------------------------------------
    auto fullWordMatch = [&](const String &s) {
        if (s.length() == 0) return false;

        String lower = s;
        lower.toLowerCase();

        if (lower == t) return true;

        int start = 0;
        while (true) {
            int space = lower.indexOf(' ', start);
            String word = (space == -1)
                ? lower.substring(start)
                : lower.substring(start, space);

            word.trim();
            if (word == t) return true;

            if (space == -1) break;
            start = space + 1;
        }
        return false;
    };

    // -----------------------------------------
    // 1. SEARCH INVENTORY
    // -----------------------------------------
    for (int i = 0; i < p.invCount; i++) {
        int worldIndex = p.invIndices[i];
        if (worldIndex < 0 || worldIndex >= (int)worldItems.size())
            continue;

        WorldItem &wi = worldItems[worldIndex];

        String id   = wi.name;
        String disp = resolveDisplayName(wi);

        if (fullWordMatch(id) || fullWordMatch(disp)) {
            // Encode inventory: high bit + slot
            return (i | 0x80000000);
        }
    }

    // -----------------------------------------
    // 2. SEARCH ROOM
    // -----------------------------------------
    Room &r = p.currentRoom;

    for (int i = 0; i < (int)worldItems.size(); i++) {
        WorldItem &wi = worldItems[i];

        if (wi.x != r.x || wi.y != r.y || wi.z != r.z)
            continue;

        String id   = wi.name;
        String disp = resolveDisplayName(wi);

        if (fullWordMatch(id) || fullWordMatch(disp)) {
            return i;  // room item: direct worldIndex
        }
    }

    return -1;  // not found
}


void consumeWorldItem(Player &p, int worldIndex) {
    // Remove from inventory if present
    for (int i = 0; i < p.invCount; i++) {
        if (p.invIndices[i] == worldIndex) {

            // Shift down
            for (int j = i; j < p.invCount - 1; j++) {
                p.invIndices[j] = p.invIndices[j + 1];
            }

            p.invCount--;
            break;
        }
    }

    // Remove from world
    worldItems[worldIndex].x = -9999;
    worldItems[worldIndex].y = -9999;
    worldItems[worldIndex].z = -9999;
    worldItems[worldIndex].ownerName  = "__consumed__";
    worldItems[worldIndex].parentName = "";
}


void checkNPCAggro(Player &p) {

    // Resolve playerIndex
    int playerIndex = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (&players[i] == &p) {
            playerIndex = i;
            break;
        }
    }
    if (playerIndex == -1) return;

    auto npcsHere = getNPCsAt(p.roomX, p.roomY, p.roomZ);

    for (auto npc : npcsHere) {
        // SAFE lookup into npcDefs using std::string key
        std::string key = npc->npcId.c_str();
        auto it = npcDefs.find(key);
        if (it == npcDefs.end()) {
            continue;
        }

        auto &def = it->second;

        // ---------------------------------------------
        // PASSIVE NPC: resume combat if previously hostile
        // ---------------------------------------------
        if (npc->alive &&
            npc->hostileTo[playerIndex] &&
            npc->targetPlayer == playerIndex) {

            p.inCombat = true;
            p.combatTarget = npc;
            p.nextCombatTime = millis() + 1000;
            return;
        }

        // ---------------------------------------------
        // AGGRESSIVE NPC: auto-aggro on sight
        // ---------------------------------------------
        if (npc->alive &&
            def.attributes.count("aggressive") &&
            def.attributes.at("aggressive") == "1") {

            // ⭐ REQUIRED FOR COMBAT LOOP TO WORK ⭐
            npc->hostileTo[playerIndex] = true;
            npc->targetPlayer = playerIndex;

            // Start combat for THIS player
            p.inCombat = true;
            p.combatTarget = npc;
            p.nextCombatTime = millis() + 1000;

            String npcName = def.attributes.at("name").c_str();
            p.client.println("The " + npcName + " snarls and attacks you!");
            broadcastRoomExcept(
                p,
                "The " + npcName + " attacks " + capFirst(p.name) + "!",
                p
            );

            return; // only one aggro at a time
        }
    }
}


NpcInstance* findNPCInRoom(Player &p, const String &id) {
    for (auto &n : npcInstances) {
        if (!n.alive) continue;
        if (n.hp <= 0) continue;

        if (n.x == p.roomX && n.y == p.roomY && n.z == p.roomZ) {
            if (n.npcId == id) {   // ⭐ match by ID, not name
                return &n;
            }
        }
    }
    return nullptr;
}






// =============================
// Player equipment helpers (WorldItem-based)
// =============================

// Get integer attribute from itemDefs or default
int getItemIntAttr(const WorldItem &wi, const char* key, int defaultVal) {
  String v = wi.getAttr(key, itemDefs);
  if (v.length() == 0) return defaultVal;
  return v.toInt();
}

BodySlot getItemSlot(const WorldItem &wi) {
    // Read numeric slot from item attributes
    String s = wi.getAttr("slot", itemDefs);
    if (s.length() == 0)
        return SLOT_HANDS;  // fallback

    int slotIndex = s.toInt();

    if (slotIndex < 0 || slotIndex >= SLOT_COUNT)
        return SLOT_HANDS;  // fallback

    return (BodySlot)slotIndex;
}

// Check flags from attributes
bool isWearable(const WorldItem &wi) {
  String t = wi.getAttr("wearable", itemDefs);
  t.toLowerCase();
  return (t == "true" || t == "1" || t == "yes");
}

bool isWieldable(const WorldItem &wi) {
  String t = wi.getAttr("wieldable", itemDefs);
  t.toLowerCase();
  return (t == "true" || t == "1" || t == "yes");
}

bool isPortable(const WorldItem &wi) {
  String t = wi.getAttr("portable", itemDefs);
  t.toLowerCase();
  if (t.length() == 0) return true; // default portable
  return (t == "true" || t == "1" || t == "yes");
}

int getWeight(const WorldItem &wi) {
  return getItemIntAttr(wi, "weight", 1);
}

int getDamageBonus(const WorldItem &wi) {
  return getItemIntAttr(wi, "damage", 0);
}

int getArmorClass(const WorldItem &wi) {
  return getItemIntAttr(wi, "armor", 0);
}

// Total weight of an item and all its children
int getItemTreeWeight(int wiIndex) {
    if (wiIndex < 0 || wiIndex >= (int)worldItems.size()) return 0;

    WorldItem &root = worldItems[wiIndex];
    int total = getWeight(root);

    std::vector<int> kids;
    getChildrenOf(root.name, kids);
    for (int idx : kids) {
        total += getWeight(worldItems[idx]);
    }
    return total;
}

// =============================
// Carried weight for a player
// =============================

int getCarriedWeight(Player &p) {
  int total = 0;
  for (int i = 0; i < p.invCount; i++) {
    int wiIndex = p.invIndices[i];
    if (wiIndex < 0 || wiIndex >= (int)worldItems.size()) continue;
    total += getWeight(worldItems[wiIndex]);
  }
  return total;
}

// =============================
// Calculate max weight player can carry based on level
// Base is MAX_WEIGHT (10), +20% per level above 1
// Level 1 = 10, Level 2 = 12, Level 3 = 14.4->15, etc.
// Formula: MAX_WEIGHT * (1.2 ^ (level - 1)), rounded up
// =============================

int getMaxCarryWeight(Player &p) {
  if (p.level < 1) return MAX_WEIGHT;
  
  // Calculate using 1.2^(level-1)
  double multiplier = 1.0;
  for (int i = 1; i < p.level; i++) {
    multiplier *= 1.2;
  }
  
  int maxWeight = (int)ceil((double)MAX_WEIGHT * multiplier);
  return maxWeight;
}

// =============================
// Weapon and armor bonuses
// =============================

int getWeaponBonus(Player &p) {
  int idx = p.wieldedItemIndex;
  if (idx < 0 || idx >= (int)worldItems.size()) return 0;
  return getDamageBonus(worldItems[idx]);
}

int getArmorDefense(Player &p) {
  int defense = 0;
  for (int s = 0; s < SLOT_COUNT; s++) {
    int idx = p.wornItemIndices[s];
    if (idx < 0 || idx >= (int)worldItems.size()) continue;
    defense += getArmorClass(worldItems[idx]);
  }
  return defense;
}

// =============================
// Simple emote table (copied from original, unchanged behavior)
// =============================

Emote emotes[] = {
  {"admire","admires","at","openly", false},
  {"announce","announces","to","grandly", true},
  {"apologize","apologizes","to","half-heartedly", true},
  {"applaud","applauds","at","sarcastically", true},
  {"approach","approaches","toward","slowly", true},
  {"beam","beams","at","brightly", true},
  {"beck","becks","to","softly", true},
  {"beckon","beckons","to","invitingly", true},
  {"blush","blushes","at","embarrassedly", true},
  {"boast","boasts","at","loudly", true},
  {"boop","boops","on","playfully", false},
  {"bounce","bounces","at","cheerfully", true},
  {"bow","bows","to","formally", true},
  {"brag","brags","at","obnoxiously", true},
  {"bump","bumps","into","awkwardly", true},
  {"caress","caresses","to","tenderly", false},
  {"celebrate","celebrates","at","wildly", true},
  {"chant","chants","at","ritually", true},
  {"cheer","cheers","at","loudly", true},
  {"chirp","chirps","at","brightly", true},
  {"chortle","chortles","at","gleefully", true},
  {"circle","circles","around","suspiciously", true},
  {"circle","circles","around","methodically", true},
  {"clap","claps","at","enthusiastically", true},
  {"comfort","comforts","to","warmly", false},
  {"compliment","compliments","to","awkwardly", false},
  {"congratulate","congratulates","to","enthusiastically", false},
  {"console","consoles","to","gently", false},
  {"cough","coughs","at","politely", true},
  {"cry","cries","at","pitifully", true},
  {"curtsy","curtsies","to","gracefully", true},
  {"dance","dances","at","gracefully", true},
  {"declare","declares","to","proudly", true},
  {"encourage","encourages","to","supportively", false},
  {"farewell","bids farewell","to","dramatically", true},
  {"flex","flexes","at","vainly", true},
  {"flinch","flinches","at","nervously", true},
  {"freeze","freezes","at","suddenly", true},
  {"frown","frowns","at","disapprovingly", true},
  {"gaze","gazes","at","dreamily", true},
  {"gesture","gestures","to","vaguely", true},
  {"giggle","giggles","at","childishly", true},
  {"glance","glances","at","briefly", true},
  {"glare","glares","at","angrily", true},
  {"glomp","glomps","onto","enthusiastically", true},
  {"glow","glows","at","warmly", true},
  {"grin","grins","at","wickedly", true},
  {"grimace","grimaces","at","painfully", true},
  {"groan","groans","at","miserably", true},
  {"growl","growls","at","angrily", true},
  {"hail","hails","to","formally", true},
  {"harmonize","harmonizes","with","softly", true},
  {"hover","hovers","near","ominously", true},
  {"hover","hovers","around","curiously", true},
  {"hug","hugs","to","affectionately", false},
  {"hum","hums","at","quietly", true},
  {"inspect","inspects","at","critically", false},
  {"invite","invites","to","politely", false},
  {"jump","jumps","at","excitedly", true},
  {"kick","kicks","at","lightly", false},
  {"kiss","kisses","to","passionately", false},
  {"kneel","kneels","to","reverently", true},
  {"laugh","laughs","at","sarcastically", true},
  {"lean","leans","toward","closely", true},
  {"lick","licks","at","creepily", false},
  {"mourn","mourns","at","solemnly", true},
  {"mock","mocks","at","cruelly", false},
  {"moan","moans","at","dramatically", true},
  {"muse","muses","at","philosophically", true},
  {"nudge","nudges","at","insistently", false},
  {"nuzzle","nuzzles","to","affectionately", false},
  {"pat","pats","to","condescendingly", false},
  {"peer","peers","at","suspiciously", true},
  {"peer","peers","at","closely", true},
  {"pester","pesters","at","relentlessly", false},
  {"point","points","at","accusingly", true},
  {"poke","pokes","at","annoyingly", false},
  {"ponder","ponders","at","thoughtfully", true},
  {"pose","poses","at","heroically", true},
  {"praise","praises","to","warmly", false},
  {"pray","prays","to","solemnly", true},
  {"prod","prods","at","impatiently", false},
  {"purr","purrs","at","contentedly", true},
  {"recite","recites","to","formally", true},
  {"regard","regards","at","coolly", true},
  {"retreat","retreats","from","quickly", true},
  {"roar","roars","at","ferociously", true},
  {"salivate","salivates","at","hungrily", true},
  {"salute","salutes","to","smartly", true},
  {"salute","salutes","to","respectfully", true},
  {"scoff","scoffs","at","dismissively", true},
  {"scoot","scoots","toward","cautiously", true},
  {"scowl","scowls","at","darkly", true},
  {"shake","shakes","at","vigorously", false},
  {"shiver","shivers","at","nervously", true},
  {"shout","shouts","at","boldly", true},
  {"shrug","shrugs","at","indifferently", true},
  {"shudder","shudders","at","horribly", true},
  {"sigh","sighs","at","dramatically", true},
  {"sing","sings","at","tunefully", true},
  {"skip","skips","at","happily", true},
  {"smile","smiles","at","warmly", true},
  {"smirk","smirks","at","smugly", true},
  {"snarl","snarls","at","menacingly", true},
  {"snicker","snickers","at","mockingly", true},
  {"sniff","sniffs","at","curiously", true},
  {"snort","snorts","at","derisively", true},
  {"snuggle","snuggles","to","comfortably", false},
  {"sneeze","sneezes","at","explosively", true},
  {"scoff","scoffs","at","dismissively", true},
  {"study","studies","at","intently", true},
  {"stare","stares","at","intensely", true},
  {"stretch","stretches","at","awkwardly", true},
  {"sway","sways","at","dreamily", true},
  {"tap","taps","on","gently", false},
  {"taunt","taunts","at","mercilessly", false},
  {"tease","teases","at","sarcastically", false},
  {"thank","thanks","to","gratefully", false},
  {"think","thinks","at","deeply", true},
  {"threaten","threatens","to","ominously", false},
  {"tickle","tickles","to","playfully", false},
  {"tilt","tilts","at","quizzically", true},
  {"tremble","trembles","at","fearfully", true},
  {"wave","waves","at","cheerfully", true},
  {"welcome","welcomes","to","warmly", false},
  {"whimper","whimpers","at","pathetically", true},
  {"whine","whines","at","pathetically", true},
  {"whirl","whirls","around","energetically", true},
  {"whistle","whistles","at","tunelessly", true},
  {"wink","winks","at","suggestively", true},
  {"yawn","yawns","at","lazily", true}
};


const int emoteCount = sizeof(emotes) / sizeof(emotes[0]);


// =============================
// HELPERS**********************
// =============================

// Helper function to get field description for player file lines
String getPlayerFieldDescription(int lineNum) {
    // Lines in user_[player].txt file:
    // 1=password, 2=raceId, 3=level, 4=xp, 5=hp, 6=maxHp, 7=coins
    // 8=IsWizard, 9=debugDest, 10=showStats
    // 11=EnterMsg, 12=ExitMsg, 13=wimpMode
    // 14=roomX, 15=roomY, 16=roomZ
    // 17=invCount, 18-49=inventory items (32 max), 50=wielded, 51-61=worn items (11 slots)
    // 62-71=quest completion flags (10), 72-171=quest step flags (100)
    
    switch(lineNum) {
        case 1:  return "Login Password";
        case 2:  return "Race ID";
        case 3:  return "Level";
        case 4:  return "Experience Points";
        case 5:  return "Current Health";
        case 6:  return "Max Health";
        case 7:  return "Coins";
        case 8:  return "Is Wizard (0=no, 1=yes)";
        case 9:  return "Debug Destination (0=none, 1=serial, 2=telnet)";
        case 10: return "Show Stats (0=no, 1=yes)";
        case 11: return "Enter Message";
        case 12: return "Exit Message";
        case 13: return "Wimp Mode (0=no, 1=yes)";
        case 14: return "Room X";
        case 15: return "Room Y";
        case 16: return "Room Z";
        case 17: return "Inventory Count";
        default:
            if (lineNum >= 18 && lineNum <= 49) {
                int invIdx = lineNum - 18;
                return "Inventory Item #" + String(invIdx);
            }
            if (lineNum == 50) return "Wielded Item";
            if (lineNum >= 51 && lineNum <= 61) {
                int slotIdx = lineNum - 51;
                const char* slotNames[] = {"Head", "Chest", "Chest2", "Legs", "Feet", "Hands", "Arms", "Back", "Waist", "Neck", "Finger"};
                if (slotIdx < 11) return "Worn Item: " + String(slotNames[slotIdx]);
                return "Worn Item #" + String(slotIdx);
            }
            if (lineNum >= 62 && lineNum <= 71) {
                int questIdx = lineNum - 62;
                return "Quest " + String(questIdx) + " Completed";
            }
            if (lineNum >= 72 && lineNum <= 171) {
                int questStep = lineNum - 72;
                int questId = questStep / 10;
                int stepId = questStep % 10;
                return "Quest " + String(questId) + " Step " + String(stepId);
            }
            return "";
    }
}

// Helper function to mask passwords in player file output
// When printing user_[player].txt files, the first line is the password
// This function replaces it with asterisks
String maskPasswordLine(const String &filename, int lineNum, const String &line) {
    // Check if this is a player file (user_*.txt)
    if ((filename.startsWith("user_") || filename.startsWith("/user_")) && 
        filename.endsWith(".txt") && 
        lineNum == 1) {  // First line is the password
        // Return masked password (asterisks for each character)
        String masked = "";
        for (int i = 0; i < line.length(); i++) {
            masked += "*";
        }
        return masked;
    }
    return line;
}

bool isPortalCommand(const Room &r, const String &cmd) {
    if (!r.hasPortal) return false;
    String pcmd = String(r.portalCommand);
    pcmd.trim();
    pcmd.toLowerCase();
    String c = cmd;
    c.trim();
    c.toLowerCase();
    return (pcmd == c);
}

void printFile(Player &p, const char *filename) {
    String path = filename;
    if (!path.startsWith("/")) path = "/" + path;

    // File exists?
    if (!LittleFS.exists(path)) {
        debugPrint(p, "");
        debugPrint(p, "[Warning] " + path + " not found on LittleFS.");
        return;
    }

    // Header
    debugPrint(p, "");
    debugPrint(p, "[System] " + path + " found! Printing contents:");
    debugPrint(p, "----------------------------------------------");

    File f = LittleFS.open(path, "r");
    if (!f) {
        debugPrint(p, "[Error] Failed to open " + path + " for reading.");
        return;
    }

    // Stream file line-by-line to correct destination
    int lineNum = 0;
    bool isPlayerFile = (path.startsWith("user_") || path.startsWith("/user_")) && path.endsWith(".txt");
    
    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();  // Remove carriage returns and extra whitespace
        lineNum++;

        // Mask password in player files (first line is the password)
        line = maskPasswordLine(path, lineNum, line);

        // For player files, add field descriptions
        if (isPlayerFile) {
            String desc = getPlayerFieldDescription(lineNum);
            if (desc.length() > 0) {
                // Format: value     -> Description
                // Pad the value to 20 chars, then add arrow and description
                while (line.length() < 20) {
                    line += " ";
                }
                line = line + "-> " + desc;
            }
        }

        if (p.debugDest == DEBUG_TO_SERIAL) {
            Serial.println(line);
        } else {
            p.client.println(line);
        }
    }

    f.close();

    // Footer
    debugPrint(p, "----------------------------------------------");
    debugPrint(p, "[System] End of " + path);
}



void printItemDefinitions() {
  Serial.println("\n[Memory] Printing Item Definitions (VXD Data):");
  Serial.println("----------------------------------------------");

  for (const auto &pair : itemDefs) {
    const std::string &name = pair.first;
    const ItemDefinition &def = pair.second;

    Serial.print("Item: ");
    Serial.println(name.c_str());

    Serial.print("  Type: ");
    Serial.println(def.type.c_str());

    for (const auto &attr : def.attributes) {
      const std::string &key = attr.first;
      const std::string &val = attr.second;

      Serial.print("  - ");
      Serial.print(key.c_str());
      Serial.print(" = ");
      Serial.println(val.c_str());
    }

    Serial.println("---");
  }

  Serial.print("[System] Total Definitions: ");
  Serial.println((int)itemDefs.size());
}



void printWorldItems() {
  Serial.println("\n[Memory] Printing World Placements (VXI Data):");
  Serial.println("----------------------------------------------");

  for (int i = 0; i < (int)worldItems.size(); i++) {
    const auto &item = worldItems[i];
    Serial.print("#"); Serial.print(i); Serial.print(" Loc: (");
    Serial.print(item.x); Serial.print(",");
    Serial.print(item.y); Serial.print(",");
    Serial.print(item.z); Serial.print(") ");

    Serial.print("Item: "); Serial.print(item.name);

    if (item.parentName.length() > 0) {
      Serial.print(" [Inside: ");
      Serial.print(item.parentName);
      Serial.print("]");
    } else {
      Serial.print(" [On Floor]");
    }
    Serial.println();
  }
  Serial.print("[System] Total Items in World: ");
  Serial.println(worldItems.size());
}

// =============================
// Races, titles, and experience chart
// =============================

const char* raceNames[5] = {"Human","Elf","Dwarf","Orc","Halfling"};

const long expChart[21] = {
  0, 500, 1500, 3000, 6000, 12000, 30000, 60000, 100000, 160000,
  250000, 350000, 500000, 650000, 800000, 900000, 950000, 970000,
  985000, 995000, 1000000
};

static const char* titles[5][21] = {
  {"Utter Novice","Apprentice Squire","Militia Recruit","Town Guard","Knight-in-Training",
   "Banner Knight","Veteran Soldier","Knight","Champion","Paladin",
   "Crusader","Lord Knight","Baron","Count","Duke",
   "Prince","King","High King","Emperor","Champion of Men"},
  {"Utter Novice","Grove Initiate","Leaf Whisperer","Forest Warden","Ranger Adept",
   "Bow Adept","Spell Adept","Warden","Grove Protector","High Ranger",
   "Spell Warden","Archmage Adept","Archmage","High Archmage","Spell Lord",
   "Spell Master","Arch Druid","High Druid","Arch Sage","High Lord of Elves"},
  {"Utter Novice","Tunnel Scout","Stone Hauler","Hammer Adept","Forge Keeper",
   "Shield Brother","Deep Miner","Tunnel Defender","Forge Master","Stone Lord",
   "Forge Elder","Deep Elder","Stone Elder","Forge Tyrant","Stone Lord",
   "Forge Master","Stone King","Forge King","Stone Emperor","High Lord of Dwarves"},
  {"Utter Novice","Blooded Youth","Axe-Bearer","Warband Raider","Brutal Warrior",
   "Skull Crusher","Berserker","War Chief","Battle Lord","Warlord",
   "Blood Champion","Warbringer","War Tyrant","Doom Lord","War Destroyer",
   "Blood Lord","War King","Doom King","War Emperor","High Lord of Orcs"},
  {"Utter Novice","Burrow Runner","Hearth Helper","Trickster","Nimble Scout",
   "Cunning Wanderer","Quickblade","Shadowfoot","Trickster Sage","Master Scout",
   "Hearth Sage","Wanderer Elder","Trickster Elder","Nimble Elder","Hearth Lord",
   "Wanderer Lord","Trickster King","Hearth King","Wanderer Emperor","High Lord of Halflings"}
};

// =============================
// Name helpers and titles
// =============================

String getPlayerTitle(int raceId, int level) {
    // Safety clamps
    if (raceId < 0 || raceId > 4) raceId = 0;   // default Human
    if (level < 1) level = 1;
    if (level > 20) level = 20;

    return String(titles[raceId][level - 1]);
}


String normalizeName(const char *name) {
  String s = String(name);
  s.trim();
  s.toLowerCase();
  return s;
}

String capFirst(const char *name) {
  String s = String(name);
  s.trim();
  s.toLowerCase();
  if (s.length() > 0) s.setCharAt(0, toupper(s.charAt(0)));
  return s;
}

String getTitle(Player &p) {
  int lvl = p.level;
  if (lvl < 1) lvl = 1;
  if (lvl > 20) lvl = 20;
  return titles[p.raceId][lvl - 1];
}

// =============================
// Movement and room look
// =============================

// Announce entry to other players in the same voxel
void announceEntry(Player &p, const char *name) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (!players[i].active) continue;
    if (&players[i] == &p) continue;
    if (players[i].roomX == p.roomX &&
        players[i].roomY == p.roomY &&
        players[i].roomZ == p.roomZ) {
      players[i].client.print(name);
      players[i].client.println(" enters the area.");
    }
  }
}


bool playerHasItem(const Player &p, const String &itemId) {
    if (itemId.length() == 0)
        return false;

    for (int i = 0; i < p.invCount; i++) {
        int idx = p.invIndices[i];

        // Validate index
        if (idx < 0 || idx >= (int)worldItems.size())
            continue;

        const WorldItem &wi = worldItems[idx];

        // Compare by item ID (exact match)
        if (wi.name == itemId)
            return true;
    }

    return false;
}


void cmdInventory(Player &p, const String &input) {
    (void)input;

    p.client.println("You are carrying:");

    if (p.invCount == 0) {
        p.client.println("  Nothing.");
        return;
    }

    int totalArmorBonus = 0;
    int totalDamageBonus = 0;

    // Collect inventory items with their display info for sorting
    struct InventoryItem {
        int idx;
        String displayName;
        String line;
        bool isWielded;
        bool isWorn;
    };
    
    std::vector<InventoryItem> items;

    // ----------------------------------------------------
    // COLLECT INVENTORY ITEMS (UNSORTED), SKIP GOLD COINS
    // ----------------------------------------------------
    for (int i = 0; i < p.invCount; i++) {
        int idx = p.invIndices[i];
        if (idx < 0 || idx >= (int)worldItems.size()) continue;

        WorldItem &wi = worldItems[idx];
        
        // NEVER show gold coins in inventory display - they should only be in p.coins
        if (wi.name == "gold_coin") continue;
        
        String name = getItemDisplayName(wi);

        bool isWielded = (idx == p.wieldedItemIndex);
        bool isWorn = false;

        for (int s = 0; s < SLOT_COUNT; s++) {
            if (p.wornItemIndices[s] == idx) {
                isWorn = true;
                break;
            }
        }

        String line = "  " + name;
        if (isWielded) line += " (wielded)";
        if (isWorn)    line += " (worn)";

       // WIZARD: ITEM STAT BREAKDOWN
        if (p.IsWizard && p.showStats) {

            // SAFE lookup using std::string key
            std::string key = wi.name.c_str();
            auto it = itemDefs.find(key);

            if (it != itemDefs.end()) {
                auto &def = it->second;

                // Armor stat
                auto ait = def.attributes.find("armor");
                if (ait != def.attributes.end()) {
                    int armorVal = String(ait->second.c_str()).toInt();
                    if (armorVal > 0) {
                        line += "  (Armor=" + String(armorVal) + ")";
                        if (isWorn) totalArmorBonus += armorVal;
                    }
                }

                // Damage stat
                auto dit = def.attributes.find("damage");
                if (dit != def.attributes.end()) {
                    int dmgVal = String(dit->second.c_str()).toInt();
                    if (dmgVal > 0) {
                        line += "  (Damage=" + String(dmgVal) + ")";
                        if (isWielded) totalDamageBonus += dmgVal;
                    }
                }
            }
        }

        items.push_back({idx, name, line, isWielded, isWorn});
    }

    // Sort items alphabetically by display name
    std::sort(items.begin(), items.end(), [](const InventoryItem &a, const InventoryItem &b) {
        return a.displayName < b.displayName;
    });

    // Display sorted items, but only show wielded/worn status on FIRST occurrence of each duplicate item
    std::set<String> wielded_shown;
    std::set<String> worn_shown;
    
    for (auto &item : items) {
        // For duplicates, only show (wielded) once per item name
        if (item.isWielded && wielded_shown.count(item.displayName) > 0) {
            item.line = "  " + item.displayName;
            if (item.isWorn) item.line += " (worn)";
        } else if (item.isWielded) {
            wielded_shown.insert(item.displayName);
        }
        
        // For duplicates, only show (worn) once per item name
        if (item.isWorn && worn_shown.count(item.displayName) > 0) {
            item.line = "  " + item.displayName;
            if (item.isWielded && wielded_shown.count(item.displayName) <= 1) {
                item.line += " (wielded)";
            }
        } else if (item.isWorn) {
            worn_shown.insert(item.displayName);
        }
        
        // Add wizard stats if enabled
        if (item.isWorn || item.isWielded) {
            // Reapply wizard stats if needed
            if (p.IsWizard && p.showStats) {
                int idx = item.idx;
                if (idx >= 0 && idx < (int)worldItems.size()) {
                    WorldItem &wi = worldItems[idx];
                    std::string key = wi.name.c_str();
                    auto it = itemDefs.find(key);
                    
                    if (it != itemDefs.end()) {
                        auto &def = it->second;
                        
                        auto ait = def.attributes.find("armor");
                        if (ait != def.attributes.end()) {
                            int armorVal = String(ait->second.c_str()).toInt();
                            if (armorVal > 0 && item.line.indexOf("Armor=") < 0) {
                                item.line += "  (Armor=" + String(armorVal) + ")";
                            }
                        }
                        
                        auto dit = def.attributes.find("damage");
                        if (dit != def.attributes.end()) {
                            int dmgVal = String(dit->second.c_str()).toInt();
                            if (dmgVal > 0 && item.line.indexOf("Damage=") < 0) {
                                item.line += "  (Damage=" + String(dmgVal) + ")";
                            }
                        }
                    }
                }
            }
        }
        
        p.client.println(item.line);
    }

    // ----------------------------------------------------
    // WIZARD: BASE STATS AFTER INVENTORY LIST
    // ----------------------------------------------------
    if (p.IsWizard && p.showStats) {
        p.client.println("");
        p.client.println("Base Stats:");
        p.client.println("  Base Armor:  " + String(p.baseDefense));
        p.client.println("  Base Damage: " + String(p.attack));
        p.client.println("");

        // ----------------------------------------------------
        // WIZARD: TOTALS
        // ----------------------------------------------------
        p.client.println("Totals:");
        p.client.println("  Total Armor Bonus:  " +
            String(p.baseDefense) + " + " +
            String(totalArmorBonus) + " = " +
            String(p.baseDefense + totalArmorBonus));

        p.client.println("  Total Damage Bonus: " +
            String(p.attack) + " + " +
            String(totalDamageBonus) + " = " +
            String(p.attack + totalDamageBonus));
    }
}



int createWorldItem(const String &itemName, const String &parentName) {
    WorldItem wi;

    wi.name = itemName;
    wi.parentName = parentName;
    wi.ownerName = "";

    int px = 0, py = 0, pz = 0;

    for (auto &w : worldItems) {
        if (w.name == parentName) {
            px = w.x;
            py = w.y;
            pz = w.z;
            break;
        }
    }

    wi.x = px;
    wi.y = py;
    wi.z = pz;

    worldItems.push_back(wi);
    return worldItems.size() - 1;
}


void cmdPortal(Player &p, int index) {
    Room &r = p.currentRoom;

    if (!r.hasPortal) {
        p.client.println("Nothing happens.");
        return;
    }

    // Announce departure to origin room
    announceToRoom(
        p.roomX, p.roomY, p.roomZ,
        String(capFirst(p.name)) + " disappears into thin air!",
        index
    );

    // Show flavor text to traveler
    String txt = String(r.portalText);
    txt.trim();
    if (txt.length() > 0)
        p.client.println(txt);
    else
        p.client.println("You are transported through space and time!");

    // Move player
    int destX = r.px;
    int destY = r.py;
    int destZ = r.pz;

    if (!loadRoomForPlayer(p, destX, destY, destZ)) {
        p.client.println("The portal fizzles and fails.");
        return;
    }

    // Announce arrival to destination room
    announceToRoom(
        p.roomX, p.roomY, p.roomZ,
        String(capFirst(p.name)) + " materializes out of thin air!",
        index
    );

    // Show room to traveler
    cmdLook(p);
}

void restockAllShops() {
    for (auto &sign : worldItems) {
        // Must be a sign
        String t = sign.getAttr("type", itemDefs);
        t.toLowerCase();
        if (t != "sign") continue;

        // Must have children (otherwise it's just a decorative sign)
        bool hasChildren = false;
        for (auto &wi : worldItems) {
            if (wi.parentName == sign.name) {
                hasChildren = true;
                break;
            }
        }
        if (!hasChildren) continue;

        // Collect item types originally under this sign
        String itemTypes[32];
        int typeCount = 0;

        for (auto &wi : worldItems) {
            if (wi.parentName == sign.name) {
                bool exists = false;
                for (int i = 0; i < typeCount; i++) {
                    if (itemTypes[i] == wi.name) {
                        exists = true;
                        break;
                    }
                }
                if (!exists && typeCount < 32)
                    itemTypes[typeCount++] = wi.name;
            }
        }

        // Restock each type to max 3
        for (int i = 0; i < typeCount; i++) {
            String itemName = itemTypes[i];

            int count = 0;
            for (auto &wi : worldItems) {
                if (wi.parentName == sign.name && wi.name == itemName)
                    count++;
            }

            while (count < 3) {
                createWorldItem(itemName, sign.name);
                count++;
            }
        }
    }
}

void movePlayer(Player &p, int index, const char *dir) {

    int d = dirFromString(dir);
    if (d < 0) {
        p.client.println("Unknown direction.");
        return;
    }

    Room &cur = p.currentRoom;
    bool canGo = false;

    switch (d) {
        case DIR_NORTH: canGo = cur.exit_n;  break;
        case DIR_SOUTH: canGo = cur.exit_s;  break;
        case DIR_EAST:  canGo = cur.exit_e;  break;
        case DIR_WEST:  canGo = cur.exit_w;  break;
        case DIR_UP:    canGo = cur.exit_u;  break;
        case DIR_DOWN:  canGo = cur.exit_d;  break;
        case DIR_NE:    canGo = cur.exit_ne; break;
        case DIR_NW:    canGo = cur.exit_nw; break;
        case DIR_SE:    canGo = cur.exit_se; break;
        case DIR_SW:    canGo = cur.exit_sw; break;
    }

    if (!canGo) {
        p.client.println("The path seems blocked.");
        return;
    }

    int dx, dy, dz;
    voxelDelta(d, dx, dy, dz);

    int oldX = cur.x;
    int oldY = cur.y;
    int oldZ = cur.z;

    int tx = oldX + dx;
    int ty = oldY + dy;
    int tz = oldZ + dz;

    // If player leaves the Game Parlor, end any active chess game
    if (oldX == 247 && oldY == 248 && oldZ == 50 && index >= 0 && index < MAX_PLAYERS) {
        if (chessSessions[index].gameActive) {
            chessSessions[index].gameActive = false;
            p.client.println("Your chess game has been abandoned due to leaving the Game Parlor.");
        }
    }

    // -----------------------------
    // DEPARTURE MESSAGE
    // -----------------------------
    if (!p.IsInvisible) {

        String departMsg;

        if (p.IsWizard && p.ExitMsg.length() > 0) {
            // Wizard custom exit message
            departMsg = String(capFirst(p.name)) + " " + p.ExitMsg;
        } else {
            // Normal player exit message
            departMsg = capFirst(p.name) + " leaves " + String(dir) + ".";
        }

        announceToRoom(
            oldX, oldY, oldZ,
            departMsg,
            index
        );
    }

    // Load new room — THIS updates p.currentRoom internally
    if (!loadRoomForPlayer(p, tx, ty, tz)) {
        p.client.println("The path seems blocked.");
        return;
    }

    // -----------------------------
    // ARRIVAL MESSAGE
    // -----------------------------
    if (!p.IsInvisible) {

        String arriveMsg;

        if (p.IsWizard && p.EnterMsg.length() > 0) {
            // Wizard custom arrival message
            arriveMsg = String(capFirst(p.name)) + " " + p.EnterMsg;
        } else {
            // Normal player arrival message
            arriveMsg = capFirst(p.name) + " arrives from the " + oppositeDir(dir) + ".";
        }

        announceToRoom(
            tx, ty, tz,
            arriveMsg,
            index
        );
    }

    // QUEST HOOK — reach quests fire here
    onQuestEvent(p, "reach", "", "", "", p.roomX, p.roomY, p.roomZ);

    // Send voxel coords to mapper
    if (p.sendVoxel) {
        p.client.print("(");
        p.client.print(p.roomX);
        p.client.print(",");
        p.client.print(p.roomY);
        p.client.print(",");
        p.client.print(p.roomZ);
        p.client.println(")");
    }

    cmdLook(p);

    // Check for mail when entering post office
    if (getPostOfficeForRoom(p) != nullptr) {
        checkAndSpawnMailLetters(p);
    }

    checkNPCAggro(p);
}

// =============================================================
// LOOK / EXAMINE / SEARCH (parent/child aware)
// =============================================================

// Helper: print a single item name
String getDisplayName(WorldItem &wi) {
    String d = wi.getAttr("name", itemDefs);
    if (d.length() == 0) d = wi.name;
    return d;
}

// Helper: show NPCs in a room
void showRoomNPCs(Player &p) {
    bool anyNPCs = false;

    for (int i = 0; i < npcInstances.size(); i++) {
        NpcInstance &n = npcInstances[i];

        // Skip dead NPCs
        if (!n.alive) continue;
        if (n.hp <= 0) continue;

        // Must be in same room
        if (n.x != p.roomX || n.y != p.roomY || n.z != p.roomZ) continue;

        // Look up definition for display name (String -> std::string)
        std::string key = std::string(n.npcId.c_str());
        auto it = npcDefs.find(key);
        if (it == npcDefs.end()) continue;

        NpcDefinition &def = it->second;

        anyNPCs = true;
        String npcName = def.attributes.at("name").c_str();
        p.client.println(addArticle(npcName) + " is here.");
    }

    if (anyNPCs) {
        p.client.println("");
    }
}

// Helper function to add article (A/An) to a string
String addArticle(const String &text) {
    if (text.length() == 0) return text;
    
    // Gold coins don't get an article
    if (text.indexOf("gold coin") != -1) {
        return text;
    }
    
    char firstChar = text[0];
    firstChar = tolower(firstChar);
    
    if (firstChar == 'a' || firstChar == 'e' || firstChar == 'i' || 
        firstChar == 'o' || firstChar == 'u') {
        return "An " + text;
    } else {
        return "A " + text;
    }
}

// =============================================================
// LOOK — show room, players, NPCs, and TOP‑LEVEL floor items only
// =============================================================

void cmdLook(Player &p) {
    // Draw map tracker if enabled
    if (p.mapTrackerEnabled) {
        drawPlayerMap(p);
        p.client.println("");  // blank line between map and room description
    }
    
    // Use the player's currentRoom, not a global rooms[][][]
    Room &r = p.currentRoom;

    // Room name
    p.client.println(String(r.name));

    // Room description (word-wrapped for readability)
    printWrapped(p.client, String(r.description));
    
    // Check if this room has a shop - if so, add sign description
    if (getShopForRoom(p) != nullptr) {
        p.client.println("A sign is here.");
    }
    
    // Check if this room has a post office - if so, add sign description
    if (getPostOfficeForRoom(p) != nullptr) {
        p.client.println("A sign is here.");
    }
    
    // Check if this room has a tavern - if so, add sign description
    if (getTavernForRoom(p) != nullptr) {
        p.client.println("A sign is here.");
    }
    
    // Check if this is the Game Parlor
    if (p.roomX == 247 && p.roomY == 248 && p.roomZ == 50) {
        p.client.println("A sign is here.");
    }
    
    p.client.println("");  // blank line

    // Other players in the room
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player &other = players[i];
        
        // Skip self, inactive, not logged in, or invisible
        if (&other == &p) continue;
        if (!other.active || !other.loggedIn) continue;
        if (other.IsInvisible) continue;
        
        // Must be in this room
        if (other.roomX != p.roomX || other.roomY != p.roomY || other.roomZ != p.roomZ)
            continue;
        
        // Display other player
        p.client.println(capFirst(other.name) + " is here.");
    }

    // NPCs in the room
    showRoomNPCs(p);

    // Items in the room
    bool anyItems = false;
    for (int i = 0; i < (int)worldItems.size(); i++) {
        WorldItem &wi = worldItems[i];

        // Must be in this room and not owned
        if (wi.ownerName.length() != 0) continue;
        if (wi.x != p.roomX || wi.y != p.roomY || wi.z != p.roomZ) continue;

        // Skip invisible items
        if (wi.getAttr("invisible", itemDefs) == "1")
            continue;

        if (!anyItems) anyItems = true;

        p.client.println(addArticle(getItemDisplayName(wi)) + ".");
    }

    p.client.println("");  // blank line

    // Obvious exits
    String exits = r.exitList;
    exits.trim();
    if (exits.length() == 0) exits = "none";

    p.client.println("Obvious exits: " + exits);
}




void cmdLookAt(Player &p, const String &input) {
    String arg = input;
    arg.trim();

    if (arg.length() == 0) {
        p.client.println("Look at what?");
        return;
    }

    String searchLower = arg;
    searchLower.toLowerCase();

    // CHECK FOR PLAYER FIRST
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player &other = players[i];
        
        // Skip self, inactive, not logged in, or invisible
        if (&other == &p) continue;
        if (!other.active || !other.loggedIn) continue;
        if (other.IsInvisible) continue;
        
        // Must be in this room
        if (other.roomX != p.roomX || other.roomY != p.roomY || other.roomZ != p.roomZ)
            continue;
        
        // Check if name matches (with word-based partial matching)
        String otherName = String(other.name);
        otherName.toLowerCase();
        
        if (nameMatches(otherName, searchLower)) {
            // Display player name
            p.client.println(capFirst(other.name));
            
            // Display their level title
            p.client.println(String(titles[other.raceId][other.level - 1]));
            return;
        }
    }

    // CHECK FOR NPCS
    for (int i = 0; i < (int)npcInstances.size(); i++) {
        NpcInstance &npc = npcInstances[i];
        
        // Must be alive
        if (!npc.alive || npc.hp <= 0) continue;
        
        // Must be in this room
        if (npc.x != p.roomX || npc.y != p.roomY || npc.z != p.roomZ)
            continue;
        
        // Look up NPC definition
        std::string key = std::string(npc.npcId.c_str());
        auto it = npcDefs.find(key);
        if (it == npcDefs.end()) continue;
        
        NpcDefinition &def = it->second;
        String npcName = def.attributes.at("name").c_str();
        String npcDesc = def.attributes.count("desc") ? def.attributes.at("desc").c_str() : "";
        
        // Check if name matches (with word-based partial matching)
        String npcNameLower = npcName;
        npcNameLower.toLowerCase();
        
        if (nameMatches(npcNameLower, searchLower)) {
            // Display NPC name
            p.client.println(npcName);
            
            // Display NPC description if available (word-wrapped)
            if (npcDesc.length() > 0) {
                printWrapped(p.client, npcDesc);
            }
            return;
        }
    }

    // LOOK AT uses normal resolution (not search)
    int idx = resolveItem(p, arg);
    if (idx == -1) {
        p.client.println("You don't see that here.");
        return;
    }

    WorldItem *wiPtr = getResolvedWorldItem(p, idx);
    if (!wiPtr) {
        p.client.println("You don't see that here.");
        return;
    }

    WorldItem &wi = *wiPtr;

    // Description
    showItemDescription(p, wi);

    // Container contents (visible only)
    String c1 = wi.getAttr("container", itemDefs);
    String c2 = wi.getAttr("can_contain", itemDefs);
    String itemType = wi.getAttr("type", itemDefs);
    
    if (c1 == "1" || c2 == "1") {
        bool found = false;

        for (int childIndex : wi.children) {
            if (childIndex < 0 || childIndex >= (int)worldItems.size()) continue;

            WorldItem &child = worldItems[childIndex];

            if (child.getAttr("invisible", itemDefs) == "1")
                continue;

            if (!found) {
                p.client.println("It contains:");
                found = true;
            }

            p.client.println("  " + getItemDisplayName(child));
        }

        // Only show "It is empty." for actual containers, not for weapons/other items
        if (!found && itemType == "container")
            p.client.println("It is empty.");
    }
}


void cmdLookIn(Player &p, const String &input) {
    String arg = input;
    arg.trim();

    if (arg.length() == 0) {
        p.client.println("Look in what?");
        return;
    }

    int idx = resolveItem(p, arg);
    if (idx == -1) {
        p.client.println("You don't see that here.");
        return;
    }

    WorldItem *wiPtr = getResolvedWorldItem(p, idx);
    if (!wiPtr) {
        p.client.println("You don't see that here.");
        return;
    }

    WorldItem &container = *wiPtr;

    // ACCEPT BOTH container=1 AND can_contain=1
    String c1 = container.getAttr("container", itemDefs);
    String c2 = container.getAttr("can_contain", itemDefs);

    if (!(c1 == "1" || c2 == "1")) {
        p.client.println("You can't look inside that.");
        return;
    }

    bool found = false;

    for (int childIndex : container.children) {
        if (childIndex < 0 || childIndex >= (int)worldItems.size()) continue;

        WorldItem &child = worldItems[childIndex];

        if (child.getAttr("invisible", itemDefs) == "1")
            continue;

        if (!found) {
            p.client.println("It contains:");
            found = true;
        }

        p.client.println("  " + getItemDisplayName(child));
    }

    if (!found)
        p.client.println("It is empty.");
}




void cmdLookItem(Player &p, const String &input) {
    String itemIdStr = input;
    itemIdStr.trim();
    if (itemIdStr.length() == 0) {
        p.client.println("Look up which item?");
        return;
    }

    std::string itemId = itemIdStr.c_str();
    auto it = itemDefs.find(itemId);
    if (it == itemDefs.end()) {
        p.client.println("No such item definition.");
        return;
    }

    ItemDefinition &def = it->second;

    p.client.println("Item ID: " + String(itemId.c_str()));
    p.client.println("  Type: " + String(def.type.c_str()));

    for (auto dit = def.attributes.begin(); dit != def.attributes.end(); ++dit) {
        String key = dit->first.c_str();
        String val = dit->second.c_str();
        p.client.println("    " + key + " = " + val);
    }
}




// =============================================================
// SEARCH — reveal children of an item
// =============================================================


void cmdExamineSearch(Player &p, const String &input, bool isSearch) {
    String arg = input;
    arg.trim();

    if (arg.length() == 0) {
        p.client.println(isSearch ? "Search what?" : "Examine what?");
        return;
    }

    int idx = resolveItem(p, arg);

    if (idx == -1) {
        p.client.println("You don't see that here.");
        return;
    }

    WorldItem *wiPtr = getResolvedWorldItem(p, idx);
    if (!wiPtr) {
        p.client.println("You don't see that here.");
        return;
    }

    WorldItem &wi = *wiPtr;

    // Show description (for search, always show regular desc, not empty_desc)
    if (isSearch)
        showItemDescriptionNormal(p, wi);
    else
        showItemDescription(p, wi);

    // Reveal hidden children
    bool revealedSomething = false;
    for (int childIndex : wi.children) {
        if (childIndex < 0 || childIndex >= (int)worldItems.size()) continue;

        WorldItem &child = worldItems[childIndex];

        if (child.getAttr("invisible", itemDefs) == "1") {
            child.attributes["invisible"] = "0";
            revealedSomething = true;
        }
    }

    if (revealedSomething) {
        p.client.println("You discover something hidden inside " +
                         getItemDisplayName(wi) + ".");
    }

    // Show visible contents
    String c1 = wi.getAttr("container", itemDefs);
    String c2 = wi.getAttr("can_contain", itemDefs);
    String itemType = wi.getAttr("type", itemDefs);
    
    if (c1 == "1" || c2 == "1") {
        bool found = false;

        for (int childIndex : wi.children) {
            if (childIndex < 0 || childIndex >= (int)worldItems.size()) continue;

            WorldItem &child = worldItems[childIndex];

            if (child.getAttr("invisible", itemDefs) == "1")
                continue;

            if (!found) {
                p.client.println("It contains:");
                found = true;
            }

            p.client.println("  " + getItemDisplayName(child));
        }

        // Only show "It is empty." for actual containers, not for weapons/other items
        if (!found && itemType == "container")
            p.client.println("It is empty.");
    }
}



// =============================================================
// DEBUG: Print all world items with full parent/child info
// =============================================================

void debugPrintNoNL(Player &p, const String &msg) {
    if (p.debugDest == DEBUG_TO_SERIAL) {
        Serial.print(msg);
    } else {
        p.client.print(msg);
    }
}


void debugDumpSinglePlayer(Player &p, const String &name) {
    String path1 = "user_" + name + ".txt";
    String path2 = "/user_" + name + ".txt";

    String path;

    if (LittleFS.exists(path1)) path = path1;
    else if (LittleFS.exists(path2)) path = path2;
    else {
        debugPrint(p, "Player file not found for: " + name);
        return;
    }

    File f = LittleFS.open(path, "r");
    if (!f) {
        debugPrint(p, "Failed to open player file: " + path);
        return;
    }

    debugPrint(p, "========================================");
    debugPrint(p, "        PLAYER DEBUG: " + name);
    debugPrint(p, "========================================");

    auto read = [&](const char *label) {
        String v = f.readStringUntil('\n');
        v.trim();
        debugPrint(p, String(label) + v);
        return v;
    };

    // -----------------------------
    // BASIC FIELDS
    // -----------------------------
    debugPrint(p, "--- BASIC ---");
    read("password: ");
    read("raceId: ");
    read("level: ");
    read("xp: ");
    read("hp: ");
    read("maxHp: ");
    read("coins: ");

    // -----------------------------
    // WIZARD FIELDS
    // -----------------------------
    debugPrint(p, "");
    debugPrint(p, "--- WIZARD ---");
    read("isWizard: ");
    read("debugDest: ");

    // -----------------------------
    // CUSTOM MESSAGES
    // -----------------------------
    debugPrint(p, "");
    debugPrint(p, "--- CUSTOM MESSAGES ---");
    read("EnterMsg: ");
    read("ExitMsg: ");

    // -----------------------------
    // POSITION
    // -----------------------------
    debugPrint(p, "");
    debugPrint(p, "--- POSITION ---");
    read("wimpMode: ");
    read("roomX: ");
    read("roomY: ");
    read("roomZ: ");

    // -----------------------------
    // INVENTORY
    // -----------------------------
    debugPrint(p, "");
    debugPrint(p, "--- INVENTORY ---");

    String invCountStr = read("invCount: ");
    int invCount = invCountStr.toInt();

    debugPrintNoNL(p, "items: ");
    for (int i = 0; i < invCount; i++) {
        String line = f.readStringUntil('\n');
        line.trim();
        debugPrintNoNL(p, line.length() ? line : "-");
        if (i < invCount - 1) debugPrintNoNL(p, ", ");
    }
    debugPrint(p, "");

    // -----------------------------
    // WIELDED
    // -----------------------------
    debugPrint(p, "");
    debugPrint(p, "--- EQUIPMENT ---");

    String wieldStr = f.readStringUntil('\n');
    wieldStr.trim();
    debugPrint(p, "wielded: " + (wieldStr.length() ? wieldStr : "<none>"));

    // -----------------------------
    // WORN SLOTS
    // -----------------------------
    debugPrintNoNL(p, "worn: ");
    for (int s = 0; s < SLOT_COUNT; s++) {
        String wid = f.readStringUntil('\n');
        wid.trim();
        debugPrintNoNL(p, wid.length() ? wid : "-1");
        if (s < SLOT_COUNT - 1) debugPrintNoNL(p, ", ");
    }
    debugPrint(p, "");

    f.close();

    // -----------------------------
    // QUEST FLAGS
    // -----------------------------
    debugPrint(p, "");
    debugPrint(p, "--- QUEST FLAGS ---");

    for (int q = 0; q < 10; q++) {
        int questId = q + 1;
        bool completed = p.questCompleted[q];

        String line = "Quest " + String(questId) + ": ";
        line += completed ? "COMPLETED" : "in progress";
        debugPrint(p, line);

        debugPrintNoNL(p, "  Steps: ");
        bool any = false;

        for (int s = 0; s < 10; s++) {
            if (p.questStepDone[q][s]) {
                debugPrintNoNL(p, String(s + 1) + " ");
                any = true;
            }
        }

        if (!any)
            debugPrintNoNL(p, "<none>");

        debugPrint(p, "");
    }

    debugPrint(p, "--- END QUEST FLAGS ---");
    debugPrint(p, "");
    debugPrint(p, "========================================");
    debugPrint(p, "        END PLAYER DEBUG");
    debugPrint(p, "========================================");
}

void debugDumpPlayers(Player &p) {
    File root = LittleFS.open("/");
    if (!root || !root.isDirectory()) {
        debugPrint(p, "LittleFS root directory not found.");
        return;
    }

    File file = root.openNextFile();

    while (file) {
        String fname = file.name();

        bool isPlayerFile =
            (fname.startsWith("user_") || fname.startsWith("/user_")) &&
            fname.endsWith(".txt");

        if (isPlayerFile) {
            debugPrint(p, "Player file: " + fname);
            printFile(p, fname.c_str()); // still prints to Serial
            debugPrint(p, "");
        }

        file = root.openNextFile();
    }
}



void debugDumpItems(Player &p) {
    for (int i = 0; i < (int)worldItems.size(); i++) {
        WorldItem &wi = worldItems[i];

        String line = "#" + String(i) +
                      "  ID=" + wi.name +
                      "  (" + getItemDisplayName(wi) + ")" +
                      "  owner=" + (wi.ownerName.length() ? wi.ownerName : "<world>") +
                      "  parent=" + (wi.parentName.length() ? wi.parentName : "<none>") +
                      "  xyz=(" + String(wi.x) + "," + String(wi.y) + "," + String(wi.z) + ")";

        debugPrint(p, line);

        // String -> std::string for itemDefs key
        std::string key = std::string(wi.name.c_str());
        if (itemDefs.count(key)) {
            auto &def = itemDefs[key];

            debugPrint(p, "    attributes:");
            for (auto &kv : def.attributes) {
                debugPrint(p, "      " +
                               String(kv.first.c_str()) +
                               " = " +
                               kv.second.c_str());
            }
        }

        std::vector<int> kids;
        getChildrenOf(wi.name, kids);
        if (!kids.empty()) {
            debugPrint(p, "    children:");
            for (int k : kids) {
                WorldItem &c = worldItems[k];
                debugPrint(p, "      -> " + c.name + " (" + getItemDisplayName(c) + ")");
            }
        }

        debugPrint(p, "");
    }
}



void debugListAllFiles(Player &p) {
    debugPrint(p, "=== LISTING ALL FILES IN ROOT ===");

    File root = LittleFS.open("/");
    File file = root.openNextFile();

    while (file) {
        debugPrint(p, file.name());
        file = root.openNextFile();
    }

    debugPrint(p, "=== END LIST ===");
}


void debugDumpFiles(Player &p) {
    printFile(p, "/rooms.txt");
    printFile(p, "/items.vxd");
    printFile(p, "/items.vxi");
    printFile(p, "/npcs.vxi");
    printFile(p, "/npcs.vxd");
    printFile(p, "/quests.txt");
    printFile(p, "/world_items.vxi");
}



void debugDumpNPCs(Player &p) {
    for (auto &pair : npcDefs) {
        // pair.first is std::string
        String id = String(pair.first.c_str());
        auto &def = pair.second;

        debugPrint(p, "NPC ID: " + id);

        for (auto &kv : def.attributes) {
            debugPrint(p, "    " +
                           String(kv.first.c_str()) +
                           " = " +
                           kv.second.c_str());
        }

        debugPrint(p, "");
    }

    for (int i = 0; i < (int)npcInstances.size(); i++) {
        auto &npc = npcInstances[i];

        String line = "#" + String(i) +
                      "  npcId=" + npc.npcId +
                      "  alive=" + String(npc.alive ? "yes" : "no") +
                      "  hp=" + String(npc.hp) +
                      "  gold=" + String(npc.gold) +
                      "  xyz=(" + String(npc.x) + "," + String(npc.y) + "," + String(npc.z) + ")";

        debugPrint(p, line);
        debugPrint(p, "");
    }
}




void cmdDebug(Player &p, const String &input) {
    String arg = input;
    arg.trim();

    if (arg.length() == 0) {
        p.client.println("Usage: debug items|debug files");
        return;
    }

    // ============================================================
    // DEBUG ITEMS
    // ============================================================
    if (arg == "items") {
        p.client.println("Debug output sent to Arduino Serial Monitor.");
        debugDumpItemsToSerial();
        return;
    }

    // ============================================================
    // DEBUG FILES
    // ============================================================
    if (arg == "files") {
        p.client.println("Debug output sent to Arduino Serial Monitor.");
        debugDumpFilesToSerial();
        return;
    }

    // ============================================================
    // FLASH SPACE (ESP32 LittleFS FIXED VERSION)
    // ============================================================
    if (arg == "flashspace") {
        size_t total = LittleFS.totalBytes();
        size_t used  = LittleFS.usedBytes();
        size_t freeB = total - used;

        size_t totalKB = total / 1024;
        size_t usedKB  = used  / 1024;
        size_t freeKB  = freeB / 1024;

        p.client.println("=== LittleFS Flash Space ===");
        p.client.println("Total : " + String(totalKB) + " KB");
        p.client.println("Used  : " + String(usedKB)  + " KB");
        p.client.println("Free  : " + String(freeKB)  + " KB");
        p.client.println("============================");
        return;
    }

    // ============================================================
    // DELETE FILE
    // ============================================================
    if (arg.startsWith("delete ")) {
        String fname = arg.substring(7);
        fname.trim();

        if (fname.length() == 0) {
            p.client.println("Usage: debug delete <filename>");
            return;
        }

        // Ensure it starts with '/'
        if (!fname.startsWith("/")) {
            fname = "/" + fname;
        }

        if (!LittleFS.exists(fname)) {
            p.client.println("File not found: " + fname);
            return;
        }

        if (LittleFS.remove(fname)) {
            p.client.println("Deleted: " + fname);
        } else {
            p.client.println("FAILED to delete: " + fname);
        }

        return;
    }

    p.client.println("Unknown debug argument.");
}

// BUY/SELL====================

void showTavernSign(Player &p, Tavern &tavern) {
    p.client.println("");
    p.client.println("      >>> " + tavern.tavernName + " <<<");
    p.client.println("");

    // Compute longest drink name
    int longest = 0;
    for (auto &drink : tavern.drinks) {
        if (drink.name.length() > longest)
            longest = drink.name.length();
    }

    // Print aligned lines
    for (auto &drink : tavern.drinks) {
        String line = "  " + drink.name;

        int dots = (longest - drink.name.length()) + 8;
        for (int d = 0; d < dots; d++)
            line += ".";

        line += " " + String(drink.price) + " gold";

        p.client.println(line);
    }
    
    p.client.println("");
    p.client.println("  Drunkenness: " + String(p.drunkenness) + "/6");
}

void showPostOfficeSign(Player &p, PostOffice &po) {
    p.client.println("");
    p.client.println("      Esperthertu Post Office");
    p.client.println("");
    p.client.println("SENDING MAIL:");
    p.client.println("  type 'send [emailaddress] [message]'");
    p.client.println("  Cost: 10 gold per message");
    p.client.println("");
    p.client.println("RECEIVING MAIL:");
    p.client.println("  type 'mail' to check for incoming letters");
    p.client.println("  type 'get letter' to pick up your mail");
    p.client.println("  type 'examine letter' to read your mail");
    p.client.println("");
    p.client.println("We cannot guarantee delivery");
    p.client.println("");
}

void cmdReadSign(Player &p, const String &input) {
    // Check if there's a tavern in this room
    Tavern* tavern = getTavernForRoom(p);
    if (tavern) {
        showTavernSign(p, *tavern);
        return;
    }
    
    // Check if there's a post office in this room
    PostOffice* po = getPostOfficeForRoom(p);
    if (po) {
        showPostOfficeSign(p, *po);
        return;
    }
    
    // Check if this is the Game Parlor
    if (p.roomX == 247 && p.roomY == 248 && p.roomZ == 50) {
        // Find player index to get their game pot
        int potAmount = globalHighLowPot;
        
        p.client.println("===== GAME PARLOR GAMES ===============");
        p.client.println("1. High-Low Card Game - Test your luck!");
        p.client.println("2. Chess - match wits against a local!");
        p.client.println("");
        p.client.println("Type 'play [#]' to play!");
        p.client.println("Type 'rules [#]' for game rules!");
        p.client.println("=======================================");
        return;
    }
    
    // Check if there's a shop in this room
    Shop* shop = getShopForRoom(p);
    if (!shop) {
        p.client.println("There is no sign to read here.");
        return;
    }
    
    shop->displayInventory(p.client);
}

void cmdBuy(Player &p, const String &arg) {
    String itemName = arg;
    itemName.trim();
    if (itemName.length() == 0) {
        p.client.println("Buy what?");
        return;
    }

    // Get the shop for this room
    Shop* shop = getShopForRoom(p);
    if (!shop) {
        p.client.println("You can't buy anything here.");
        return;
    }

    // Find the item in shop inventory
    ShopInventoryItem* shopItem = shop->findItem(itemName);
    if (!shopItem) {
        p.client.println("They don't sell that here.");
        return;
    }

    // Check quantity
    if (shopItem->quantity <= 0) {
        p.client.println("That item is out of stock.");
        return;
    }

    // CHECK 1: Can player afford it?
    if (p.coins < shopItem->price) {
        p.client.println("You can't afford that. You cannot buy " + shopItem->itemName + " because it costs " + String(shopItem->price) + " gold and you only have " + String(p.coins) + ".");
        return;
    }

    // Look up the item definition to get weight and all attributes
    std::string itemIdStr = std::string(shopItem->itemId.c_str());
    int itemWeight = 1;  // default weight
    
    if (itemDefs.count(itemIdStr)) {
        auto &def = itemDefs[itemIdStr];
        String weightStr = def.attributes.count("weight") ? String(def.attributes["weight"].c_str()) : "1";
        itemWeight = weightStr.toInt();
        if (itemWeight <= 0) itemWeight = 1;
    }

    // CHECK 2: Is it too heavy?
    int currentWeight = getCarriedWeight(p);
    int maxWeight = getMaxCarryWeight(p);
    if (currentWeight + itemWeight > maxWeight) {
        p.client.println("You cannot buy " + shopItem->itemName + " because it is too heavy to carry. You would need to drop some items.");
        return;
    }

    // Check inventory space
    if (p.invCount >= 32) {
        p.client.println("You can't carry any more items.");
        return;
    }

    // Deduct coins
    p.coins -= shopItem->price;

    // Create the new item with ALL attributes from itemDefs
    WorldItem newItem;
    newItem.name = shopItem->itemId;  // Use itemId as the item name
    newItem.ownerName = p.name;
    newItem.parentName = "";
    newItem.x = newItem.y = newItem.z = -1;

    // Copy ALL attributes from the item definition
    if (itemDefs.count(itemIdStr)) {
        auto &def = itemDefs[itemIdStr];
        // Copy the type attribute
        newItem.attributes["type"] = def.type;
        // Copy all other attributes
        for (auto &kv : def.attributes) {
            newItem.attributes[kv.first] = kv.second;
        }
    }
    
    // Set value from attribute if available
    String valueStr = "";
    if (newItem.attributes.count("value")) {
        valueStr = String(newItem.attributes["value"].c_str());
    }
    newItem.value = valueStr.length() > 0 ? valueStr.toInt() : shopItem->price;

    // Set description to the shop display name
    newItem.attributes["description"] = shopItem->itemName.c_str();

    worldItems.push_back(newItem);
    int newIdx = worldItems.size() - 1;

    // Add to player inventory
    p.invIndices[p.invCount++] = newIdx;

    // Decrement shop quantity
    shopItem->quantity--;

    p.client.println("You buy " + shopItem->itemName + " for " + String(shopItem->price) + " gold.");
}


void cmdSell(Player &p, const String &arg) {
    String itemName = arg;
    itemName.trim();
    if (itemName.length() == 0) {
        p.client.println("Sell what?");
        return;
    }

    // Find shop in current room
    Shop* shop = getShopForRoom(p);
    if (!shop) {
        p.client.println("You can't sell anything here.");
        return;
    }

    // Find item in inventory
    int idx = findInventoryItemIndexForShop(p, itemName);
    if (idx < 0 || idx >= (int)worldItems.size()) {
        p.client.println("You don't have that.");
        return;
    }

    WorldItem &wi = worldItems[idx];

    // Get item type
    String itemType = wi.getAttr("type", itemDefs);
    itemType.toLowerCase();

    // SHOP FILTERING
    if (shop->shopType == "blacksmith") {
        if (itemType != "weapon" && itemType != "armor") {
            p.client.println("The blacksmith says: I only buy weapons and armor.");
            return;
        }
    } else if (shop->shopType == "misc") {
        if (itemType != "misc") {
            p.client.println("The shopkeeper says: I only buy miscellaneous items.");
            return;
        }
    }

    // Get item value
    int value = wi.getAttr("value", itemDefs).toInt();
    if (value <= 0) {
        p.client.println("The shopkeeper says: I can't buy that.");
        return;
    }

    // Pay half value
    int payout = value / 2;
    if (payout <= 0) payout = 1;

    // Remove from inventory
    int invSlot = -1;
    for (int i = 0; i < p.invCount; i++) {
        if (p.invIndices[i] == idx) {
            invSlot = i;
            break;
        }
    }

    if (invSlot >= 0) {
        for (int j = invSlot; j < p.invCount - 1; j++)
            p.invIndices[j] = p.invIndices[j + 1];
        p.invCount--;
    }

    // Get item display name for message
    String disp = getItemDisplayName(wi);
    
    // ADD ITEM TO SHOP INVENTORY (using itemId to track it)
    // Use the world item's name as the itemId
    shop->addOrUpdateItem(wi.name, disp, payout);
    
    // Destroy the item (remove from worldItems)
    wi.ownerName = ""; // mark as unowned, effectively removing from world
    
    // Pay the player
    p.coins += payout;

    p.client.println("You sell " + disp + " for " + String(payout) + " gold.");
}


void cmdSellAll(Player &p) {
    // Check if there's a shop in this room
    Shop* shop = getShopForRoom(p);
    if (!shop) {
        p.client.println("You can't sell anything here.");
        return;
    }

    if (p.invCount == 0) {
        p.client.println("You aren't carrying anything.");
        return;
    }

    int itemsSold = 0;
    int totalGold = 0;

    // Make a copy of inventory indices since we'll be modifying the inventory during the loop
    int invIndicesCopy[MAX_INVENTORY];
    for (int i = 0; i < p.invCount; i++) {
        invIndicesCopy[i] = p.invIndices[i];
    }
    int originalCount = p.invCount;

    // Try to sell each item
    for (int i = 0; i < originalCount; i++) {
        int idx = invIndicesCopy[i];
        if (idx < 0 || idx >= (int)worldItems.size()) continue;

        WorldItem &wi = worldItems[idx];

        // Get item type
        String itemType = wi.getAttr("type", itemDefs);
        itemType.toLowerCase();

        // SHOP FILTERING
        if (shop->shopType == "blacksmith") {
            if (itemType != "weapon" && itemType != "armor") {
                continue;  // Skip this item
            }
        } else if (shop->shopType == "misc") {
            if (itemType != "misc") {
                continue;  // Skip this item
            }
        }

        // Get item value
        int value = wi.getAttr("value", itemDefs).toInt();
        if (value <= 0) {
            continue;  // Skip items with no value
        }

        // Pay half value
        int payout = value / 2;
        if (payout <= 0) payout = 1;

        // Remove from inventory
        int invSlot = -1;
        for (int j = 0; j < p.invCount; j++) {
            if (p.invIndices[j] == idx) {
                invSlot = j;
                break;
            }
        }

        if (invSlot >= 0) {
            for (int j = invSlot; j < p.invCount - 1; j++)
                p.invIndices[j] = p.invIndices[j + 1];
            p.invCount--;
        }

        // Add item to shop inventory
        String disp = getItemDisplayName(wi);
        shop->addOrUpdateItem(wi.name, disp, payout);
        
        // Destroy the item
        wi.ownerName = "";
        
        // Pay the player
        p.coins += payout;
        
        itemsSold++;
        totalGold += payout;
    }

    if (itemsSold == 0) {
        p.client.println("You have nothing the shopkeeper wants to buy.");
        return;
    }

    p.client.println("You sell " + String(itemsSold) + " item(s) for " + String(totalGold) + " gold.");
}

void cmdDrink(Player &p, const String &arg) {
    String drinkName = arg;
    drinkName.trim();
    
    if (drinkName.length() == 0) {
        p.client.println("Drink what?");
        return;
    }

    // First try exact match
    int idx = findItemInRoomOrInventory(p, drinkName);

    // If no exact match, try partial match for any item
    if (idx == -1) {
        String searchLower = drinkName;
        searchLower.toLowerCase();
        
        // Search inventory first
        for (int i = 0; i < p.invCount; i++) {
            int worldIndex = p.invIndices[i];
            if (worldIndex < 0 || worldIndex >= (int)worldItems.size())
                continue;
            
            WorldItem &wi = worldItems[worldIndex];
            String id = wi.name;
            id.toLowerCase();
            String disp = resolveDisplayName(wi);
            disp.toLowerCase();
            
            // Check if item matches name or display name (partial)
            if (id.indexOf(searchLower) != -1 || disp.indexOf(searchLower) != -1) {
                idx = (i | 0x80000000);
                break;
            }
        }
        
        // If still not found, search room
        if (idx == -1) {
            Room &r = p.currentRoom;
            for (int i = 0; i < (int)worldItems.size(); i++) {
                WorldItem &wi = worldItems[i];
                if (wi.x != r.x || wi.y != r.y || wi.z != r.z)
                    continue;
                
                String id = wi.name;
                id.toLowerCase();
                String disp = resolveDisplayName(wi);
                disp.toLowerCase();
                
                // Check if item matches name or display name (partial)
                if (id.indexOf(searchLower) != -1 || disp.indexOf(searchLower) != -1) {
                    idx = i;
                    break;
                }
            }
        }
    }

    if (idx == -1) {
        p.client.println("You don't see that here.");
        return;
    }

    bool fromInv = (idx & 0x80000000);
    int worldIndex = fromInv
                     ? p.invIndices[idx & 0x7FFFFFFF]
                     : idx;

    if (worldIndex < 0 || worldIndex >= (int)worldItems.size()) {
        p.client.println("You don't see that here.");
        return;
    }

    WorldItem &wi = worldItems[worldIndex];

    // Check drunkenness before drinking
    if (p.drunkenness <= 0) {
        p.client.println("You are too drunk to drink any more!");
        return;
    }

    // Determine drunkenness cost and healing based on drink name
    String itemName = wi.name;
    itemName.toLowerCase();
    
    int drunkennessCost = 0;
    int heal = 0;
    
    if (itemName == "giants_beer") {
        drunkennessCost = 3;
        heal = 2;
    } else if (itemName == "honeyed_mead") {
        drunkennessCost = 2;
        heal = 3;
    } else if (itemName == "faery_fire") {
        drunkennessCost = 5;
        heal = 5;
    } else {
        p.client.println("You can't drink that!");
        return;
    }

    // Check if player has enough drunkenness units
    if (p.drunkenness < drunkennessCost) {
        p.client.println("You are too drunk to drink any more!");
        return;
    }

    // Deduct drunkenness units
    p.drunkenness -= drunkennessCost;
    if (p.drunkenness < 0) p.drunkenness = 0;

    // Restore HP
    int oldHp = p.hp;
    p.hp += heal;
    if (p.hp > p.maxHp) p.hp = p.maxHp;

    String disp = getItemDisplayName(wi);

    // Message to player
    p.client.println("You drink the " + disp + ".");
    p.client.println("You are feeling a good buzz... *hiccup!*");
    
    // Drunkenness feedback
    if (p.drunkenness == 0) {
        p.client.println("You are now completely drunk and can't drink any more!");
    } else if (p.drunkenness <= 2) {
        p.client.println("You're getting quite drunk...");
    }

    broadcastRoomExcept(
        p,
        capFirst(p.name) + " drinks a " + disp + ".",
        p
    );

    consumeWorldItem(p, worldIndex);
}

// =============================================================
// POST OFFICE SYSTEM - Send mail
// =============================================================

void cmdSend(Player &p, const String &input) {
    // Check if player is in a post office
    PostOffice* po = getPostOfficeForRoom(p);
    if (!po) {
        p.client.println("You must be at a post office to send mail.");
        return;
    }

    // Send mail directly with hard-coded password
    cmdSendMail(p, input);
}

void cmdSendMail(Player &p, const String &input) {
    // Check if player is in a post office
    PostOffice* po = getPostOfficeForRoom(p);
    if (!po) {
        p.client.println("You must be at a post office to send mail.");
        return;
    }

    // Check if player has enough gold (10gp per email)
    const int MAIL_COST = 10;
    if (p.coins < MAIL_COST) {
        p.client.println("The Postal Clerk says: \"Sending mail isn't free! We charge " + String(MAIL_COST) + "gp, which you don't have.\"");
        return;
    }

    String args = input;
    args.trim();
    
    if (args.length() == 0) {
        p.client.println("Send to whom?");
        return;
    }

    // Parse: send <email> <message>
    int spaceIdx = args.indexOf(' ');
    if (spaceIdx == -1) {
        p.client.println("Usage: send [emailaddress] [message]");
        return;
    }

    String emailAddress = args.substring(0, spaceIdx);
    String message = args.substring(spaceIdx + 1);
    
    emailAddress.trim();
    message.trim();
    
    if (emailAddress.length() == 0 || message.length() == 0) {
        p.client.println("Usage: send [emailaddress] [message]");
        return;
    }

    // Attempt to send email with hard-coded password
    p.client.println("Sending mail to " + emailAddress + "...");
    
    String playerTitle = getPlayerTitle(p.raceId, p.level);
    if (sendEmailViaSMTP(emailAddress, message, String(p.name), playerTitle, String(POST_OFFICE_SMTP_PASSWORD))) {
        // Charge the player after successful send
        p.coins -= MAIL_COST;
        p.client.println("The Postal Clerk says: \"It appears this message can be delivered.\"");
        p.client.println("You have been charged " + String(MAIL_COST) + " gold pieces for our trouble.");
    } else {
        p.client.println("The Postal Clerk says: \"Our services are unavailable today. the scribe runner is sick!\"");
    }
}

// =============================
// High-Low Card Game System
// =============================

String getCardName(const Card &card) {
    String suits[] = {"Hearts", "Spades", "Diamonds", "Clubs"};
    String names[] = {"", "", "2", "3", "4", "5", "6", "7", "8", "9", "10", "Jack", "Queen", "King"};
    
    if (card.isAce) {
        return "Ace of " + suits[card.suit];
    }
    return names[card.value] + " of " + suits[card.suit];
}

// Clear telnet screen using ANSI escape sequence
void clearScreen(Player &p) {
    // ANSI/telnet clear screen: move cursor home and clear entire screen
    p.client.print("\033[H\033[2J");
}

// Render card as ASCII art - print each line separately
void printCard(Player &p, const Card &card) {
    clearScreen(p);  // Clear screen before displaying card
    
    p.client.println("Pot is at " + String(globalHighLowPot) + "gp.  You have " + String(p.coins) + " gold coins.");
    p.client.println("");
    
    String suitSymbols[] = {"♥", "♠", "♦", "♣"};
    String ranks[] = {"", "", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
    
    String rank = card.isAce ? "A" : ranks[card.value];
    String suit = suitSymbols[card.suit];
    
    // Card interior is 8 chars wide. Top-left rank + padding to fill 8 chars
    // Bottom-right: padding + rank to fill 8 chars
    String topPad, bottomPad;
    
    if (rank == "10") {
        topPad = "      ";      // 10 = 2 chars, needs 6 spaces
        bottomPad = "      ";   // 6 spaces before rank
    } else {
        topPad = "       ";     // Single char, needs 7 spaces
        bottomPad = "       ";  // 7 spaces before rank
    }
    
    p.client.println("┌────────┐");
    p.client.println("│" + rank + topPad + "│");
    p.client.println("│    " + suit + "   │");
    p.client.println("│" + bottomPad + rank + "│");
    p.client.println("└────────┘");
}

// Print 2 cards side-by-side on same lines
void printTwoCardsSideBySide(Player &p, const Card &card1, const Card &card2) {
    clearScreen(p);  // Clear screen before displaying cards
    
    p.client.println("Pot is at " + String(globalHighLowPot) + "gp.  You have " + String(p.coins) + " gold coins.");
    p.client.println("");
    
    String suitSymbols[] = {"♥", "♠", "♦", "♣"};
    String ranks[] = {"", "", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
    
    auto getRank = [&](const Card &c) { return c.isAce ? "A" : ranks[c.value]; };
    auto getSuit = [&](const Card &c) { return suitSymbols[c.suit]; };
    
    auto getPadding = [](const String &rank) -> String {
        if (rank == "10") {
            return "      ";  // 6 spaces for "10"
        } else {
            return "       ";  // 7 spaces for single char
        }
    };
    
    String r1 = getRank(card1), s1 = getSuit(card1), p1 = getPadding(r1);
    String r2 = getRank(card2), s2 = getSuit(card2), p2 = getPadding(r2);
    
    // Top borders
    p.client.println("┌────────┐      ┌────────┐");
    // Ranks top
    p.client.println("│" + r1 + p1 + "│      │" + r2 + p2 + "│");
    // Suits
    p.client.println("│    " + s1 + "   │      │    " + s2 + "   │");
    // Ranks bottom
    p.client.println("│" + p1 + r1 + "│      │" + p2 + r2 + "│");
    // Bottom borders
    p.client.println("└────────┘      └────────┘");
}

// Render 3 cards: first 2 on top row, 3rd card centered below
void renderThreeCardsSideBySide(Player &p, const Card &card1, const Card &card2, const Card &card3) {
    clearScreen(p);  // Clear screen before displaying cards
    
    p.client.println("Pot is at " + String(globalHighLowPot) + "gp.  You have " + String(p.coins) + " gold coins.");
    p.client.println("");
    
    String suitSymbols[] = {"♥", "♠", "♦", "♣"};
    String ranks[] = {"", "", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"};
    
    auto getRank = [&](const Card &c) { return c.isAce ? "A" : ranks[c.value]; };
    auto getSuit = [&](const Card &c) { return suitSymbols[c.suit]; };
    
    auto getPadding = [](const String &rank) -> String {
        if (rank == "10") {
            return "      ";  // 6 spaces for "10"
        } else {
            return "       ";  // 7 spaces for single char
        }
    };
    
    String r1 = getRank(card1), s1 = getSuit(card1), p1 = getPadding(r1);
    String r2 = getRank(card2), s2 = getSuit(card2), p2 = getPadding(r2);
    String r3 = getRank(card3), s3 = getSuit(card3), p3 = getPadding(r3);
    
    // Top row: 1st and 2nd cards with 6 spaces between
    p.client.println("┌────────┐      ┌────────┐");
    p.client.println("│" + r1 + p1 + "│      │" + r2 + p2 + "│");
    p.client.println("│    " + s1 + "   │      │    " + s2 + "   │");
    p.client.println("│" + p1 + r1 + "│      │" + p2 + r2 + "│");
    p.client.println("└────────┘      └────────┘");
    
    // Bottom row: 3rd card centered with 8 space indent
    p.client.println("");
    p.client.println("        ┌────────┐");
    p.client.println("        │" + r3 + p3 + "│");
    p.client.println("        │    " + s3 + "   │");
    p.client.println("        │" + p3 + r3 + "│");
    p.client.println("        └────────┘");
}

void initializeHighLowSession(int playerIndex) {
    HighLowSession &session = highLowSessions[playerIndex];
    session.deck.clear();
    // Note: pot is now global (globalHighLowPot), not per-player
    session.gameActive = false;
    session.awaitingAceDeclaration = false;
    session.awaitingContinue = false;
    session.betWasPot = false;
    
    // Create 104-card deck (double deck)
    for (int suit = 0; suit < 4; suit++) {
        for (int deck_num = 0; deck_num < 2; deck_num++) {
            // Add cards 2-10
            for (int value = 2; value <= 10; value++) {
                Card c;
                c.value = value;
                c.suit = suit;
                c.isAce = false;
                session.deck.push_back(c);
            }
            // Add Jack, Queen, King, Ace
            for (int value = 11; value <= 13; value++) {
                Card c;
                c.value = value;
                c.suit = suit;
                c.isAce = false;
                session.deck.push_back(c);
            }
            // Add Ace (separate flag)
            Card ace;
            ace.value = 1;  // default low
            ace.suit = suit;
            ace.isAce = true;
            session.deck.push_back(ace);
        }
    }
    
    // Shuffle the deck
    for (int i = session.deck.size() - 1; i > 0; i--) {
        int j = random(0, i + 1);
        Card temp = session.deck[i];
        session.deck[i] = session.deck[j];
        session.deck[j] = temp;
    }
}

void dealHighLowHand(Player &p, int playerIndex) {
    HighLowSession &session = highLowSessions[playerIndex];
    
    // Reset deck if less than 3 cards
    if (session.deck.size() < 3) {
        initializeHighLowSession(playerIndex);
    }
    
    // Deal first two cards
    session.card1 = session.deck.back();
    session.deck.pop_back();
    session.card2 = session.deck.back();
    session.deck.pop_back();
    
    // Ensure card2 creates a valid betting range with card1
    // Keep redrawing card2 until it creates a valid range
    // Invalid: same value OR difference of 1 (no number can fit between them)
    // Exception: Two Aces are always allowed (1 to 14 range)
    while ((session.card1.value == session.card2.value || 
            abs(session.card1.value - session.card2.value) == 1) &&
           !(session.card1.isAce && session.card2.isAce)) {
        // card2 creates invalid range - need to redraw
        
        // If deck is depleted, reshuffle
        if (session.deck.size() < 1) {
            initializeHighLowSession(playerIndex);
        }
        
        session.card2 = session.deck.back();
        session.deck.pop_back();
    }
    
    // Set card values (handle Aces later if needed)
    session.card1Value = session.card1.isAce ? 1 : session.card1.value;
    session.card2Value = session.card2.isAce ? 1 : session.card2.value;
    
    // Check if first card is an Ace - if so, show ONLY first card and wait for declaration
    if (session.card1.isAce) {
        printCard(p, session.card1);
        p.client.println("");
        session.awaitingAceDeclaration = true;
        p.client.println("High or Low?  Enter '1' for Low and '2' for High");
        return;
    }
    
    // 1st card not an Ace - show both cards side-by-side and prompt for bet
    printTwoCardsSideBySide(p, session.card1, session.card2);
    p.client.println("");
    p.client.println("Enter bet amount, 'pot' or 'end':");
}

void processHighLowBet(Player &p, int playerIndex, int betAmount, bool potBet) {
    HighLowSession &session = highLowSessions[playerIndex];
    session.betWasPot = potBet;  // Track if this was a pot bet
    
    // Validate bet
    if (betAmount < 0) {
        p.client.println("Invalid bet amount.");
        return;
    }
    
    // Check minimum bet of 10gp (no passing with 0 anymore)
    if (betAmount < 10) {
        p.client.println("Your bet is too low. Minimum bet is 10gp.");
        p.client.println("Enter bet amount, 'pot' or 'end':");
        return;
    }
    
    if (betAmount > p.coins) {
        p.client.println("You don't have that much gold!");
        return;
    }
    
    // Player must have enough to cover 2x the bet (worst case: POST loss)
    if (betAmount > p.coins / 2) {
        p.client.println("You can't afford to lose double! You can only bet up to " + String(p.coins / 2) + "gp.");
        p.client.println("Enter bet amount, 'pot' or 'end':");
        return;
    }
    
    // Deal third card
    if (session.deck.size() < 1) {
        initializeHighLowSession(playerIndex);
    }
    
    session.card3 = session.deck.back();
    session.deck.pop_back();
    
    p.client.println("");
    renderThreeCardsSideBySide(p, session.card1, session.card2, session.card3);
    
    // Determine win/loss
    // Get the lower and higher card values
    int lowerCard = min(session.card1Value, session.card2Value);
    int higherCard = max(session.card1Value, session.card2Value);
    
    // Determine 3rd card value
    int card3Value;
    if (session.card3.isAce) {
        // If either first or second card is an Ace, 3rd Ace is a WIN
        if (session.card1.isAce || session.card2.isAce) {
            card3Value = -1;  // Flag for automatic win
        } else {
            // 3rd card Ace should be valued as 1 or 14 based on what's optimal
            // Default to 1 (low) unless that makes it outside range
            card3Value = 1;
            if (card3Value < lowerCard || card3Value > higherCard) {
                card3Value = 14;  // Try high instead
            }
        }
    } else {
        card3Value = session.card3.value;
    }
    
    // Check for automatic win (3rd Ace when first or second is also Ace)
    if (card3Value == -1) {
        // WIN - double Ace is automatic win!
        // Player wins their bet amount, pot is reduced by bet
        p.coins += betAmount;
        p.client.println("Double Ace! You WIN " + String(betAmount) + "gp!");
        globalHighLowPot -= betAmount;
        
        // Check if pot is depleted (player won the game)
        if (globalHighLowPot <= 0) {
            globalHighLowPot = 50;  // Reset pot for next player
            p.client.println("The pot is depleted! YOU WIN THE GAME!");
            savePlayerToFS(p);
            endHighLowGame(p, playerIndex);
            return;
        }
        
        // Pot still has money - continue game
        p.client.println("Pot is now at " + String(globalHighLowPot) + "gp.");
        savePlayerToFS(p);
        
        // Prompt for continue
        p.client.println("");
        promptHighLowContinue(p, playerIndex);
        return;
    }
    // Check for post hit (3rd card equals one of first two)
    else if (card3Value == session.card1Value || card3Value == session.card2Value) {
        // POST HIT - player loses double
        int loss = betAmount * 2;
        if (p.coins < loss) {
            p.client.println("You don't have enough to cover double! Game over.");
            endHighLowGame(p, playerIndex);
            return;
        }
        p.coins -= loss;
        
        String card3Name = getCardName(session.card3);
        p.client.println("POST! ... PAY DOUBLE!");
        p.client.println("You pay the dealer " + String(loss) + " gold coins.");
        
        globalHighLowPot += loss;
        savePlayerToFS(p);
        
        // Prompt for continue
        p.client.println("");
        promptHighLowContinue(p, playerIndex);
        return;
    }
    // Check for win (card inside range - strictly between lower and higher card)
    else if (card3Value > lowerCard && card3Value < higherCard) {
        // WIN - player wins their bet amount, pot is reduced
        p.coins += betAmount;
        
        String card3Name = getCardName(session.card3);
        if (session.betWasPot) {
            p.client.println("WIN... TAKE IT!");
            p.client.println("The dealer gives you the entire pot!");
        } else {
            p.client.println("WIN... TAKE IT!");
            p.client.println("The dealer gives you " + String(betAmount) + " gold coins.");
        }
        
        globalHighLowPot -= betAmount;
        
        // Check if pot is depleted (player won the game!)
        if (globalHighLowPot <= 0) {
            globalHighLowPot = 50;  // Reset pot for next player
            p.client.println("The pot is depleted! YOU WIN THE GAME!");
            savePlayerToFS(p);
            endHighLowGame(p, playerIndex);
            return;
        }
        
        // Pot still has money - continue playing
        p.client.println("Pot is now at " + String(globalHighLowPot) + "gp.");
        savePlayerToFS(p);
        
        // Prompt for continue
        p.client.println("");
        promptHighLowContinue(p, playerIndex);
        return;
    }
    // Otherwise lose (card outside range)
    else {
        // LOSE
        p.coins -= betAmount;
        
        String card3Name = getCardName(session.card3);
        if (session.betWasPot) {
            p.client.println("MISS... YOU PAY THE WHOLE POT!");
            p.client.println("You pay the dealer " + String(betAmount) + " gold coins.");
        } else {
            p.client.println("MISS... PAY IT!");
            p.client.println("You pay the dealer " + String(betAmount) + " gold coins.");
        }
        
        globalHighLowPot += betAmount;
        savePlayerToFS(p);
        
        // Prompt for continue
        p.client.println("");
        promptHighLowContinue(p, playerIndex);
        return;
    }
}

void declareAceValue(Player &p, int playerIndex, int aceValue) {
    HighLowSession &session = highLowSessions[playerIndex];
    
    bool card1IsAce = session.card1.isAce;
    bool card2IsAce = session.card2.isAce;
    
    // Only card1 is Ace - use player's declaration
    if (card1IsAce && !card2IsAce) {
        if (aceValue == 1) {
            session.card1Value = 1;
            p.client.println("Ace is LOW.");
        } else if (aceValue == 2) {
            session.card1Value = 14;
            p.client.println("Ace is HIGH.");
        } else {
            p.client.println("Invalid choice. Enter '1' for Low or '2' for High.");
            return;
        }
    }
    // BOTH cards are Aces - set first to declaration, second to opposite
    else if (card1IsAce && card2IsAce) {
        if (aceValue == 1) {
            session.card1Value = 1;   // First Ace is LOW
            session.card2Value = 14;  // Second Ace is HIGH (opposite)
            p.client.println("First Ace is LOW.");
            p.client.println("Second card is also an Ace - automatically set to HIGH.");
        } else if (aceValue == 2) {
            session.card1Value = 14;  // First Ace is HIGH
            session.card2Value = 1;   // Second Ace is LOW (opposite)
            p.client.println("First Ace is HIGH.");
            p.client.println("Second card is also an Ace - automatically set to LOW.");
        } else {
            p.client.println("Invalid choice. Enter '1' for Low or '2' for High.");
            return;
        }
    }
    // Only card2 is Ace - use player's declaration
    else if (!card1IsAce && card2IsAce) {
        if (aceValue == 1) {
            session.card2Value = 1;
            p.client.println("Ace is LOW.");
        } else if (aceValue == 2) {
            session.card2Value = 14;
            p.client.println("Ace is HIGH.");
        } else {
            p.client.println("Invalid choice. Enter '1' for Low or '2' for High.");
            return;
        }
    } else {
        p.client.println("Invalid choice. Enter '1' for Low or '2' for High.");
        return;
    }
    
    session.awaitingAceDeclaration = false;
    
    // Ready for betting - show both cards side-by-side
    p.client.println("");
    printTwoCardsSideBySide(p, session.card1, session.card2);
    p.client.println("");
    p.client.println("Enter bet amount, 'pot' or 'end':");
}

void promptHighLowContinue(Player &p, int playerIndex) {
    HighLowSession &session = highLowSessions[playerIndex];
    
    // Mark that we're waiting for continue/end decision
    session.awaitingContinue = true;
    p.client.println("Press [Enter] to continue or type 'end'");
}

void endHighLowGame(Player &p, int playerIndex) {
    HighLowSession &session = highLowSessions[playerIndex];
    
    session.gameActive = false;
    session.awaitingAceDeclaration = false;
    session.awaitingContinue = false;
    p.client.println("");
    p.client.println("Game ended.");
    p.client.println("");
    
    // Show the Game Parlor sign
    p.client.println("===== GAME PARLOR GAMES ===============");
    p.client.println("1. High-Low Card Game - Test your luck!");
    p.client.println("2. Chess - match wits against a local!");
    p.client.println("");
    p.client.println("Type 'play [#]' to play!");
    p.client.println("Type 'rules [#]' for game rules!");
    p.client.println("=======================================");
    p.client.println("");
}

// =============================
// CHESS GAME FUNCTIONS
// =============================


// Helper: Initialize board to starting position
void initializeChessBoard(unsigned char board[64]) {
    // Clear board
    memset(board, 0, 64);
    
    // Set up pawns
    for (int i = 0; i < 8; i++) {
        board[8 + i] = 1;        // White pawns on rank 2
        board[48 + i] = 7;       // Black pawns on rank 7
    }
    
    // Set up pieces
    // Rank 1 (White): a1=R, b1=N, c1=B, d1=Q, e1=K, f1=B, g1=N, h1=R
    board[0] = 4;   // a1: White Rook
    board[1] = 2;   // b1: White Knight
    board[2] = 3;   // c1: White Bishop
    board[3] = 5;   // d1: White Queen
    board[4] = 6;   // e1: White King
    board[5] = 3;   // f1: White Bishop
    board[6] = 2;   // g1: White Knight
    board[7] = 4;   // h1: White Rook
    
    // Rank 8 (Black)
    board[56] = 10; // a8: Black Rook
    board[57] = 8;  // b8: Black Knight
    board[58] = 9;  // c8: Black Bishop
    board[59] = 11; // d8: Black Queen
    board[60] = 12; // e8: Black King
    board[61] = 9;  // f8: Black Bishop
    board[62] = 8;  // g8: Black Knight
    board[63] = 10; // h8: Black Rook
}

void initChessGame(ChessSession &session, bool playerIsWhite) {
    session.gameActive = true;
    session.playerIsWhite = playerIsWhite;
    session.moveCount = 0;
    session.isBlackToMove = false;  // White always moves first
    session.lastEngineMove = "";
    session.lastPlayerMove = "";
    session.gameEnded = false;
    session.endReason = "";
    
    // Initialize board to standard starting position
    initializeChessBoard(session.board);
}

// Get piece character for display
char getPieceChar(unsigned char piece) {
    switch(piece) {
        case 0: return ' ';
        case 1: return 'P'; // White Pawn
        case 2: return 'N'; // White Knight
        case 3: return 'B'; // White Bishop
        case 4: return 'R'; // White Rook
        case 5: return 'Q'; // White Queen
        case 6: return 'K'; // White King
        case 7: return 'P'; // Black Pawn
        case 8: return 'N'; // Black Knight
        case 9: return 'B'; // Black Bishop
        case 10: return 'R'; // Black Rook
        case 11: return 'Q'; // Black Queen
        case 12: return 'K'; // Black King
        default: return '?';
    }
}

// Check if piece is white (1-6)
bool isWhitePiece(unsigned char piece) {
    return piece > 0 && piece < 7;
}

// Check if piece is black (7-12)
bool isBlackPiece(unsigned char piece) {
    return piece > 6 && piece < 13;
}

void renderChessBoard(Player &p, ChessSession &session) {
    p.client.println("\x1B[2J\x1B[H");  // Clear screen
    
    // Render from rank 8 down to rank 1 (top to bottom)
    p.client.println("       ---------------------------------");
    for (int rank = 7; rank >= 0; rank--) {
        p.client.print("    " + String(rank + 1) + "  |");
        
        // Determine file order based on player color
        if (session.playerIsWhite) {
            // White: files go a-h (left to right) = 0-7
            for (int file = 0; file <= 7; file++) {
                unsigned char piece = session.board[rank * 8 + file];
                char pieceChar = getPieceChar(piece);
                bool isBlack = isBlackPiece(piece);
                
                if (isBlack) {
                    p.client.print(" *" + String(pieceChar) + "|");
                } else {
                    p.client.print("  " + String(pieceChar) + "|");
                }
            }
        } else {
            // Black: files go h-a (left to right) = 7-0 (flipped)
            for (int file = 7; file >= 0; file--) {
                unsigned char piece = session.board[rank * 8 + file];
                char pieceChar = getPieceChar(piece);
                bool isBlack = isBlackPiece(piece);
                
                if (isBlack) {
                    p.client.print(" *" + String(pieceChar) + "|");
                } else {
                    p.client.print("  " + String(pieceChar) + "|");
                }
            }
        }
        
        // Right side info
        if (rank == 7) {
            p.client.println("     Move # : " + String(session.moveCount) + " (" + 
                           String(session.isBlackToMove ? "Black" : "White") + ")");
        } else if (rank == 6) {
            p.client.println("     " + 
                           (session.lastEngineMove.length() > 0 ? 
                            "White Moves : '" + session.lastEngineMove + "'" : 
                            "White Moves : '--'"));
        } else if (rank == 4) {
            p.client.println("     Black to move: " + String(session.isBlackToMove ? "YES" : "NO"));
        } else {
            p.client.println("");
        }
        
        p.client.println("       |---+---+---+---+---+---+---+---|");
    }
    
    // Show file notation based on player's perspective
    if (session.playerIsWhite) {
        p.client.println("         a   b   c   d   e   f   g   h");
    } else {
        p.client.println("         h   g   f   e   d   c   b   a");
    }
    p.client.println("");
}

String formatTime(unsigned long ms) {
    int minutes = ms / 60000;
    int seconds = (ms % 60000) / 1000;
    String result = "";
    if (minutes < 10) result += "0";
    result += String(minutes) + ":";
    if (seconds < 10) result += "0";
    result += String(seconds);
    return result;
}

bool parseChessMove(String moveStr, int &fromCol, int &fromRow, int &toCol, int &toRow) {
    moveStr.toLowerCase();
    moveStr.trim();
    
    // Standard algebraic notation: d2d4 (from d-file 2nd rank to d-file 4th rank)
    if (moveStr.length() == 4) {
        if (!isalpha(moveStr[0]) || !isdigit(moveStr[1]) || 
            !isalpha(moveStr[2]) || !isdigit(moveStr[3])) {
            return false;
        }
        
        fromCol = moveStr[0] - 'a';  // a=0, h=7
        fromRow = moveStr[1] - '1';  // 1=0, 8=7
        toCol = moveStr[2] - 'a';
        toRow = moveStr[3] - '1';
        
        // Validate ranges
        if (fromCol < 0 || fromCol > 7 || fromRow < 0 || fromRow > 7 ||
            toCol < 0 || toCol > 7 || toRow < 0 || toRow > 7) {
            return false;
        }
        
        return true;
    }
    
    // Piece shorthand notation: Rh4, Nf3, Bf5, Qd4, Ke2 (piece letter + destination)
    if (moveStr.length() == 3) {
        char pieceLetter = moveStr[0];
        if ((pieceLetter != 'R' && pieceLetter != 'N' && pieceLetter != 'B' && 
             pieceLetter != 'Q' && pieceLetter != 'K') ||
            !isalpha(moveStr[1]) || !isdigit(moveStr[2])) {
            // Not piece shorthand, might be 2-char shorthand below
            if (moveStr.length() == 2) {
                if (!isalpha(moveStr[0]) || !isdigit(moveStr[1])) {
                    return false;
                }
                toCol = moveStr[0] - 'a';
                toRow = moveStr[1] - '1';
                if (toCol < 0 || toCol > 7 || toRow < 0 || toRow > 7) {
                    return false;
                }
                fromCol = -1;
                fromRow = -1;
                return true;
            }
            return false;
        }
        
        toCol = moveStr[1] - 'a';
        toRow = moveStr[2] - '1';
        
        // Validate ranges
        if (toCol < 0 || toCol > 7 || toRow < 0 || toRow > 7) {
            return false;
        }
        
        // Use special markers to indicate piece shorthand: fromCol = piece code
        // fromCol: 0=Rook, 1=Knight, 2=Bishop, 3=Queen, 4=King
        // fromRow = -1 to distinguish from 2-char shorthand
        switch(pieceLetter) {
            case 'R': fromCol = 0; break;
            case 'N': fromCol = 1; break;
            case 'B': fromCol = 2; break;
            case 'Q': fromCol = 3; break;
            case 'K': fromCol = 4; break;
        }
        fromRow = -1;
        
        return true;
    }
    
    // Shorthand notation: d4, e5 (just destination square)
    if (moveStr.length() == 2) {
        if (!isalpha(moveStr[0]) || !isdigit(moveStr[1])) {
            return false;
        }
        
        toCol = moveStr[0] - 'a';
        toRow = moveStr[1] - '1';
        
        // Validate ranges
        if (toCol < 0 || toCol > 7 || toRow < 0 || toRow > 7) {
            return false;
        }
        
        // Return special markers in fromCol/fromRow to indicate shorthand
        fromCol = -1;
        fromRow = -1;
        
        return true;
    }
    
    return false;
}

// Check if a move is legally possible for the piece at fromSquare
bool isLegalMove(unsigned char board[64], int fromRow, int fromCol, int toRow, int toCol, bool isWhiteMove) {
    unsigned char piece = board[fromRow * 8 + fromCol];
    unsigned char target = board[toRow * 8 + toCol];
    
    // Can't move empty square
    if (piece == 0) return false;
    
    // Can't move opponent's piece
    if (isWhiteMove && !isWhitePiece(piece)) return false;
    if (!isWhiteMove && !isBlackPiece(piece)) return false;
    
    // Can't capture own piece
    if (isWhiteMove && isWhitePiece(target)) return false;
    if (!isWhiteMove && isBlackPiece(target)) return false;
    
    // Can't move to same square
    if (fromRow == toRow && fromCol == toCol) return false;
    
    unsigned char baseType = piece > 6 ? piece - 6 : piece;
    
    // Pawn (1)
    if (baseType == 1) {
        int direction = isWhiteMove ? 1 : -1;
        int startRow = isWhiteMove ? 1 : 6;
        
        // Forward move
        if (toCol == fromCol) {
            // One square forward
            if (toRow == fromRow + direction && board[toRow * 8 + toCol] == 0) {
                return true;
            }
            // Two squares forward from starting position
            if (fromRow == startRow && toRow == fromRow + 2 * direction && 
                board[toRow * 8 + toCol] == 0 && 
                board[(fromRow + direction) * 8 + fromCol] == 0) {
                return true;
            }
        }
        // Capture diagonally
        if (abs(toCol - fromCol) == 1 && toRow == fromRow + direction && target != 0) {
            return true;
        }
        return false;
    }
    
    // Knight (2)
    if (baseType == 2) {
        int dRow = abs(toRow - fromRow);
        int dCol = abs(toCol - fromCol);
        return (dRow == 2 && dCol == 1) || (dRow == 1 && dCol == 2);
    }
    
    // Bishop (3) - diagonal
    if (baseType == 3) {
        if (abs(toRow - fromRow) != abs(toCol - fromCol)) return false;
        int dRow = (toRow > fromRow) ? 1 : -1;
        int dCol = (toCol > fromCol) ? 1 : -1;
        int r = fromRow + dRow;
        int c = fromCol + dCol;
        while (r != toRow) {
            if (board[r * 8 + c] != 0) return false;
            r += dRow;
            c += dCol;
        }
        return true;
    }
    
    // Rook (4) - straight
    if (baseType == 4) {
        if (toRow != fromRow && toCol != fromCol) return false;
        if (toRow == fromRow) {
            int dir = (toCol > fromCol) ? 1 : -1;
            for (int c = fromCol + dir; c != toCol; c += dir) {
                if (board[fromRow * 8 + c] != 0) return false;
            }
        } else {
            int dir = (toRow > fromRow) ? 1 : -1;
            for (int r = fromRow + dir; r != toRow; r += dir) {
                if (board[r * 8 + fromCol] != 0) return false;
            }
        }
        return true;
    }
    
    // Queen (5) - rook + bishop
    if (baseType == 5) {
        // Rook-like move
        if (toRow == fromRow || toCol == fromCol) {
            if (toRow == fromRow) {
                int dir = (toCol > fromCol) ? 1 : -1;
                for (int c = fromCol + dir; c != toCol; c += dir) {
                    if (board[fromRow * 8 + c] != 0) return false;
                }
            } else {
                int dir = (toRow > fromRow) ? 1 : -1;
                for (int r = fromRow + dir; r != toRow; r += dir) {
                    if (board[r * 8 + fromCol] != 0) return false;
                }
            }
            return true;
        }
        // Bishop-like move
        if (abs(toRow - fromRow) == abs(toCol - fromCol)) {
            int dRow = (toRow > fromRow) ? 1 : -1;
            int dCol = (toCol > fromCol) ? 1 : -1;
            int r = fromRow + dRow;
            int c = fromCol + dCol;
            while (r != toRow) {
                if (board[r * 8 + c] != 0) return false;
                r += dRow;
                c += dCol;
            }
            return true;
        }
        return false;
    }
    
    // King (6)
    if (baseType == 6) {
        return abs(toRow - fromRow) <= 1 && abs(toCol - fromCol) <= 1;
    }
    
    return false;
}

// Apply a move to the board
void applyMove(unsigned char board[64], int fromRow, int fromCol, int toRow, int toCol) {
    board[toRow * 8 + toCol] = board[fromRow * 8 + fromCol];
    board[fromRow * 8 + fromCol] = 0;
}

// Find king position for a side
bool findKing(unsigned char board[64], bool isWhite, int &kingRow, int &kingCol) {
    unsigned char kingPiece = isWhite ? 6 : 12;
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            if (board[r * 8 + c] == kingPiece) {
                kingRow = r;
                kingCol = c;
                return true;
            }
        }
    }
    return false;
}

// Check if a side is in check
bool isInCheck(unsigned char board[64], bool isWhite) {
    int kingRow, kingCol;
    if (!findKing(board, isWhite, kingRow, kingCol)) return false;
    
    // Check if any opponent piece can attack the king
    for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 8; c++) {
            unsigned char piece = board[r * 8 + c];
            bool isOpponentPiece = isWhite ? isBlackPiece(piece) : isWhitePiece(piece);
            
            if (isOpponentPiece && isLegalMove(board, r, c, kingRow, kingCol, !isWhite)) {
                return true;
            }
        }
    }
    return false;
}

// Check if a side has any legal moves
bool hasLegalMoves(unsigned char board[64], bool isWhite) {
    for (int fromR = 0; fromR < 8; fromR++) {
        for (int fromC = 0; fromC < 8; fromC++) {
            unsigned char piece = board[fromR * 8 + fromC];
            bool isPiece = isWhite ? isWhitePiece(piece) : isBlackPiece(piece);
            
            if (!isPiece) continue;
            
            for (int toR = 0; toR < 8; toR++) {
                for (int toC = 0; toC < 8; toC++) {
                    if (isLegalMove(board, fromR, fromC, toR, toC, isWhite)) {
                        // Test if move leaves king in check
                        unsigned char testBoard[64];
                        memcpy(testBoard, board, 64);
                        applyMove(testBoard, fromR, fromC, toR, toC);
                        
                        if (!isInCheck(testBoard, isWhite)) {
                            return true;
                        }
                    }
                }
            }
        }
    }
    return false;
}

// Check for checkmate or stalemate
bool checkGameEnd(unsigned char board[64], bool isWhite, String &reason) {
    bool inCheck = isInCheck(board, isWhite);
    bool hasLegals = hasLegalMoves(board, isWhite);
    
    if (!hasLegals) {
        if (inCheck) {
            reason = isWhite ? "White is checkmated!" : "Black is checkmated!";
            return true;
        } else {
            reason = "Stalemate! The game is a draw.";
            return true;
        }
    }
    
    return false;
}

void startChessGame(Player &p, int playerIndex, ChessSession &session) {
    bool playerIsWhite = (playerIndex % 2 == 0);
    
    initChessGame(session, playerIsWhite);
    session.gameRoomX = p.roomX;
    session.gameRoomY = p.roomY;
    session.gameRoomZ = p.roomZ;
    
    p.client.println("Welcome to Chess!");
    p.client.println("You are playing as " + String(playerIsWhite ? "WHITE" : "BLACK") + ".");
    p.client.println("");
    p.client.println("Enter moves in format: d2d4 (from d2 to d4)");
    p.client.println("Type 'resign' to give up, 'end' to quit.");
    p.client.println("");
    
    renderChessBoard(p, session);
}

String formatChessMove(unsigned char board[64], int fromR, int fromC, int toR, int toC, bool isPlayerWhite) {
    unsigned char movingPiece = board[fromR * 8 + fromC];
    unsigned char targetPiece = board[toR * 8 + toC];
    
    // Get piece name
    String pieceName = "";
    bool isPawn = false;
    switch(movingPiece) {
        case 1: case 7: pieceName = "Pawn"; isPawn = true; break;
        case 2: case 8: pieceName = "Knight"; break;
        case 3: case 9: pieceName = "Bishop"; break;
        case 4: case 10: pieceName = "Rook"; break;
        case 5: case 11: pieceName = "Queen"; break;
        case 6: case 12: pieceName = "King"; break;
    }
    
    // Get target square notation
    char toColChar = 'a' + toC;
    char toRowChar = '1' + toR;
    String toSquare = String(toColChar) + String(toRowChar);
    
    // Check for castling
    if ((movingPiece == 6 || movingPiece == 12) && abs(fromC - toC) == 2) {
        // King moved 2 squares = castling
        if (toC > fromC) {
            return "Castles King Side";
        } else {
            return "Castles Queen Side";
        }
    }
    
    // Check for capture
    if (targetPiece != 0) {
        String targetName = "";
        switch(targetPiece) {
            case 1: case 7: targetName = "Pawn"; break;
            case 2: case 8: targetName = "Knight"; break;
            case 3: case 9: targetName = "Bishop"; break;
            case 4: case 10: targetName = "Rook"; break;
            case 5: case 11: targetName = "Queen"; break;
            case 6: case 12: targetName = "King"; break;
        }
        // For pawn captures, use shorthand: "exd4" style notation
        if (isPawn) {
            char fromColChar = 'a' + fromC;
            return String(fromColChar) + "x" + toSquare;
        }
        return pieceName + " takes " + targetName + " on " + toSquare;
    }
    
    // Regular move - shorthand for pawns, full notation for other pieces
    if (isPawn) {
        return toSquare;
    }
    return pieceName + " to " + toSquare;
}

void processChessMove(Player &p, int playerIndex, ChessSession &session, String moveStr) {
    int fromCol, fromRow, toCol, toRow;
    
    if (!parseChessMove(moveStr, fromCol, fromRow, toCol, toRow)) {
        p.client.println("Invalid move format. Use: d2d4 (full) or d4 (pawn) or Rh4 (piece shorthand)");
        return;
    }
    
    // Handle shorthand notation (e.g., "d4" or "Rh4")
    if (fromCol == -1 && fromRow == -1) {
        // 2-character pawn shorthand (d4, e5, etc.)
        // Find which piece can move to this square
        bool isPlayerWhite = session.playerIsWhite;
        bool foundMove = false;
        
        for (int r = 0; r < 8 && !foundMove; r++) {
            for (int c = 0; c < 8 && !foundMove; c++) {
                unsigned char piece = session.board[r * 8 + c];
                if (piece == 0) continue;
                
                // Check if it's player's piece
                if (isPlayerWhite && !isWhitePiece(piece)) continue;
                if (!isPlayerWhite && !isBlackPiece(piece)) continue;
                
                // Check if this piece can legally move to toRow, toCol
                if (isLegalMove(session.board, r, c, toRow, toCol, isPlayerWhite)) {
                    unsigned char testBoard[64];
                    memcpy(testBoard, session.board, 64);
                    applyMove(testBoard, r, c, toRow, toCol);
                    
                    if (!isInCheck(testBoard, isPlayerWhite)) {
                        fromRow = r;
                        fromCol = c;
                        foundMove = true;
                    }
                }
            }
        }
        
        if (!foundMove) {
            p.client.println("No legal move to that square!");
            return;
        }
    }
    // Handle piece shorthand notation (e.g., "Rh4", "Nf3")
    else if (fromRow == -1 && fromCol >= 0 && fromCol <= 4) {
        // fromCol contains piece code: 0=Rook, 1=Knight, 2=Bishop, 3=Queen, 4=King
        int pieceCode = fromCol;
        unsigned char targetPiece = 0;
        bool isPlayerWhite = session.playerIsWhite;
        
        // Determine which piece to search for
        switch(pieceCode) {
            case 0: targetPiece = isPlayerWhite ? 4 : 10; break;  // Rook
            case 1: targetPiece = isPlayerWhite ? 2 : 8; break;   // Knight
            case 2: targetPiece = isPlayerWhite ? 3 : 9; break;   // Bishop
            case 3: targetPiece = isPlayerWhite ? 5 : 11; break;  // Queen
            case 4: targetPiece = isPlayerWhite ? 6 : 12; break;  // King
        }
        
        bool foundMove = false;
        
        // Find a piece of the specified type that can move to the destination
        for (int r = 0; r < 8 && !foundMove; r++) {
            for (int c = 0; c < 8 && !foundMove; c++) {
                unsigned char piece = session.board[r * 8 + c];
                
                // Check if this is the piece we're looking for
                if (piece != targetPiece) continue;
                
                // Check if this piece can legally move to toRow, toCol
                if (isLegalMove(session.board, r, c, toRow, toCol, isPlayerWhite)) {
                    unsigned char testBoard[64];
                    memcpy(testBoard, session.board, 64);
                    applyMove(testBoard, r, c, toRow, toCol);
                    
                    if (!isInCheck(testBoard, isPlayerWhite)) {
                        fromRow = r;
                        fromCol = c;
                        foundMove = true;
                    }
                }
            }
        }
        
        if (!foundMove) {
            p.client.println("No legal move for that piece to that square!");
            return;
        }
    }
    
    // Check if game is already over
    if (session.gameEnded) {
        p.client.println("Game has ended: " + session.endReason);
        return;
    }
    
    // Check if it's the player's turn
    bool isPlayerWhite = session.playerIsWhite;
    bool isPlayerTurn = (isPlayerWhite && !session.isBlackToMove) || (!isPlayerWhite && session.isBlackToMove);
    
    if (!isPlayerTurn) {
        p.client.println("It's not your turn!");
        return;
    }
    
    // Validate the move
    if (!isLegalMove(session.board, fromRow, fromCol, toRow, toCol, isPlayerWhite)) {
        p.client.println("Illegal move! That piece cannot move there.");
        return;
    }
    
    // Test if move leaves player's king in check
    unsigned char testBoard[64];
    memcpy(testBoard, session.board, 64);
    applyMove(testBoard, fromRow, fromCol, toRow, toCol);
    
    if (isInCheck(testBoard, isPlayerWhite)) {
        p.client.println("Illegal move! Your king would be in check.");
        return;
    }
    
    // Save info about the move before applying it
    unsigned char movedPiece = session.board[fromRow * 8 + fromCol];
    unsigned char capturedPiece = session.board[toRow * 8 + toCol];
    
    // Move is valid - apply it
    applyMove(session.board, fromRow, fromCol, toRow, toCol);
    session.lastPlayerMove = moveStr;
    session.isBlackToMove = !session.isBlackToMove;
    session.moveCount++;
    
    // Display formatted move
    p.client.println("Your move: " + formatChessMove(session.board, fromRow, fromCol, toRow, toCol, isPlayerWhite));
    p.client.println("");
    
    // Render the board showing the player's move
    renderChessBoard(p, session);
    p.client.println("");
    
    // Check if player's move ended the game
    String endReason = "";
    if (checkGameEnd(session.board, !isPlayerWhite, endReason)) {
        session.gameEnded = true;
        session.endReason = endReason;
        p.client.println(endReason);
        return;
    }
    
    // Engine's turn
    p.client.println("Local Game Parlor local thinking about his move...");
    
    bool foundEngineMove = false;
    String engineMove = "";
    int bestFromR = -1, bestFromC = -1, bestToR = -1, bestToC = -1;
    
    // Greedy engine: prioritize captures, then checks, then any legal move
    // First pass: look for captures
    for (int fromR = 0; fromR < 8 && !foundEngineMove; fromR++) {
        for (int fromC = 0; fromC < 8 && !foundEngineMove; fromC++) {
            unsigned char piece = session.board[fromR * 8 + fromC];
            bool isEnginePiece = isPlayerWhite ? isBlackPiece(piece) : isWhitePiece(piece);
            
            if (!isEnginePiece) continue;
            
            for (int toR = 0; toR < 8 && !foundEngineMove; toR++) {
                for (int toC = 0; toC < 8 && !foundEngineMove; toC++) {
                    unsigned char target = session.board[toR * 8 + toC];
                    if (target == 0) continue;  // Skip non-captures
                    
                    if (isLegalMove(session.board, fromR, fromC, toR, toC, !isPlayerWhite)) {
                        memcpy(testBoard, session.board, 64);
                        applyMove(testBoard, fromR, fromC, toR, toC);
                        
                        if (!isInCheck(testBoard, !isPlayerWhite)) {
                            bestFromR = fromR;
                            bestFromC = fromC;
                            bestToR = toR;
                            bestToC = toC;
                            foundEngineMove = true;
                        }
                    }
                }
            }
        }
    }
    
    // Second pass: look for checks if no capture found
    if (!foundEngineMove) {
        for (int fromR = 0; fromR < 8 && !foundEngineMove; fromR++) {
            for (int fromC = 0; fromC < 8 && !foundEngineMove; fromC++) {
                unsigned char piece = session.board[fromR * 8 + fromC];
                bool isEnginePiece = isPlayerWhite ? isBlackPiece(piece) : isWhitePiece(piece);
                
                if (!isEnginePiece) continue;
                
                for (int toR = 0; toR < 8 && !foundEngineMove; toR++) {
                    for (int toC = 0; toC < 8 && !foundEngineMove; toC++) {
                        if (isLegalMove(session.board, fromR, fromC, toR, toC, !isPlayerWhite)) {
                            memcpy(testBoard, session.board, 64);
                            applyMove(testBoard, fromR, fromC, toR, toC);
                            
                            if (!isInCheck(testBoard, !isPlayerWhite) && isInCheck(testBoard, isPlayerWhite)) {
                                bestFromR = fromR;
                                bestFromC = fromC;
                                bestToR = toR;
                                bestToC = toC;
                                foundEngineMove = true;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Third pass: take any legal move if no capture or check found
    if (!foundEngineMove) {
        for (int fromR = 0; fromR < 8 && !foundEngineMove; fromR++) {
            for (int fromC = 0; fromC < 8 && !foundEngineMove; fromC++) {
                unsigned char piece = session.board[fromR * 8 + fromC];
                bool isEnginePiece = isPlayerWhite ? isBlackPiece(piece) : isWhitePiece(piece);
                
                if (!isEnginePiece) continue;
                
                for (int toR = 0; toR < 8 && !foundEngineMove; toR++) {
                    for (int toC = 0; toC < 8 && !foundEngineMove; toC++) {
                        if (isLegalMove(session.board, fromR, fromC, toR, toC, !isPlayerWhite)) {
                            memcpy(testBoard, session.board, 64);
                            applyMove(testBoard, fromR, fromC, toR, toC);
                            
                            if (!isInCheck(testBoard, !isPlayerWhite)) {
                                bestFromR = fromR;
                                bestFromC = fromC;
                                bestToR = toR;
                                bestToC = toC;
                                foundEngineMove = true;
                            }
                        }
                    }
                }
            }
        }
    }
    
    // Allow 5 seconds for the search (all other players will see this delay)
    delay(5000);
    
    // Apply the move found
    if (foundEngineMove) {
        // Save move info before applying
        unsigned char enginePiece = session.board[bestFromR * 8 + bestFromC];
        unsigned char engineCaptured = session.board[bestToR * 8 + bestToC];
        
        // Build engine move notation (always with full piece names, no shorthand)
        String pieceName = "";
        switch(enginePiece) {
            case 1: case 7: pieceName = "Pawn"; break;
            case 2: case 8: pieceName = "Knight"; break;
            case 3: case 9: pieceName = "Bishop"; break;
            case 4: case 10: pieceName = "Rook"; break;
            case 5: case 11: pieceName = "Queen"; break;
            case 6: case 12: pieceName = "King"; break;
        }
        
        char toColChar = 'a' + bestToC;
        char toRowChar = '1' + bestToR;
        String toSquare = String(toColChar) + String(toRowChar);
        
        String engineMoveNotation = "";
        
        // Check for castling
        if ((enginePiece == 6 || enginePiece == 12) && abs(bestFromC - bestToC) == 2) {
            engineMoveNotation = (bestToC > bestFromC) ? "Castles King Side" : "Castles Queen Side";
        }
        // Check for capture
        else if (engineCaptured != 0) {
            String capturedName = "";
            switch(engineCaptured) {
                case 1: case 7: capturedName = "Pawn"; break;
                case 2: case 8: capturedName = "Knight"; break;
                case 3: case 9: capturedName = "Bishop"; break;
                case 4: case 10: capturedName = "Rook"; break;
                case 5: case 11: capturedName = "Queen"; break;
                case 6: case 12: capturedName = "King"; break;
            }
            engineMoveNotation = pieceName + " takes " + capturedName + " on " + toSquare;
        }
        // Regular move
        else {
            engineMoveNotation = pieceName + " moves to " + toSquare;
        }
        
        applyMove(session.board, bestFromR, bestFromC, bestToR, bestToC);
        
        char fromColChar = 'a' + bestFromC;
        char fromRowChar = '1' + bestFromR;
        char toColChar2 = 'a' + bestToC;
        char toRowChar2 = '1' + bestToR;
        
        engineMove = String(fromColChar) + String(fromRowChar) + String(toColChar2) + String(toRowChar2);
        session.lastEngineMove = engineMove;
        
        // Render the board with the engine's move applied
        renderChessBoard(p, session);
        p.client.println("");
        p.client.println("Engine's move: " + engineMoveNotation + ".");
    } else {
        // No legal moves found - check for game end
        p.client.println("ERROR: Engine could not find a legal move!");
        String engineEndReason = "";
        if (checkGameEnd(session.board, !isPlayerWhite, engineEndReason)) {
            session.gameEnded = true;
            session.endReason = engineEndReason;
            renderChessBoard(p, session);
            p.client.println("");
            p.client.println(engineEndReason);
            return;
        }
    }
    
    session.isBlackToMove = !session.isBlackToMove;
    
    // Check if engine move ends the game
    if (checkGameEnd(session.board, isPlayerWhite, endReason)) {
        session.gameEnded = true;
        session.endReason = endReason;
    }
    
    p.client.println("");
}

void endChessGame(Player &p, int playerIndex) {
    ChessSession &session = chessSessions[playerIndex];
    session.gameActive = false;
    
    p.client.println("Game ended. Returning to Game Parlor...");
    p.client.println("");
    
    // Show the Game Parlor sign
    p.client.println("===== GAME PARLOR GAMES ===============");
    p.client.println("1. High-Low Card Game - Test your luck!");
    p.client.println("2. Chess - match wits against a local!");
    p.client.println("");
    p.client.println("Type 'play [#]' to play!");
    p.client.println("Type 'rules [#]' for game rules!");
    p.client.println("=======================================");
    p.client.println("");
}

// =============================
// Score, levels, and help
// =============================

void cmdScore(Player &p) {
    p.client.println("");
    p.client.println("======== Your Score =========");

    String nameLine = capFirst(p.name);
    if (p.IsWizard) {
        nameLine += " (Wizard)";
    }
    p.client.println("Name: " + nameLine);

    p.client.print("Race: ");
    p.client.println(raceNames[p.raceId]);

    p.client.print("Title: ");
    p.client.println(getTitle(p));

    p.client.print("Level: ");
    p.client.println(p.level);

    p.client.print("Experience: ");
    p.client.println(p.xp);

    // ----------------------------------------------------
    // XP needed to advance
    // ----------------------------------------------------
    if (p.level >= 20) {
        p.client.println("You have reached the maximum level.");
    } else {
        int nextLevel = p.level + 1;

        // expChart[level] = XP required to REACH that level
        long threshold = expChart[p.level];   // correct index
        long xpNeeded = threshold - p.xp;
        if (xpNeeded < 0) xpNeeded = 0;

        p.client.println(
            "You need " +
            String(xpNeeded) +
            " more for Level " +
            String(nextLevel)
        );
    }

    int safeHP = p.hp;
    if (safeHP < 0) safeHP = 0;

    p.client.println("HP: " + String(safeHP) + "/" + String(p.maxHp));

    p.client.print("Gold Coins: ");
    p.client.println(p.coins);

    p.client.print("Quests Completed: ");
    p.client.println(p.questsCompleted);

    p.client.print("Wimp mode: ");
    p.client.println(p.wimpMode ? "ON" : "OFF");

    p.client.println("=============================");
}


int getLevelFromXp(long xp) {
    int level = 1;

    for (int lvl = 1; lvl <= 20; lvl++) {
        if (xp >= expChart[lvl]) {
            level = lvl + 1;   // ⭐ FIXED: shift up one level
        } else {
            break;
        }
    }

    if (level > 20) level = 20;
    return level;
}


int getToHitBonus(int level) {
    return (level - 1) * 2;   // Example: +2% per level
}

void applyLevelBonuses(Player &p) {
    // Simple linear scaling (adjust as needed)
    p.attack = 1 + (p.level - 1);        // +1 damage per level
    p.baseDefense = 9 + (p.level - 1);   // +1 AC per level
}

// Level Advance!
void cmdAdvance(Player &p) {

    int correctLevel = getLevelFromXp(p.xp);

    // Already at correct level
    if (correctLevel <= p.level) {
        p.client.println("You do not have enough experience to advance.");
        return;
    }

    // Advance!
    p.level = correctLevel;
    applyLevelBonuses(p);

    p.client.println("Congratulations! You have advanced to level " + String(p.level) + "!");

    // Build full title safely using Arduino String
    String fullTitle = String(p.name) + " The " + String(titles[p.raceId][p.level - 1]);
    p.client.println("You are now known as: " + fullTitle);

    savePlayerToFS(p);
}


void cmdLevels(Player &p) {
    p.client.println("===== Experience Levels =====");

    for (int lvl = 1; lvl <= 20; lvl++) {

        long low, high;

        if (lvl == 1) {
            low = 0;
            high = expChart[1] - 1;
        } else {
            low = expChart[lvl - 1];
            high = expChart[lvl] - 1;
        }

        // Level 20 ends exactly at expChart[20]
        if (lvl == 20) {
            high = expChart[20];
        }

        char titleBuf[32];
        snprintf(titleBuf, sizeof(titleBuf), "%-25s", titles[p.raceId][lvl - 1]);

        char rangeBuf[32];
        snprintf(rangeBuf, sizeof(rangeBuf), "%ld-%ld", low, high);

        // FIXED: format level into a 2‑wide field
        char lvlBuf[4];
        snprintf(lvlBuf, sizeof(lvlBuf), "%2d", lvl);

        // Print aligned columns
        p.client.print(lvlBuf);
        p.client.print("  ");
        p.client.print(titleBuf);
        p.client.println(rangeBuf);
    }

    p.client.println("=============================");
}


void cmdHelp(Player &p) {
    p.client.println("===== Help: Main Commands =====");

    // --- Core navigation & interaction ---
    p.client.println("(l)ook                 - Examine your current room");
    p.client.println("(i)nventory            - Show what you are carrying");
    p.client.println("get <item>             - Pick up an item");
    p.client.println("drop <item>            - Drop an item");
    p.client.println("drop all               - Drop everything");
    p.client.println("wear <item>            - Wear armor");
    p.client.println("remove <item>          - Remove worn armor");
    p.client.println("wield <item>           - Wield a weapon");
    p.client.println("unwield                - Stop wielding your weapon");
    p.client.println("examine <item>         - Examine/Search an item");
    p.client.println("search <item>          - Examine/Search an item");

    // --- Combat ---
    p.client.println("kill <target>          - Attack a player or NPC");
    p.client.println("wimp                   - Toggle auto-flee when weak");

    // --- Character info & progression ---
    p.client.println("(sc)ore                - Show your stats");
    p.client.println("levels                 - Show level titles and XP ranges");
    p.client.println("advance                - Advance to the next level (if eligible)");
    p.client.println("questlist              - Show available quests and completion status");

    // --- Social ---
    p.client.println("say <msg>              - Speak in your room");
    p.client.println("shout <msg>            - Broadcast to all players");
    p.client.println("tell <player> <msg>    - Send a private message to a player");
    p.client.println("actions                - List available emotes");

    // --- World navigation ---
    p.client.println("map                    - Toggle the Mapper Utility on or off");
    p.client.println("townmap                - Town Map of Espertheru");

    // --- Account / system ---
    p.client.println("password               - Change your password");
    p.client.println("quit                   - Save and disconnect");

    // --- Wizard section ---
    if (p.IsWizard) {
        p.client.println("wizhelp                - Wizard commands");
    }

    p.client.println("===============================");
}

/**
 * Generate and display a QR code from a message
 * Usage: qrcode [message]
 */
void cmdQrCode(Player &p, const String &input) {
    String args = input;
    args.trim();
    
    if (args.length() == 0) {
        p.client.println("Usage: qrcode [message] or qrcode [playername] [message]");
        p.client.println("Example: qrcode hello world");
        p.client.println("Example: qrcode Atew meet me at coordinates 100,200,50");
        return;
    }
    
    // Parse potential player name
    String firstWord = "";
    String message = args;
    
    int spacePos = args.indexOf(' ');
    if (spacePos > 0) {
        firstWord = args.substring(0, spacePos);
        message = args.substring(spacePos + 1);
        message.trim();
    } else {
        // Only one word - treat it as message
        firstWord = "";
        message = args;
    }
    
    // Try to find player with firstWord as name
    Player* targetPlayer = nullptr;
    if (firstWord.length() > 0) {
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (players[i].active && players[i].loggedIn &&
                strcasecmp(players[i].name, firstWord.c_str()) == 0) {
                targetPlayer = &players[i];
                break;
            }
        }
    }
    
    // If no valid message (single word only), treat it as message
    if (targetPlayer == nullptr && spacePos < 0) {
        message = args;
        firstWord = "";
    }
    
    // Limit message length to avoid excessive QR codes
    if (message.length() > 200) {
        message = message.substring(0, 200);
    }
    
    // Create QR code with higher version for longer text
    QRCode qrcode;
    uint8_t qrcodedata[qrcode_getBufferSize(5)];  // Version 5 allows for larger data
    qrcode_initText(&qrcode, qrcodedata, 5, ECC_LOW, message.c_str());
    
    // Function to display QR code to a client
    auto displayQrCode = [&](Client &client) {
        client.println("");
        client.println("QR Code for: " + message);
        client.println("");
        
        // Print QR code using block characters - scale to fit in 100 char width
        // Each module is 2 chars wide (██ or  ) for aspect ratio
        for (uint8_t y = 0; y < qrcode.size; y++) {
            String line = "";
            for (uint8_t x = 0; x < qrcode.size; x++) {
                if (qrcode_getModule(&qrcode, x, y)) {
                    line += "██";  // Black block
                } else {
                    line += "  ";  // White block
                }
            }
            // Trim if line is too long, otherwise print as-is
            if (line.length() > MAX_QRCODE_WIDTH * 2) {
                line = line.substring(0, MAX_QRCODE_WIDTH * 2);
            }
            client.println(line);
        }
        client.println("");
    };
    
    // Send QR code
    if (targetPlayer != nullptr) {
        // Send to target player
        p.client.println("You sent " + String(targetPlayer->name) + " the qrcode for: " + message);
        targetPlayer->client.println("");
        targetPlayer->client.println(capFirst(p.name) + " sent you a qrcode! Maybe your phone can read it...");
        targetPlayer->client.println("");
        
        // Display QR code to target player
        for (uint8_t y = 0; y < qrcode.size; y++) {
            String line = "";
            for (uint8_t x = 0; x < qrcode.size; x++) {
                if (qrcode_getModule(&qrcode, x, y)) {
                    line += "██";
                } else {
                    line += "  ";
                }
            }
            if (line.length() > MAX_QRCODE_WIDTH * 2) {
                line = line.substring(0, MAX_QRCODE_WIDTH * 2);
            }
            targetPlayer->client.println(line);
        }
        targetPlayer->client.println("");
    } else {
        // Display locally
        displayQrCode(p.client);
    }
}

// =============================
// Map display command
// =============================

/**
 * Generate a 3x3 character block for a room
 * Represents all 10 directions (8 cardinal/diagonal + up/down)
 * Center is filled circle (●) if only 1 of the 8 directions (dead end), else filled square (█)
 * Up/Down arrows do NOT count toward dead end detection
 * If isPlayerHere is true, center shows @ instead of █/●
 * Returns a 3-line string representing the icon
 */
String getMapBlock(int exit_n, int exit_s, int exit_e, int exit_w,
                   int exit_ne, int exit_nw, int exit_se, int exit_sw,
                   int exit_u, int exit_d, bool isPlayerHere = false, char locationCode = 0) {
    // 3x3 grid for each voxel
    // Using 0 for empty, 1 for filled square, 2 for up arrow, 3 for down arrow, 4 for circle, 5 for player, 6 for location code
    int grid[3][3] = {
        {0, 0, 0},
        {0, 0, 0},
        {0, 0, 0}
    };
    
    // No room at all - return empty 3x3
    if (!exit_n && !exit_s && !exit_e && !exit_w &&
        !exit_ne && !exit_nw && !exit_se && !exit_sw &&
        !exit_u && !exit_d) {
        return "   \n   \n   ";
    }
    
    // Count only the 8 cardinal/diagonal directions (not up/down)
    int directionCount = exit_n + exit_s + exit_e + exit_w +
                        exit_ne + exit_nw + exit_se + exit_sw;
    
    // Center: location code if provided, else filled circle (●) if dead end, else filled square (█)
    // Note: Player position will be drawn LAST to always appear on top
    if (locationCode != 0) {
        grid[1][1] = 6;  // Location code
    } else {
        grid[1][1] = (directionCount == 1) ? 4 : 1;
    }
    
    // Cardinal directions
    if (exit_n) grid[0][1] = 1;   // North - top center
    if (exit_s) grid[2][1] = 1;   // South - bottom center
    if (exit_e) grid[1][2] = 1;   // East - right center
    if (exit_w) grid[1][0] = 1;   // West - left center
    
    // Diagonal directions
    if (exit_ne) grid[0][2] = 1;  // NorthEast - top right
    if (exit_nw) grid[0][0] = 1;  // NorthWest - top left
    if (exit_se) grid[2][2] = 1;  // SouthEast - bottom right
    if (exit_sw) grid[2][0] = 1;  // SouthWest - bottom left
    
    // Vertical directions (overwrite left/right middle with arrows)
    if (exit_u) grid[1][0] = 2;   // Up arrow - left middle
    if (exit_d) grid[1][2] = 3;   // Down arrow - right middle
    
    // Player position is drawn LAST to ensure it always appears on top of everything
    if (isPlayerHere) {
        grid[1][1] = 5;  // Player icon (X) - overrides location code and center
    }
    
    // Build the 3-line string
    String result = "";
    for (int row = 0; row < 3; row++) {
        for (int col = 0; col < 3; col++) {
            if (grid[row][col] == 0) {
                result += " ";
            } else if (grid[row][col] == 1) {
                result += "█";
            } else if (grid[row][col] == 2) {
                result += "↑";
            } else if (grid[row][col] == 3) {
                result += "↓";
            } else if (grid[row][col] == 4) {
                result += "●";
            } else if (grid[row][col] == 5) {
                result += "X";
            } else if (grid[row][col] == 6) {
                result += locationCode;
            }
        }
        if (row < 2) result += "\n";
    }
    
    return result;
}

char getLocationCode(int x, int y) {
    // Map coordinates to location codes
    if (x == 250 && y == 205) return 'C';  // Church
    if (x == 249 && y == 248) return 'E';  // Esperthertu Inn
    if (x == 246 && y == 246) return 'D';  // Doctor's Office
    if (x == 246 && y == 244) return 'L';  // Lawyer's Office
    if (x == 248 && y == 242) return 'W';  // Weather Station
    if (x == 249 && y == 242) return 'A';  // The Hammered Anvil (Tavern)
    if (x == 251 && y == 242) return 'R';  // Recycling Center
    if (x == 252 && y == 242) return 'P';  // Police Station
    if (x == 254 && y == 244) return 'B';  // Blacksmith
    if (x == 254 && y == 245) return '$';  // Bank
    if (x == 254 && y == 246) return 'O';  // Administration Offices
    if (x == 254 && y == 247) return 'M';  // Magic Shop
    if (x == 252 && y == 248) return 'p';  // Post Office
    if (x == 251 && y == 248) return 'S';  // Esperthertu Shop
    return 0;  // No location code
}

// Draw the player's visited map (20x20 grid centered on player)
// Called by both the map tracker display and the cmdMap toggle
void drawPlayerMap(Player &p) {
    const int GRID_RADIUS = 10;  // 10 voxels in each direction from player
    int TARGET_Z = p.roomZ;       // Use player's current Z level

    // Read all rooms on this Z level
    File f = LittleFS.open("/rooms.txt", "r");
    if (!f) {
        return;
    }

    std::map<uint64_t, String> roomMap;  // key = packVoxelKey(x,y,z), value = room line

    if (f.available()) f.readStringUntil('\n');  // Skip header

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        // Parse CSV: x,y,z,...
        int c1 = line.indexOf(',');
        int c2 = line.indexOf(',', c1 + 1);
        int c3 = line.indexOf(',', c2 + 1);

        if (c1 < 0 || c2 < 0 || c3 < 0) continue;

        int x = line.substring(0, c1).toInt();
        int y = line.substring(c1 + 1, c2).toInt();
        int z = line.substring(c2 + 1, c3).toInt();

        if (z == TARGET_Z) {
            uint64_t key = packVoxelKey(x, y, z);
            roomMap[key] = line;
        }
    }
    f.close();

    // Create a helper to parse room data from a line
    auto parseRoomData = [](const String &line) -> std::pair<bool, std::array<int, 10>> {
        int c1 = line.indexOf(',');
        int c2 = line.indexOf(',', c1 + 1);
        int c3 = line.indexOf(',', c2 + 1);
        
        if (c1 < 0 || c2 < 0 || c3 < 0) {
            return {false, {}};
        }

        auto getField = [&](int fieldIndex) -> int {
            int count = 0;
            int start = 0;
            for (int i = 0; i <= line.length(); i++) {
                if (i == line.length() || line[i] == ',') {
                    if (count == fieldIndex) {
                        return line.substring(start, i).toInt();
                    }
                    start = i + 1;
                    count++;
                }
            }
            return 0;
        };

        std::array<int, 10> exits = {
            getField(5),   // exit_n
            getField(6),   // exit_s
            getField(7),   // exit_e
            getField(8),   // exit_w
            getField(9),   // exit_ne
            getField(10),  // exit_nw
            getField(11),  // exit_se
            getField(12),  // exit_sw
            getField(13),  // exit_u
            getField(14)   // exit_d
        };
        
        return {true, exits};
    };

    // Build the 20x20 grid centered on player
    // Create a set of visited voxels for fast lookup
    std::set<uint64_t> visitedSet;
    for (const auto &voxel : p.visitedVoxels) {
        visitedSet.insert(packVoxelKey(voxel.x, voxel.y, voxel.z));
    }

    // Display the map without header/footer (just the grid)
    for (int dy = -GRID_RADIUS; dy <= GRID_RADIUS; dy++) {
        // Build 3 output lines for this row of voxels
        String outLines[3] = {"", "", ""};
        
        for (int dx = -GRID_RADIUS; dx <= GRID_RADIUS; dx++) {
            int worldX = p.roomX + dx;
            int worldY = p.roomY + dy;
            uint64_t key = packVoxelKey(worldX, worldY, TARGET_Z);
            
            String block;
            
            // Check if this voxel has been visited
            if (visitedSet.count(key)) {
                // Voxel visited - show its exits
                const String &roomLine = roomMap[key];
                auto result = parseRoomData(roomLine);
                bool exists = result.first;
                std::array<int, 10> exits = result.second;
                
                if (exists) {
                    bool isPlayerHere = (dx == 0 && dy == 0);
                    block = getMapBlock(exits[0], exits[1], exits[2], exits[3],
                                       exits[4], exits[5], exits[6], exits[7],
                                       exits[8], exits[9], isPlayerHere);
                } else {
                    block = "   \n   \n   ";
                }
            } else {
                // Unvisited voxel - show empty space
                block = "   \n   \n   ";
            }
            
            // Split block into 3 lines and append to output lines
            int lineIdx = 0;
            int startPos = 0;
            for (int i = 0; i <= block.length() && lineIdx < 3; i++) {
                if (i == block.length() || block[i] == '\n') {
                    outLines[lineIdx] += block.substring(startPos, i);
                    startPos = i + 1;
                    lineIdx++;
                }
            }
        }
        
        // Print the 3 lines for this row, skipping blank lines
        for (int i = 0; i < 3; i++) {
            // Check if line has any non-whitespace characters
            bool hasContent = false;
            for (int j = 0; j < outLines[i].length(); j++) {
                if (outLines[i][j] != ' ') {
                    hasContent = true;
                    break;
                }
            }
            // Only print if line has content
            if (hasContent) {
                p.client.println(outLines[i]);
            }
        }
    }
}

void cmdMap(Player &p) {
    // Toggle map tracker on/off
    p.mapTrackerEnabled = !p.mapTrackerEnabled;
    
    if (p.mapTrackerEnabled) {
        p.client.println("Map tracker is on");
    } else {
        p.client.println("Map tracker is off");
    }
}

void cmdTownMap(Player &p) {
    const int TARGET_Z = 50;
    const int TOWN_MIN_X = 246;
    const int TOWN_MIN_Y = 242;
    const int TOWN_MAX_X = 254;
    const int TOWN_MAX_Y = 251;
    
    // Scan all rooms to find those in the town area
    File f = LittleFS.open("/rooms.txt", "r");
    if (!f) {
        p.client.println("Cannot open rooms file.");
        return;
    }

    std::vector<String> townLines;

    if (f.available()) f.readStringUntil('\n');  // Skip header

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0) continue;

        // Parse CSV: x,y,z,...
        int c1 = line.indexOf(',');
        int c2 = line.indexOf(',', c1 + 1);
        int c3 = line.indexOf(',', c2 + 1);

        if (c1 < 0 || c2 < 0 || c3 < 0) continue;

        int x = line.substring(0, c1).toInt();
        int y = line.substring(c1 + 1, c2).toInt();
        int z = line.substring(c2 + 1, c3).toInt();

        // Filter for town area at Z=50
        if (z == TARGET_Z && x >= TOWN_MIN_X && x <= TOWN_MAX_X &&
            y >= TOWN_MIN_Y && y <= TOWN_MAX_Y) {
            townLines.push_back(line);
        }
    }
    f.close();

    if (townLines.empty()) {
        p.client.println("No town rooms found.");
        return;
    }

    // Calculate grid dimensions (town is fixed: 9x10)
    int gridWidth = TOWN_MAX_X - TOWN_MIN_X + 1;   // 9 voxels
    int gridHeight = TOWN_MAX_Y - TOWN_MIN_Y + 1;  // 10 voxels

    // Create a 2D grid to store room data
    struct RoomData {
        bool exists = false;
        int exit_n = 0, exit_s = 0, exit_e = 0, exit_w = 0;
        int exit_ne = 0, exit_nw = 0, exit_se = 0, exit_sw = 0;
        int exit_u = 0, exit_d = 0;
    };

    std::vector<std::vector<RoomData>> grid(gridHeight, std::vector<RoomData>(gridWidth));

    // Populate grid with room exit data
    for (const String &line : townLines) {
        int c1 = line.indexOf(',');
        int c2 = line.indexOf(',', c1 + 1);
        int c3 = line.indexOf(',', c2 + 1);

        int x = line.substring(0, c1).toInt();
        int y = line.substring(c1 + 1, c2).toInt();

        int gridX = x - TOWN_MIN_X;
        int gridY = y - TOWN_MIN_Y;

        auto getField = [&](int fieldIndex) -> int {
            int count = 0;
            int start = 0;
            for (int i = 0; i <= line.length(); i++) {
                if (i == line.length() || line[i] == ',') {
                    if (count == fieldIndex) {
                        return line.substring(start, i).toInt();
                    }
                    start = i + 1;
                    count++;
                }
            }
            return 0;
        };

        RoomData &room = grid[gridY][gridX];
        room.exists = true;
        room.exit_n = getField(5);
        room.exit_s = getField(6);
        room.exit_e = getField(7);
        room.exit_w = getField(8);
        room.exit_ne = getField(9);
        room.exit_nw = getField(10);
        room.exit_se = getField(11);
        room.exit_sw = getField(12);
        room.exit_u = getField(13);
        room.exit_d = getField(14);
    }

    // Display the town map
    p.client.println("");
    p.client.println("═══════════════════════════════════════════════════════════════════");
    p.client.println("                     The Town of Espertheru");
    p.client.println("═══════════════════════════════════════════════════════════════════");
    p.client.println("");

    // Legend data - 15 items total (14 locations + YOU ARE HERE)
    const char* legendCodes[] = {"C", "E", "D", "L", "W", "A", "R", "P", "B", "$", "O", "M", "p", "S", "X"};
    const char* legendDescs[] = {
        "Church",
        "Esperthertu Inn",
        "Doctor's Office",
        "Lawyer's Office",
        "Weather Station",
        "The Hammered Anvil",
        "Recycling Center",
        "Police Station",
        "Blacksmith",
        "Bank",
        "Administration Offices",
        "Magic Shop",
        "Post Office",
        "Esperthertu Shop",
        "YOU ARE HERE"
    };
    const int legendCount = 15;

    // Print each row of voxels - each voxel becomes a 3-character wide, 3-line tall block
    // Map has 10 rows (gridHeight), 3 output lines per map row = 30 lines total
    // Legend has 15 items, so: 1 "Legend:" + 9 items fit in first 9 map rows + 1 item in row 10 = 11 items shown
    // Then remaining 4 items (items 11-14) print below map
    int legendIdx = 0;
    
    for (int y = 0; y < gridHeight; y++) {
        // Build 3 output lines for this row of voxels
        String outLines[3] = {"", "", ""};
        
        for (int x = 0; x < gridWidth; x++) {
            // Calculate actual world coordinates
            int worldX = TOWN_MIN_X + x;
            int worldY = TOWN_MIN_Y + y;
            
            const RoomData &room = grid[y][x];
            String block;
            
            if (room.exists) {
                // Check if player is at this location
                bool isPlayerHere = (p.roomX == worldX && p.roomY == worldY && p.roomZ == TARGET_Z);
                char locCode = getLocationCode(worldX, worldY);
                block = getMapBlock(room.exit_n, room.exit_s, room.exit_e, room.exit_w,
                                   room.exit_ne, room.exit_nw, room.exit_se, room.exit_sw,
                                   room.exit_u, room.exit_d, isPlayerHere, locCode);
            } else {
                block = "   \n   \n   ";
            }
            
            // Split block into 3 lines and append to output lines
            int lineIdx = 0;
            int startPos = 0;
            for (int i = 0; i <= block.length() && lineIdx < 3; i++) {
                if (i == block.length() || block[i] == '\n') {
                    outLines[lineIdx] += block.substring(startPos, i);
                    startPos = i + 1;
                    lineIdx++;
                }
            }
        }
        
        // For each of the 3 output lines in this voxel row, add corresponding legend item
        // Helper function to calculate visual width of a string (counting UTF-8 characters, not bytes)
        auto visualWidth = [](const String &s) -> int {
            int width = 0;
            for (size_t i = 0; i < s.length(); i++) {
                unsigned char c = s[i];
                // Count only non-continuation bytes (leading bytes of UTF-8 sequences)
                // Continuation bytes start with 10xxxxxx (0x80-0xBF)
                if ((c & 0xC0) != 0x80) {
                    width++;
                }
            }
            return width;
        };

        for (int i = 0; i < 3; i++) {
            String mapLine = outLines[i];
            
            // Calculate absolute output line number
            int outputLineNum = y * 3 + i;
            String legendCode = "";
            String legendDesc = "";
            
            // Build legend based on output line number
            if (outputLineNum == 2) {
                legendCode = "----------Legend:----------";
            } else if (outputLineNum == 3) {
                legendCode = "";  // Empty spacing line
            } else if (outputLineNum >= 4 && outputLineNum <= 17) {
                int itemIdx = outputLineNum - 4;
                legendCode = String(legendCodes[itemIdx]);
                legendDesc = String(legendDescs[itemIdx]);
            } else if (outputLineNum == 18) {
                legendCode = "";  // Empty spacing line
            } else if (outputLineNum == 19) {
                legendCode = String(legendCodes[14]);
                legendDesc = String(legendDescs[14]);
            }
            
            if (outputLineNum >= 2 && outputLineNum <= 19) {
                String outputLine = mapLine;
                
                // Pad to exactly 31 visual characters (position 32 is where legend starts)
                // Use visualWidth instead of length() to account for multi-byte UTF-8 characters
                while (visualWidth(outputLine) < 31) {
                    outputLine += " ";
                }
                
                // Add legend starting at position 32
                if (outputLineNum == 2) {
                    // Full header line
                    outputLine += legendCode;
                } else if (outputLineNum == 3 || outputLineNum == 18) {
                    // Empty spacing lines - nothing added
                } else if (legendCode.length() > 0) {
                    // Regular legend item: code (C:, E:, etc) + description
                    outputLine += legendCode + ":  " + legendDesc;
                }
                
                p.client.println(outputLine);
            } else {
                p.client.println(mapLine);
            }
        }
    }


    p.client.println("");
    p.client.println("═══════════════════════════════════════════════════════════════════");
}


void cmdWizHelp(Player &p) {
    if (!p.IsWizard) {
        p.client.println("What?");
        return;
    }

    p.client.println("===== Wizard Commands =====");

    // ---------------------------------------------------------
    // DEBUG SYSTEM (alphabetical)
    // ---------------------------------------------------------
    p.client.println("debug delete <file>     - Delete a LittleFS file");
    p.client.println("debug destination       - Toggle debug output");
    p.client.println("debug extract <file>    - Backup a single file");
    p.client.println("debug extractall        - Backup all LittleFS files");
    p.client.println("debug files             - Dump core data files");
    p.client.println("debug flashspace        - Show LittleFS usage");
    p.client.println("debug items             - Dump all world items");
    p.client.println("debug list              - List LittleFS files");
    p.client.println("debug npcs              - Dump NPC definitions");
    p.client.println("debug online            - List connected players with stats");
    p.client.println("debug players           - Dump all player saves");
    p.client.println("debug questflags        - Show quest flags");
    p.client.println("debug sessions          - Show last 50 session log records");
    p.client.println("debug ymodem            - YMODEM transfer log");
    p.client.println("debug <player>          - Dump a single player");

    // ---------------------------------------------------------
    // QUEST SYSTEM (alphabetical)
    // ---------------------------------------------------------
    p.client.println("questinfo <id>          - Show quest definition");

    // ---------------------------------------------------------
    // WIZARD UTILITIES (alphabetical)
    // ---------------------------------------------------------
    p.client.println("clone                   - Clone an item or NPC to your room");
    p.client.println("clonegold <amount>      - Spawn gold coins to your room");
    p.client.println("goto <x,y,z|player>     - Teleport instantly");
    p.client.println("heal <player>           - Fully heal a player");
    p.client.println("invis                   - Toggle invisibility");
    p.client.println("reboot                  - Restart the world");
    p.client.println("resetworlditems         - Reset all world item spawns");
    p.client.println("stats                   - Toggle wizard stats");

    // ---------------------------------------------------------
    // WIZARD FLAVOR (alphabetical)
    // ---------------------------------------------------------
    p.client.println("entermsg <text>         - Set custom arrival msg");
    p.client.println("exitmsg <text>          - Set custom departure msg");

    p.client.println("===============================");
}




// =============================
// Emotes and social commands
// =============================

void cmdActions(Player &p) {
  p.client.println("Available actions:");
  int perLine = 10;
  for (int i = 0; i < emoteCount; i++) {
    p.client.print(emotes[i].base);
    if (i < emoteCount - 1) p.client.print(", ");
    if ((i + 1) % perLine == 0) p.client.println();
  }
  p.client.println();
}

int findEmote(const String &cmd) {
    for (int i = 0; i < (int)(sizeof(emotes)/sizeof(emotes[0])); i++) {
        if (cmd == emotes[i].base) return i;
    }
    return -1;
}

void executeEmote(Player &p, int index, const String &cmd, const String &args) {
    int e = findEmote(cmd);
    if (e < 0) return;

    Emote &E = emotes[e];

    // Parse target (strip optional "at" / "to")
    String t = args;
    t.trim();
    if (t.startsWith("at "))  t = t.substring(3);
    else if (t.startsWith("to "))  t = t.substring(3);
    t.trim();

    // --- NO TARGET ---
    if (t.length() == 0) {
        // You
        p.client.println("You " + String(E.base) + " " + E.adverb + ".");

        // Others
        announceToRoom(
            p.roomX, p.roomY, p.roomZ,
            capFirst(p.name) + " " + E.verb + " " + E.adverb + ".",
            index
        );
        return;
    }

    // --- FIND TARGET PLAYER IN ROOM ---
    int targetIndex = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!players[i].active || !players[i].loggedIn) continue;
        if (players[i].roomX != p.roomX ||
            players[i].roomY != p.roomY ||
            players[i].roomZ != p.roomZ) continue;

        if (!strcasecmp(players[i].name, t.c_str())) {
            targetIndex = i;
            break;
        }
    }

    if (targetIndex < 0) {
        p.client.println("They aren't here.");
        return;
    }

    Player &T = players[targetIndex];

    // --- NEW: invisibility rule ---
    if (T.IsInvisible && !p.IsWizard) {
        p.client.println("You don't see them here.");
        return;
    }

    // --- SELF TARGET ---
    if (targetIndex == index) {
        if (E.needsPreposition)
            p.client.println("You " + String(E.base) + " " + E.preposition + " yourself " + E.adverb + ".");
        else
            p.client.println("You " + String(E.base) + " yourself " + E.adverb + ".");
        return;
    }

    // --- NORMAL TARGETED EMOTE ---

    // You → target
    if (E.needsPreposition)
        p.client.println("You " + String(E.base) + " " + E.preposition + " " + capFirst(T.name) + " " + E.adverb + ".");
    else
        p.client.println("You " + String(E.base) + " " + capFirst(T.name) + " " + E.adverb + ".");

    // Target sees
    if (E.needsPreposition)
        T.client.println(capFirst(p.name) + " " + E.verb + " " + E.preposition + " you " + E.adverb + ".");
    else
        T.client.println(capFirst(p.name) + " " + E.verb + " you " + E.adverb + ".");

    // Bystanders see
    if (E.needsPreposition)
        announceToRoomExcept(
            p.roomX, p.roomY, p.roomZ,
            capFirst(p.name) + " " + E.verb + " " + E.preposition + " " + capFirst(T.name) + " " + E.adverb + ".",
            index, targetIndex
        );
    else
        announceToRoomExcept(
            p.roomX, p.roomY, p.roomZ,
            capFirst(p.name) + " " + E.verb + " " + capFirst(T.name) + " " + E.adverb + ".",
            index, targetIndex
        );
}


void cmdSay(Player &p, const char *message) {
    if (!message || strlen(message) == 0) {
        p.client.println("Say what?");
        return;
    }

    p.client.print("You say: ");
    p.client.println(message);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!players[i].active) continue;
        if (&players[i] == &p) continue;
        if (players[i].roomX == p.roomX &&
            players[i].roomY == p.roomY &&
            players[i].roomZ == p.roomZ) {

            players[i].client.print(capFirst(p.name));
            players[i].client.print(" says: ");
            players[i].client.println(message);
        }
    }

    // ⭐ QUEST HOOK — say quests
    onQuestEvent(p, "say", "", "", String(message), p.roomX, p.roomY, p.roomZ);
}


void cmdShout(Player &p, const char *message) {
  if (!message || strlen(message) == 0) {
    p.client.println("Shout what?");
    return;
  }

  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (!players[i].active) continue;
    players[i].client.print(capFirst(p.name));
    players[i].client.print(" shouts: ");
    players[i].client.println(message);
  }
}


void cmdTell(Player &p, const String &targetName, const String &message) {
    if (targetName.length() == 0) {
        p.client.println("Usage: tell [player name] [message]");
        p.client.println("   or: tell [player name] qrcode [message]");
        return;
    }

    if (message.length() == 0) {
        p.client.println("Usage: tell [player name] [message]");
        p.client.println("   or: tell [player name] qrcode [message]");
        return;
    }

    // Find the target player
    Player *target = nullptr;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!players[i].active || !players[i].loggedIn) continue;
        if (players[i].name[0] == '\0') continue;
        if (strcasecmp(players[i].name, targetName.c_str()) == 0) {
            target = &players[i];
            break;
        }
    }

    if (!target) {
        p.client.println("That player is not online.");
        return;
    }

    if (target == &p) {
        p.client.println("You can't tell yourself.");
        return;
    }

    // Check if message starts with "qrcode"
    String msgLower = message;
    msgLower.toLowerCase();
    
    if (msgLower.startsWith("qrcode ")) {
        // Extract the QR code message (everything after "qrcode ")
        String qrcodeMsg = message.substring(7);  // 7 = length of "qrcode "
        qrcodeMsg.trim();
        
        if (qrcodeMsg.length() == 0) {
            p.client.println("qrcode: no message specified");
            return;
        }
        
        // Limit message length
        if (qrcodeMsg.length() > 200) {
            qrcodeMsg = qrcodeMsg.substring(0, 200);
        }
        
        // Notify sender
        p.client.println("You send a QR code to " + capFirst(target->name) + ": " + qrcodeMsg);
        
        // Generate and send QR code to target
        target->client.println("");
        target->client.println(capFirst(p.name) + " sent you a QR code:");
        target->client.println("");
        
        // Create QR code
        QRCode qrcode;
        uint8_t qrcodedata[qrcode_getBufferSize(5)];
        qrcode_initText(&qrcode, qrcodedata, 5, ECC_LOW, qrcodeMsg.c_str());
        
        // Print QR code using block characters
        for (uint8_t y = 0; y < qrcode.size; y++) {
            String line = "";
            for (uint8_t x = 0; x < qrcode.size; x++) {
                if (qrcode_getModule(&qrcode, x, y)) {
                    line += "██";  // Black block
                } else {
                    line += "  ";  // White block
                }
            }
            // Trim if line is too long
            if (line.length() > MAX_QRCODE_WIDTH * 2) {
                line = line.substring(0, MAX_QRCODE_WIDTH * 2);
            }
            target->client.println(line);
        }
        target->client.println("");
    } else {
        // Regular tell message
        // Send to sender
        p.client.print("You tell ");
        p.client.print(capFirst(target->name));
        p.client.print(": ");
        p.client.println(message);

        // Send to target (only the target sees this)
        target->client.print(capFirst(p.name));
        target->client.print(" tells you: ");
        target->client.println(message);
    }
}


void cmdPassword(Player &p, int index) {
    // 1) Ask for old password
    p.client.println("Enter your current password:");
    while (!p.client.available()) delay(1);
    String oldpw = cleanInput(p.client.readStringUntil('\n'));

    if (oldpw != String(p.storedPassword)) {
        p.client.println("Incorrect password. Cancelled.");
        return;
    }

    // 2) Ask for new password
    p.client.println("Enter new password:");
    while (!p.client.available()) delay(1);
    String newpw = cleanInput(p.client.readStringUntil('\n'));

    if (!isValidPassword(newpw)) {
        p.client.println("Invalid password format. Cancelled.");
        return;
    }

    // 3) Confirm new password
    p.client.println("Confirm new password:");
    while (!p.client.available()) delay(1);
    String confirm = cleanInput(p.client.readStringUntil('\n'));

    if (confirm != newpw) {
        p.client.println("Passwords do not match. Cancelled.");
        return;
    }

    // 4) Save
    strncpy(p.storedPassword, newpw.c_str(), sizeof(p.storedPassword)-1);
    savePlayerToFS(p);

    p.client.println("Password updated successfully.");
}



void cmdKill(Player &p, const char* arg) {
    String t = arg;
    t.trim();

    if (t.length() == 0) {
        p.client.println("Kill what?");
        return;
    }

    NpcInstance* npc = findNPCInRoom(p, t);
    if (!npc) {
        p.client.println("You don't see that here.");
        return;
    }

    // String -> std::string for npcDefs key
    std::string key = std::string(npc->npcId.c_str());
    auto it = npcDefs.find(key);
    if (it == npcDefs.end()) {
        p.client.println("That creature seems undefined in the world.");
        return;
    }

    NpcDefinition &def = it->second;
    String npcName = def.attributes.at("name").c_str();

    // Resolve playerIndex
    int playerIndex = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (&players[i] == &p) {
            playerIndex = i;
            break;
        }
    }
    if (playerIndex == -1) return;

    // Mark hostility and target
    npc->hostileTo[playerIndex] = true;
    npc->targetPlayer = playerIndex;

    // Start combat
    p.inCombat = true;
    p.combatTarget = npc;
    p.nextCombatTime = millis() + 1000;  // 1 second delay before first hit

    p.client.println("You engage the " + npcName + " in combat!");
    broadcastRoomExcept(p, capFirst(p.name) + " attacks the " + npcName + "!", p);
}


void autoWimpFlee(Player &p) {

    // Find player's index in players[]
    int index = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (&players[i] == &p) {
            index = i;
            break;
        }
    }
    if (index < 0) return; // should never happen

    Room &r = p.currentRoom;

    // Collect valid exits
    std::vector<String> exits;

    if (r.exit_n) exits.push_back("north");
    if (r.exit_s) exits.push_back("south");
    if (r.exit_e) exits.push_back("east");
    if (r.exit_w) exits.push_back("west");
    if (r.exit_u) exits.push_back("up");
    if (r.exit_d) exits.push_back("down");
    if (r.exit_ne) exits.push_back("northeast");
    if (r.exit_nw) exits.push_back("northwest");
    if (r.exit_se) exits.push_back("southeast");
    if (r.exit_sw) exits.push_back("southwest");

    if (exits.empty()) return; // nowhere to flee

    // Pick random direction
    String dir = exits[random(exits.size())];

    p.client.println("You flee " + dir + "!");

    // Use your existing movement system
    movePlayer(p, index, dir.c_str());

    // End combat
    p.inCombat = false;
    p.combatTarget = nullptr;
}





// ***************THE ACTUAL COMBAT ENGINE********************

void doCombatRound(Player &p) {

    int playerIndex = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (&players[i] == &p) {
            playerIndex = i;
            break;
        }
    }
    if (playerIndex == -1) return;

    if (!p.inCombat || p.combatTarget == nullptr) return;

    NpcInstance* npc = p.combatTarget;

    // NPC already dead or gone
    if (!npc->alive || npc->hp <= 0) {
        p.inCombat = false;
        p.combatTarget = nullptr;
        return;
    }

    // NPC left the room
    if (npc->x != p.roomX || npc->y != p.roomY || npc->z != p.roomZ) {
        p.inCombat = false;
        p.combatTarget = nullptr;
        return;
    }

    // Hostility check (end AFTER round)
    bool lostHostility = !npc->hostileTo[playerIndex];

    // SAFE lookup
    std::string npcKey = std::string(npc->npcId.c_str());
    auto it = npcDefs.find(npcKey);
    if (it == npcDefs.end()) {
        p.client.println("Internal error: NPC definition missing.");
        p.inCombat = false;
        p.combatTarget = nullptr;
        return;
    }

    NpcDefinition &def = it->second;

    // NPC name
    String npcName = def.attributes.count("name")
                     ? String(def.attributes.at("name").c_str())
                     : String("creature");

    Serial.print("DEBUG: NPC HP = ");
    Serial.println(npc->hp);

    auto rollToHit = [&](int atk, int defv) {
        int roll = random(1, 21);
        return (roll + atk) >= defv;
    };

    // Defense
    int npcDefense = 5;
    auto defIt = def.attributes.find("defense");
    if (defIt != def.attributes.end())
        npcDefense = atoi(defIt->second.c_str());

    // Attack
    int npcBaseDmg = 1;
    auto atkIt = def.attributes.find("attack");
    if (atkIt != def.attributes.end())
        npcBaseDmg = atoi(atkIt->second.c_str());

    int playerTotalAtk = p.attack + p.weaponBonus;
    int playerToHitBonus = (p.level - 1) * 2;

    // ---------------------------------------------------------
    // PLAYER ATTACKS NPC
    // ---------------------------------------------------------
    if (rollToHit(playerTotalAtk + playerToHitBonus, npcDefense)) {

        int playerDmg = random(1, playerTotalAtk + 1);
        npc->hp -= playerDmg;

        String verb = combatVerbs[random(7)];

        p.client.println("You hit " + npcName);

        if (p.IsWizard && p.showStats) {
            int armorTotal = p.baseDefense + p.armorBonus;
            int dmgTotal   = p.attack + p.weaponBonus;

            p.client.println("  (damage=" + String(playerDmg) + ")");
            p.client.println("    armor:  base(" + String(p.baseDefense) +
                             ") + bonus(" + String(p.armorBonus) +
                             ") = " + String(armorTotal));
            p.client.println("    damage: base(" + String(p.attack) +
                             ") + bonus(" + String(p.weaponBonus) +
                             ") = " + String(dmgTotal));
        }

        broadcastRoomExcept(
            p,
            capFirst(p.name) + " " + verb + "s the " + npcName + "!",
            p
        );

        // ---------------------------------------------------------
        // NPC DIES
        // ---------------------------------------------------------
        if (npc->hp <= 0) {

            npc->hp    = 0;
            npc->alive = false;

            String deathMsg = npcDeathMsgs[random(6)];
            p.client.println(npcName + " dies.");
            broadcastRoomExcept(
                p,
                "The " + npcName + " " + deathMsg,
                p
            );

            // XP
            int xpReward = 5;
            auto xpIt = def.attributes.find("xp");
            if (xpIt != def.attributes.end())
                xpReward = atoi(xpIt->second.c_str());

            p.xp += xpReward;

            // Gold drop
            int dropGold = npc->gold;
            auto goldIt = def.attributes.find("gold");
            if (dropGold <= 0 && goldIt != def.attributes.end())
                dropGold = atoi(goldIt->second.c_str());

            if (dropGold > 0)
                spawnGoldAt(p.roomX, p.roomY, p.roomZ, dropGold);

            npc->gold = 0;

            npc->respawnTime = millis() + (30UL * 60UL * 1000UL);

            for (int i = 0; i < MAX_PLAYERS; i++)
                npc->hostileTo[i] = false;

            npc->targetPlayer = -1;

            onQuestEvent(p, "kill", "", npc->npcId, "", 0,0,0);

            p.inCombat = false;
            p.combatTarget = nullptr;
            return;
        }

    } else {
        p.client.println("You miss.");

        if (p.IsWizard && p.showStats) {
            int armorTotal = p.baseDefense + p.armorBonus;
            int dmgTotal   = p.attack + p.weaponBonus;

            p.client.println("  (damage=0)");
            p.client.println("    armor:  base(" + String(p.baseDefense) +
                             ") + bonus(" + String(p.armorBonus) +
                             ") = " + String(armorTotal));
            p.client.println("    damage: base(" + String(p.attack) +
                             ") + bonus(" + String(p.weaponBonus) +
                             ") = " + String(dmgTotal));
        }

        broadcastRoomExcept(
            p,
            capFirst(p.name) + " misses the " + npcName + "!",
            p
        );
    }

    // ---------------------------------------------------------
    // NPC COUNTERATTACK
    // ---------------------------------------------------------
    if (npc->targetPlayer == playerIndex) {

        int playerDefense = p.baseDefense + p.armorBonus;

        if (rollToHit(npcBaseDmg, playerDefense)) {

            int npcRollDmg = random(1, npcBaseDmg + 1);

            int absorb = (p.armorBonus > 0)
                         ? random(0, p.armorBonus + 1)
                         : 0;

            int finalNpcDmg = npcRollDmg - absorb;
            if (finalNpcDmg < 1) finalNpcDmg = 1;

            p.hp -= finalNpcDmg;

            if (p.IsWizard && p.hp <= 0) {
                p.hp = 1;
                p.client.println("Your immortality protects you from death!");
                goto SKIP_DEATH_CHECK;
            }

            if (p.hp < 0) p.hp = 0;

            String nverb = combatVerbs[random(7)];

            p.client.println(npcName + " hits you.");

            if (p.IsWizard && p.showStats) {
                int playerArmorTotal = p.baseDefense + p.armorBonus;
                int playerDmgTotal   = p.attack + p.weaponBonus;

                p.client.println("  (damage=" + String(finalNpcDmg) + ")");
                p.client.println("    npcDamageRoll: " + String(npcRollDmg));
                p.client.println("    yourArmorAbsorb: " + String(absorb));
                p.client.println("    yourArmor: base(" + String(p.baseDefense) +
                                 ") + bonus(" + String(p.armorBonus) +
                                 ") = " + String(playerArmorTotal));
                p.client.println("    yourDamage: base(" + String(p.attack) +
                                 ") + bonus(" + String(p.weaponBonus) +
                                 ") = " + String(playerDmgTotal));
            }

            broadcastRoomExcept(
                p,
                "The " + npcName + " " + nverb + "s " +
                capFirst(p.name) + "!",
                p
            );

        } else {
            p.client.println(npcName + " misses you.");

            if (p.IsWizard && p.showStats) {
                int playerArmorTotal = p.baseDefense + p.armorBonus;
                int playerDmgTotal   = p.attack + p.weaponBonus;

                p.client.println("  (damage=0)");
                p.client.println("    yourArmor: base(" + String(p.baseDefense) +
                                 ") + bonus(" + String(p.armorBonus) +
                                 ") = " + String(playerArmorTotal));
                p.client.println("    yourDamage: base(" + String(p.attack) +
                                 ") + bonus(" + String(p.weaponBonus) +
                                 ") = " + String(playerDmgTotal));
            }

            broadcastRoomExcept(
                p,
                "The " + npcName + " misses " + capFirst(p.name) + "!",
                p
            );
        }
    }

    // ---------------------------------------------------------
    // AGGRESSIVE NPC DIALOG
    // ---------------------------------------------------------
    {
        auto agIt = def.attributes.find("aggressive");
        if (npc->hp > 0 &&
            agIt != def.attributes.end() &&
            atoi(agIt->second.c_str()) == 1)
        {
            npc->combatDialogCounter++;

            if (npc->combatDialogCounter >= 3) {
                npc->combatDialogCounter = 0;

                int idx = npc->dialogOrder[npc->dialogIndex];
                String key = "dialog_" + String(idx + 1);

                auto dlgIt = def.attributes.find(std::string(key.c_str()));
                if (dlgIt != def.attributes.end()) {

                    String line = String(dlgIt->second.c_str());

                    if (line.endsWith(".")) {
                        line.remove(line.length() - 1);
                        line += "!";
                    }

                    announceToRoom(
                        npc->x, npc->y, npc->z,
                        "The " + npcName + " yells: \"" + line + "\"",
                        -1
                    );
                }

                npc->dialogIndex++;
                if (npc->dialogIndex >= 3) {
                    npc->dialogIndex = 0;

                    for (int i = 0; i < 3; i++) {
                        int r = random(0, 3);
                        int tmp = npc->dialogOrder[i];
                        npc->dialogOrder[i] = npc->dialogOrder[r];
                        npc->dialogOrder[r] = tmp;
                    }
                }
            }
        }
    }

SKIP_DEATH_CHECK:

    // Player death
    if (!p.IsWizard && p.hp == 0) {
        npc->hostileTo[playerIndex] = false;
        if (npc->targetPlayer == playerIndex)
            npc->targetPlayer = -1;
        handlePlayerDeath(p);
        return;
    }

    // Wimp flee
    if (p.wimpMode && p.hp <= 5) {
        autoWimpFlee(p);
        return;
    }

    // End combat if hostility lost
    if (lostHostility) {
        p.inCombat = false;
        p.combatTarget = nullptr;
    }

    p.nextCombatTime = millis() + 3000;
}

// =============================
// Wimp / healing
// =============================



void cmdEnterMsg(Player &p, const String &args) {
    if (!p.IsWizard) {
        p.client.println("What?");
        return;
    }

    String msg = args;
    msg.trim();

    if (msg.length() == 0) {
        p.EnterMsg = "";
        p.client.println("Custom enter message cleared.");
        return;
    }

    p.EnterMsg = sanitizeMsg(msg);
    p.client.println("Enter message set to: " + msg);
}

void cmdExitMsg(Player &p, const String &args) {
    if (!p.IsWizard) {
        p.client.println("What?");
        return;
    }

    String msg = args;
    msg.trim();

    if (msg.length() == 0) {
        p.ExitMsg = "";
        p.client.println("Custom exit message cleared.");
        return;
    }

    p.ExitMsg  = sanitizeMsg(msg);
    p.client.println("Exit message set to: " + msg);
}


void cmdHeal(Player &p, const String &input) {
    if (!p.IsWizard) {
        p.client.println("You lack the power to do that.");
        return;
    }

    String arg = input;
    arg.trim();

    if (arg.length() == 0) {
        p.client.println("Heal whom?");
        return;
    }

    // Find target player by name (case-insensitive)
    Player* target = nullptr;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!players[i].active || !players[i].loggedIn) continue;

        if (!strcasecmp(players[i].name, arg.c_str())) {
            target = &players[i];
            break;
        }
    }

    if (!target) {
        p.client.println("No such player is online.");
        return;
    }

    // Heal target
    target->hp = target->maxHp;

    // Message to target
    target->client.println("You have been healed by a higher power!");

    // Message to wizard
    p.client.println("You restore " + String(capFirst(target->name)) + " to full health.");
}

void cmdGoto(Player &p, const String &args) {
    if (!p.IsWizard) {
        p.client.println("What?");
        return;
    }

    // Resolve playerIndex
    int playerIndex = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (&players[i] == &p) {
            playerIndex = i;
            break;
        }
    }
    if (playerIndex < 0) return;

    String a = args;
    a.trim();
    if (a.length() == 0) {
        p.client.println("Usage: goto <x,y,z>  OR  goto <player>");
        return;
    }

    // ---------------------------------------------------------
    // SUPPORT BOTH:  "250 250 50"  AND  "250,250,50"
    // ---------------------------------------------------------
    String norm = a;
    norm.replace(",", " ");
    norm.trim();

    int x, y, z;
    int count = sscanf(norm.c_str(), "%d %d %d", &x, &y, &z);

    // ---------------------------------------------------------
    // COORDINATE TELEPORT
    // ---------------------------------------------------------
    if (count == 3) {

        // Perform teleport
        if (!loadRoomForPlayer(p, x, y, z)) {
            p.client.println("No such room.");
            return;
        }

        // Announce arrival if wizard is visible
        if (!p.IsInvisible) {
           String arrival;

            if (p.EnterMsg.length() > 0)
                arrival = String(capFirst(p.name)) + " " + p.EnterMsg;
            else
                arrival = String(capFirst(p.name)) + " materializes out of thin air!";

            announceToRoom(
                p.roomX,
                p.roomY,
                p.roomZ,
                arrival,
                playerIndex
            );
        }

        // Echo voxel coords for VB.NET mapper
        p.client.print("(");
        p.client.print(p.roomX);
        p.client.print(",");
        p.client.print(p.roomY);
        p.client.print(",");
        p.client.print(p.roomZ);
        p.client.println(")");

        cmdLook(p);
        return;
    }

    // ---------------------------------------------------------
    // PLAYER NAME TELEPORT
    // ---------------------------------------------------------
    String target = a;
    target.trim();

    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player &other = players[i];
        if (!other.active) continue;
        if (&other == &p) continue;

        if (npcNameMatches(other.name, target)) {

            if (!loadRoomForPlayer(p, other.roomX, other.roomY, other.roomZ)) {
                p.client.println("Teleport failed.");
                return;
            }

            // Announce arrival if wizard is visible
            if (!p.IsInvisible) {
                String arrival;

                if (p.EnterMsg.length() > 0)
                    arrival = String(capFirst(p.name)) + " " + p.EnterMsg;
                else
                    arrival = String(capFirst(p.name)) + " materializes out of thin air!";

                announceToRoom(
                    p.roomX,
                    p.roomY,
                    p.roomZ,
                    arrival,
                    playerIndex
                );
            }

            // Echo voxel coords for VB.NET mapper
            p.client.print("(");
            p.client.print(p.roomX);
            p.client.print(",");
            p.client.print(p.roomY);
            p.client.print(",");
            p.client.print(p.roomZ);
            p.client.println(")");

            cmdLook(p);
            return;
        }
    }

    p.client.println("No such player or room.");
}

void cmdWimp(Player &p) {
    p.wimpMode = !p.wimpMode;
    p.client.print("Wimp mode is now ");
    p.client.println(p.wimpMode ? "ON" : "OFF");

    // Auto-save the new setting
    savePlayerToFS(p);
}


void cmdEat(Player &p, const char* arg) {
    String t = arg;
    t.trim();

    if (t.length() == 0) {
        p.client.println("Eat what?");
        return;
    }

    int idx = findItemInRoomOrInventory(p, t);

    // If no exact match, try partial match for any item
    if (idx == -1) {
        String searchLower = t;
        searchLower.toLowerCase();
        
        // Search inventory first
        for (int i = 0; i < p.invCount; i++) {
            int worldIndex = p.invIndices[i];
            if (worldIndex < 0 || worldIndex >= (int)worldItems.size())
                continue;
            
            WorldItem &wi = worldItems[worldIndex];
            String id = wi.name;
            id.toLowerCase();
            String disp = resolveDisplayName(wi);
            disp.toLowerCase();
            
            // Check if item matches name or display name (partial)
            if (id.indexOf(searchLower) != -1 || disp.indexOf(searchLower) != -1) {
                idx = (i | 0x80000000);
                break;
            }
        }
        
        // If still not found, search room
        if (idx == -1) {
            Room &r = p.currentRoom;
            for (int i = 0; i < (int)worldItems.size(); i++) {
                WorldItem &wi = worldItems[i];
                if (wi.x != r.x || wi.y != r.y || wi.z != r.z)
                    continue;
                
                String id = wi.name;
                id.toLowerCase();
                String disp = resolveDisplayName(wi);
                disp.toLowerCase();
                
                // Check if item matches name or display name (partial)
                if (id.indexOf(searchLower) != -1 || disp.indexOf(searchLower) != -1) {
                    idx = i;
                    break;
                }
            }
        }
    }

    // IMPORTANT: -1 means "not found"
    if (idx == -1) {
        p.client.println("You don't see that here.");
        return;
    }

    bool fromInv = (idx & 0x80000000);
    int worldIndex = fromInv
                     ? p.invIndices[idx & 0x7FFFFFFF]
                     : idx;

    if (worldIndex < 0 || worldIndex >= (int)worldItems.size()) {
        p.client.println("You don't see that here.");
        return;
    }

    WorldItem &wi = worldItems[worldIndex];

    // Check if player is too full
    if (p.fullness <= 0) {
        p.client.println("You are too full to eat any more!");
        return;
    }

    // Look up item definition for values
    std::string key = std::string(wi.name.c_str());
    int fullnessCost = 1;
    int heal = 0;

    auto it = itemDefs.find(key);
    if (it != itemDefs.end()) {
        // Get fullness cost from value attribute (default 1)
        auto valueIt = it->second.attributes.find("value");
        if (valueIt != it->second.attributes.end()) {
            int valueCost = strToInt(valueIt->second);
            if (valueCost > 0) {
                fullnessCost = valueCost;
            }
        }
        
        // Get heal amount
        auto healIt = it->second.attributes.find("heal");
        if (healIt != it->second.attributes.end()) {
            heal = strToInt(healIt->second);
        }
    }

    // Deduct fullness units
    p.fullness -= fullnessCost;
    if (p.fullness < 0) p.fullness = 0;

    // Restore HP
    int oldHp = p.hp;
    p.hp += heal;
    if (p.hp > p.maxHp) p.hp = p.maxHp;

    String disp = getItemDisplayName(wi);

    p.client.println("You eat the " + disp + ".");
    p.client.println("You feel more refreshed.");
    
    // Fullness feedback
    if (p.fullness == 0) {
        p.client.println("You are now too full and can't eat any more!");
    } else if (p.fullness <= 2) {
        p.client.println("You're getting quite full...");
    }

    broadcastRoomExcept(
        p,
        capFirst(p.name) + " eats a " + disp + ".",
        p
    );

    consumeWorldItem(p, worldIndex);
}





void updateHealing(Player &p) {
  unsigned long now = millis();
  if (now - p.lastHealCheck >= 60000UL) {
    p.lastHealCheck = now;
    if (p.hp < p.maxHp) {
      p.hp++;
      p.client.print("You feel a little better. HP: (");
      p.client.print(p.hp);
      p.client.print("/");
      p.client.print(p.maxHp);
      p.client.println(")");
    }
  }
}


// =============================
// Inventory manipulation (get/drop) using WorldItems (P/C aware)
// =============================

bool fullWordMatch(const String &targetRaw, const String &candidateRaw) {
    String t = targetRaw;
    t.trim();
    t.toLowerCase();

    String c = candidateRaw;
    c.trim();
    c.toLowerCase();

    // Exact match
    if (c == t) return true;

    // Split candidate into words
    int start = 0;
    while (true) {
        int space = c.indexOf(' ', start);
        String word = (space == -1)
            ? c.substring(start)
            : c.substring(start, space);

        word.trim();
        if (word == t) return true;

        if (space == -1) break;
        start = space + 1;
    }

    return false;
}

// =============================
// SPECIAL ROOM HELPERS
// =============================

/**
 * Detect if a room is a recycling center
 * Checks if room name contains variations of "recycle" or "recycling"
 */
bool isRecyclingCenter(const Room &room) {
    String name = String(room.name);
    name.toLowerCase();
    return (name.indexOf("recycl") >= 0);  // Matches "recycle", "recycling", "recycler", etc.
}

/**
 * Handle item recycling in Recycling Center rooms
 * Removes item from world with eco-conscious message
 * showAppreciationMsg: if true, show "We appreciate..." message; if false, only show "wisks away" message
 */
void recycleItem(Player &p, int worldIndex, bool showAppreciationMsg = true) {
    if (worldIndex < 0 || worldIndex >= (int)worldItems.size()) {
        return;
    }
    
    WorldItem &item = worldItems[worldIndex];
    String itemName = getItemDisplayName(item);
    
    // Remove the item from the world
    item.x = item.y = item.z = -1;
    item.ownerName = "DELETED";
    item.parentName = "DELETED";
    
    // Send message to player
    if (showAppreciationMsg) {
        p.client.println("We appreciate you being eco-conscious!");
    }
    p.client.println("The attendant wisks the " + itemName + " away. Hope it didn't cost much!");
}

void cmdQuestList(Player &p) {
    p.client.println("===== Available Quests =====");
    p.client.println();

    for (auto &pair : quests) {
        const QuestDef &qd = pair.second;

        // Only show quests that have a name/description
        if (qd.name.length() == 0)
            continue;

        // Determine completion status
        bool completed = p.questCompleted[qd.questId - 1];
        String status = completed ? " (completed)" : " (in progress)";

        // Print quest header with status
        p.client.println("Quest " + String(qd.questId) + ": " + qd.name + status);

        // Print description (word-wrapped, with indentation preserved)
        String wrappedDesc = wordWrap(qd.description, MAX_OUTPUT_WIDTH - 2);  // Leave 2 spaces for indent
        int pos = 0;
        while (pos < wrappedDesc.length()) {
            int newlinePos = wrappedDesc.indexOf('\n', pos);
            String line;
            if (newlinePos == -1) {
                line = wrappedDesc.substring(pos);
                p.client.println("  " + line);
                break;
            } else {
                line = wrappedDesc.substring(pos, newlinePos);
                p.client.println("  " + line);
                pos = newlinePos + 1;
            }
        }
        p.client.println();
    }

    p.client.println("============================");
}


//Helper
void mergeCoinPilesAt(int x, int y, int z) {
    int total = 0;

    // First pass: sum all coin piles at this location
    for (int i = 0; i < (int)worldItems.size(); i++) {
        WorldItem &wi = worldItems[i];

        if (wi.name == "gold_coin" &&
            wi.ownerName.length() == 0 &&
            wi.x == x && wi.y == y && wi.z == z)
        {
            total += wi.value;
            wi.value = 0; // mark for deletion
        }
    }

    // Second pass: remove zero-value piles at this location
    for (int i = 0; i < (int)worldItems.size(); ) {
        WorldItem &wi = worldItems[i];

        if (wi.name == "gold_coin" &&
            wi.ownerName.length() == 0 &&
            wi.x == x && wi.y == y && wi.z == z &&
            wi.value == 0)
        {
            worldItems.erase(worldItems.begin() + i);
        } else {
            i++;
        }
    }

    // If there were any coins, create a single merged pile
    if (total > 0) {
        WorldItem wi;
        wi.name = "gold_coin";
        wi.value = total;
        wi.ownerName = "";
        wi.parentName = "";
        wi.x = x;
        wi.y = y;
        wi.z = z;
        worldItems.push_back(wi);
    }
}







// =============================================================
// GET <item>   OR   GET COINS
// =============================================================


void cmdGet(Player &p, const String &input) {

    String raw = input;
    raw.trim();

    String lower = raw;
    lower.toLowerCase();

    // -----------------------------------------
    // SPECIAL CASE: GET <n> COINS
    // -----------------------------------------
    if (lower.indexOf("coin") >= 0) {

        // Extract number before "coin"
        int amount = 0;

        // Split by spaces
        int spacePos = lower.indexOf(' ');
        if (spacePos > 0) {
            String num = lower.substring(0, spacePos);
            amount = num.toInt();
        }

        // If no number given, default to full pile
        bool wantAll = false;
        if (amount <= 0) {
            wantAll = true;
        }

        // Find coin pile in room
        for (int i = 0; i < (int)worldItems.size(); i++) {
            WorldItem &wi = worldItems[i];

            if (wi.name == "gold_coin" &&
                wi.x == p.roomX && wi.y == p.roomY && wi.z == p.roomZ) {

                int take = wantAll ? wi.value : amount;

                if (take > wi.value)
                    take = wi.value;

                p.coins += take;

                // Reduce or delete pile
                wi.value -= take;

                if (wi.value <= 0) {
                    wi.x = wi.y = wi.z = -1;
                    wi.ownerName = "DELETED";
                }

                if (take == 1)
                    p.client.println("You pick up 1 gold coin.");
                else
                    p.client.println("You pick up " + String(take) + " gold coins.");
                return;
            }
        }

        p.client.println("You don't see any coins here.");
        return;
    }

    // -----------------------------------------
    // NORMAL GET: partial-name match in room
    // -----------------------------------------
    String arg = raw;
    int spacePos = arg.indexOf(' ');
    if (spacePos > 0) {
        arg = arg.substring(0, spacePos);
    }

    String argLower = arg;
    argLower.toLowerCase();

    int foundIndex = -1;

    for (int i = 0; i < (int)worldItems.size(); i++) {
        WorldItem &wi = worldItems[i];

        if (wi.x != p.roomX || wi.y != p.roomY || wi.z != p.roomZ)
            continue;

        String id   = wi.name;
        String disp = getItemDisplayName(wi);

        String idLower   = id;   idLower.toLowerCase();
        String dispLower = disp; dispLower.toLowerCase();

        if (idLower.indexOf(argLower) >= 0 || dispLower.indexOf(argLower) >= 0) {
            foundIndex = i;
            break;
        }
    }

    if (foundIndex < 0) {
        p.client.println("You don't see that here.");
        return;
    }

    // -----------------------------------------
    // Add item to inventory
    // -----------------------------------------
    if (p.invCount >= 32) {
        p.client.println("You cannot carry any more.");
        return;
    }

    // Check weight limit
    WorldItem &targetItem = worldItems[foundIndex];
    int itemWeight = getWeight(targetItem);
    int currentWeight = getCarriedWeight(p);
    int maxWeight = getMaxCarryWeight(p);
    
    if (currentWeight + itemWeight > maxWeight) {
        p.client.println("You cannot carry any more.");
        return;
    }

    p.invIndices[p.invCount++] = foundIndex;

    targetItem.ownerName = p.name;
    targetItem.parentName = "";
    targetItem.x = targetItem.y = targetItem.z = -1;

    p.client.println("You pick up " + getItemDisplayName(targetItem) + ".");
}

// =============================================================
// SHOP SYSTEM IMPLEMENTATION
// =============================================================

ShopInventoryItem* Shop::findItem(const String &target) {
    String searchLower = target;
    searchLower.toLowerCase();
    
    for (auto &item : inventory) {
        String nameLower = item.itemName;
        nameLower.toLowerCase();
        
        if (nameLower == searchLower || nameLower.indexOf(searchLower) != -1) {
            return &item;
        }
    }
    return nullptr;
}

void Shop::displayInventory(Client &client) {
    client.println("");
    client.println("====== " + shopName + " Inventory ======");
    client.println("");
    client.println("#   Description                     Price");
    client.println("------------------------------------------");
    
    if (inventory.empty()) {
        client.println("The shelves are empty.");
        client.println("");
        return;
    }
    
    for (const auto &item : inventory) {
        String numStr = "";
        numStr += item.itemNumber;
        numStr += ".";
        
        // Pad number to 4 characters
        while (numStr.length() < 4) {
            numStr += " ";
        }
        
        // Item name padded to 30 characters
        String nameStr = item.itemName;
        while (nameStr.length() < 30) {
            nameStr += " ";
        }
        
        // Price string
        String priceStr = "";
        priceStr += item.price;
        
        // Pad price to 4 characters
        while (priceStr.length() < 4) {
            priceStr = " " + priceStr;
        }
        
        String line = numStr + nameStr + priceStr + "   gp";
        client.println(line);
    }
    client.println("");
}

void Shop::addItem(const String &itemId, const String &displayName, int qty, int price) {
    ShopInventoryItem item;
    item.itemNumber = (int)inventory.size() + 1;
    item.itemId = itemId;
    item.itemName = displayName;
    item.quantity = qty;
    item.price = price;
    inventory.push_back(item);
}

void Shop::addOrUpdateItem(const String &itemId, const String &displayName, int price) {
    // Check if item already exists
    for (auto &item : inventory) {
        if (item.itemId == itemId) {
            // Item exists - increment quantity and update price
            item.quantity++;
            item.price = price;
            return;
        }
    }
    
    // Item doesn't exist - add it
    addItem(itemId, displayName, 1, price);
}

Shop* getShopForRoom(Player &p) {
    for (auto &shop : shops) {
        if (shop.x == p.currentRoom.x && 
            shop.y == p.currentRoom.y && 
            shop.z == p.currentRoom.z) {
            return &shop;
        }
    }
    return nullptr;
}

void initializeShops() {
    // Clear existing shops
    shops.clear();
    
    // BLACKSMITH SHOP at voxel 254,244,50
    Shop blacksmith;
    blacksmith.x = 254;
    blacksmith.y = 244;
    blacksmith.z = 50;
    blacksmith.shopName = "Blacksmith's Forge";
    blacksmith.shopType = "blacksmith";
    
    // Add weapons (itemId, displayName, initialQty, price)
    blacksmith.addItem("dagger", "Dagger", 5, 25);
    blacksmith.addItem("skinning_knife", "Knife", 5, 15);
    blacksmith.addItem("sword", "Sword", 3, 150);
    blacksmith.addItem("mace", "Mace", 3, 175);
    
    // Add armor (itemId, displayName, initialQty, price)
    // Studded Leather = outer torso
    blacksmith.addItem("studded_leather", "Studded Leather", 4, 125);
    // Iron Helmet = head
    blacksmith.addItem("iron_helmet", "Iron Helmet", 6, 80);
    // Chainmail = under torso
    blacksmith.addItem("chainmail", "Chainmail", 2, 300);
    // Gauntlets = hands
    blacksmith.addItem("gauntlets", "Gauntlets", 4, 75);
    
    shops.push_back(blacksmith);

    // ESPERTHERTURIUM WARES SHOP at voxel 251,248,50
    Shop wares;
    wares.x = 251;
    wares.y = 248;
    wares.z = 50;
    wares.shopName = "The Espertherturium Wares";
    wares.shopType = "misc";
    
    // Add misc items (itemId, displayName, initialQty, price)
    wares.addItem("torch", "Torch", 6, 4);
    wares.addItem("rope", "Rope", 4, 3);
    wares.addItem("waterskin", "Waterskin", 5, 5);
    wares.addItem("bedroll", "Bedroll", 3, 8);
    wares.addItem("lockpick_set", "Lockpick Set", 2, 12);
    wares.addItem("herb_bundle", "Herb Bundle", 8, 6);
    wares.addItem("leather_pouch", "Leather Pouch", 4, 7);
    wares.addItem("candle", "Candle", 12, 2);
    wares.addItem("iron_spoon", "Iron Spoon", 10, 1);
    wares.addItem("map_fragment", "Map Fragment", 3, 10);
    
    shops.push_back(wares);

    // TAVERN LIBATIONS SHOP at voxel 249,242,50
    Shop tavern_shop;
    tavern_shop.x = 249;
    tavern_shop.y = 242;
    tavern_shop.z = 50;
    tavern_shop.shopName = "Tavern Libations";
    tavern_shop.shopType = "misc";
    
    // Add drinks (unlimited inventory at tavern)
    tavern_shop.addItem("giants_beer", "Giant's Beer", 999, 10);
    tavern_shop.addItem("honeyed_mead", "Honeyed Mead", 999, 5);
    tavern_shop.addItem("faery_fire", "Faery Fire", 999, 20);
    
    shops.push_back(tavern_shop);
}

WorldItem* findShopSign(Player &p) {
    Room &r = p.currentRoom;

    for (auto &wi : worldItems) {
        // Must be in this room, top-level, not owned
        if (wi.ownerName.length() != 0) continue;
        if (wi.parentName.length() != 0) continue;
        if (wi.x != r.x || wi.y != r.y || wi.z != r.z) continue;

        // Must be type=sign in itemDefs
        String t = wi.getAttr("type", itemDefs);
        t.toLowerCase();
        if (t != "sign") continue;

        // Must have at least one child (shop inventory)
        bool hasChildren = false;
        for (auto &child : worldItems) {
            if (child.parentName == wi.name) {
                hasChildren = true;
                break;
            }
        }

        if (hasChildren)
            return &wi;
    }

    return nullptr;
}



void showItemDescription(Player &p, WorldItem &wi) {
    String c1 = wi.getAttr("container", itemDefs);
    String c2 = wi.getAttr("can_contain", itemDefs);
    String emptyDesc = wi.getAttr("empty_desc", itemDefs);
    
    // For containers with empty_desc attribute, check if truly empty (no children at all)
    if ((c1 == "1" || c2 == "1") && emptyDesc.length() > 0) {
        // If no children at all (visible or hidden), show empty_desc
        if (wi.children.empty()) {
            printWrapped(p.client, emptyDesc);
            return;
        }
    }
    
    // Show regular description (whether container has visible or hidden children)
    String desc = wi.getAttr("desc", itemDefs);
    if (desc.length() == 0)
        desc = "You see nothing special.";

    printWrapped(p.client, desc);
}

void showItemDescriptionNormal(Player &p, WorldItem &wi) {
    // Always show regular desc, ignore empty_desc (used for search/examine before revealing items)
    String desc = wi.getAttr("desc", itemDefs);
    if (desc.length() == 0)
        desc = "You see nothing special.";

    printWrapped(p.client, desc);
}


void showPlayerDescription(Player &p, Player &other) {
    p.client.println(other.name);
    p.client.println(getPlayerTitle(other.raceId, other.level));
}

// =============================================================
// TAVERN DRINK SYSTEM IMPLEMENTATION
// =============================================================

Drink* Tavern::findDrink(const String &target) {
    String targetLower = target;
    targetLower.toLowerCase();
    
    for (auto &drink : drinks) {
        String nameLower = drink.name;
        nameLower.toLowerCase();
        if (nameLower.indexOf(targetLower) >= 0) {
            return &drink;
        }
    }
    return nullptr;
}

void Tavern::displayMenu() {
    // This will be called when displaying the tavern sign
}

Tavern* getTavernForRoom(Player &p) {
    for (auto &tavern : taverns) {
        if (tavern.x == p.roomX && 
            tavern.y == p.roomY && 
            tavern.z == p.roomZ) {
            return &tavern;
        }
    }
    return nullptr;
}

PostOffice* getPostOfficeForRoom(Player &p) {
    for (auto &po : postOffices) {
        if (po.x == p.roomX && 
            po.y == p.roomY && 
            po.z == p.roomZ) {
            return &po;
        }
    }
    return nullptr;
}

void initializeTaverns() {
    // Clear existing taverns
    taverns.clear();
    
    // TAVERN LIBATIONS at voxel 249,242,50
    Tavern tavern;
    tavern.x = 249;
    tavern.y = 242;
    tavern.z = 50;
    tavern.tavernName = "Tavern Libations";
    
    // Add drinks
    Drink giants_beer;
    giants_beer.name = "Giant's Beer";
    giants_beer.price = 10;
    giants_beer.hpRestore = 2;
    giants_beer.drunkennessCost = 3;
    tavern.drinks.push_back(giants_beer);
    
    Drink honeyed_mead;
    honeyed_mead.name = "Honeyed Mead";
    honeyed_mead.price = 5;
    honeyed_mead.hpRestore = 3;
    honeyed_mead.drunkennessCost = 2;
    tavern.drinks.push_back(honeyed_mead);
    
    Drink faery_fire;
    faery_fire.name = "Faery Fire";
    faery_fire.price = 20;
    faery_fire.hpRestore = 5;
    faery_fire.drunkennessCost = 5;
    tavern.drinks.push_back(faery_fire);
    
    taverns.push_back(tavern);
}

void initializePostOffices() {
    // Clear existing post offices
    postOffices.clear();
    
    // POST OFFICE at voxel 252,248,50
    PostOffice po;
    po.x = 252;
    po.y = 248;
    po.z = 50;
    po.postOfficeName = "The Post Office";
    
    postOffices.push_back(po);
}

/**
 * Extract player name from email body
 * Looks for: "a message from [playername]" (case-insensitive)
 * Returns the extracted name, or empty string if not found
 */
String extractPlayerNameFromEmail(const String &emailBody) {
    String body = emailBody;
    body.toLowerCase();
    
    int pos = body.indexOf("a message from ");
    if (pos == -1) {
        return "";
    }
    
    // Start after "a message from "
    int startPos = pos + 15;  // strlen("a message from ") = 15
    
    // Find the end of the name (look for " - the" or ":" or newline)
    int endPos = startPos;
    while (endPos < body.length()) {
        char c = body[endPos];
        if (c == '-' || c == ':' || c == '\r' || c == '\n') {
            break;
        }
        endPos++;
    }
    
    String name = emailBody.substring(startPos, endPos);
    name.trim();
    return name;
}

/**
 * Fetch mail from the web server via HTTP API
 * Returns true if successful, populates letters vector
 */
bool fetchMailFromServer(const String &playerName, std::vector<Letter> &letters) {
    if (!playerName || playerName.length() == 0) {
        Serial.println("[MAIL] ERROR: Empty player name");
        return false;
    }
    
    HTTPClient http;
    // Don't pass player name - get ALL unread emails and filter by body content
    String url = "https://www.storyboardacs.com/retrieveESP32mail.php";
    
    Serial.println("");
    Serial.println("========== MAIL FETCH START ==========");
    Serial.print("[MAIL] Player: ");
    Serial.println(playerName);
    Serial.print("[MAIL] URL: ");
    Serial.println(url);
    
    http.setConnectTimeout(5000);   // 5 second connection timeout
    http.setTimeout(10000);          // 10 second total timeout
    
    Serial.println("[MAIL] Attempting HTTP connection...");
    
    if (!http.begin(url)) {
        Serial.println("[MAIL] ERROR: Failed to begin HTTP request");
        return false;
    }
    
    Serial.println("[MAIL] HTTP connection started, sending GET request...");
    http.addHeader("Content-Type", "application/json");
    
    int httpCode = http.GET();
    Serial.print("[MAIL] HTTP Response Code: ");
    Serial.println(httpCode);
    
    if (httpCode != HTTP_CODE_OK) {
        Serial.print("[MAIL] ERROR: HTTP error code ");
        Serial.println(httpCode);
        http.end();
        Serial.println("========== MAIL FETCH END (ERROR) ==========\n");
        return false;
    }
    
    String response = http.getString();
    http.end();
    
    Serial.print("[MAIL] Response length: ");
    Serial.print(response.length());
    Serial.println(" bytes");
    
    if (response.length() > 500) {
        Serial.print("[MAIL] Response (first 500 chars): ");
        Serial.println(response.substring(0, 500));
    } else {
        Serial.print("[MAIL] Full Response: ");
        Serial.println(response);
    }
    
    // Parse JSON response
    // Format: {"success": true, "emails": [...], "count": N, "errors": [...]}
    
    // Simple JSON parsing (no external library)
    // Check for success - handle both "success": true and "success":true (no spaces)
    if (response.indexOf("\"success\":true") == -1 && response.indexOf("\"success\": true") == -1) {
        Serial.println("[MAIL] ERROR: Server returned error or success=false");
        Serial.println("[MAIL] Looking for success flag in response...");
        Serial.println(response);
        Serial.println("========== MAIL FETCH END (ERROR) ==========\n");
        return false;
    }
    
    Serial.println("[MAIL] Response success=true");
    
    if (response.indexOf("\"count\":0") != -1 || response.indexOf("\"count\": 0") != -1) {
        Serial.println("[MAIL] No new mail (count=0)");
        Serial.println("========== MAIL FETCH END (SUCCESS, NO MAIL) ==========\n");
        return true;  // Success, just no mail
    }
    
    Serial.println("[MAIL] Response contains emails, parsing...");
    
    // Extract email array
    // Look for "emails": [...]
    int emailsStart = response.indexOf("\"emails\": [");
    if (emailsStart == -1) {
        emailsStart = response.indexOf("\"emails\":[");  // Also try without space
    }
    if (emailsStart == -1) {
        Serial.println("[MAIL] ERROR: Could not find emails array in response");
        Serial.println("========== MAIL FETCH END (ERROR) ==========\n");
        return false;
    }
    
    Serial.println("[MAIL] Found emails array, parsing individual emails...");
    
    // Parse each email object in the array
    // Very basic parsing - assumes well-formed JSON
    int pos = emailsStart;
    if (response.indexOf("\"emails\": [") != -1) {
        pos += 11;  // Move past "emails": [
    } else {
        pos += 10;  // Move past "emails":[
    }
    
    int emailCount = 0;
    
    while (pos < response.length()) {
        // Look for opening brace of email object
        int objStart = response.indexOf('{', pos);
        if (objStart == -1) break;
        
        // Find closing brace
        int objEnd = response.indexOf('}', objStart);
        if (objEnd == -1) break;
        
        String emailObj = response.substring(objStart, objEnd + 1);
        
        Serial.print("[MAIL] Parsing email object ");
        Serial.print(emailCount + 1);
        Serial.print(": ");
        Serial.println(emailObj.substring(0, 100));
        
        // Parse fields
        Letter letter;
        
        // Extract "to"
        int toStart = emailObj.indexOf("\"to\": \"");
        if (toStart == -1) toStart = emailObj.indexOf("\"to\":\"");
        if (toStart != -1) {
            toStart = emailObj.indexOf("\"", toStart + 4) + 1;
            int toEnd = emailObj.indexOf("\"", toStart);
            if (toEnd != -1) {
                letter.to = emailObj.substring(toStart, toEnd);
            }
        }
        
        // Extract "from"
        int fromStart = emailObj.indexOf("\"from\": \"");
        if (fromStart == -1) fromStart = emailObj.indexOf("\"from\":\"");
        if (fromStart != -1) {
            fromStart = emailObj.indexOf("\"", fromStart + 5) + 1;
            int fromEnd = emailObj.indexOf("\"", fromStart);
            if (fromEnd != -1) {
                letter.from = emailObj.substring(fromStart, fromEnd);
            }
        }
        
        // Extract "subject"
        int subjStart = emailObj.indexOf("\"subject\": \"");
        if (subjStart == -1) subjStart = emailObj.indexOf("\"subject\":\"");
        if (subjStart != -1) {
            subjStart = emailObj.indexOf("\"", subjStart + 8) + 1;
            int subjEnd = emailObj.indexOf("\"", subjStart);
            if (subjEnd != -1) {
                letter.subject = emailObj.substring(subjStart, subjEnd);
            }
        }
        
        // Extract "body" (note: body may contain escaped newlines and special chars)
        // This is more robust - find "body": then skip to the opening quote, then extract until closing quote
        int bodyStart = emailObj.indexOf("\"body\":");
        if (bodyStart != -1) {
            // Find the opening quote for the body value
            bodyStart = emailObj.indexOf("\"", bodyStart + 7);
            if (bodyStart != -1) {
                bodyStart++;  // Move past the opening quote
                
                // Find the closing quote - must handle escaped characters
                int bodyEnd = bodyStart;
                while (bodyEnd < emailObj.length()) {
                    char c = emailObj[bodyEnd];
                    if (c == '\\' && (bodyEnd + 1) < emailObj.length()) {
                        // Skip escaped character
                        bodyEnd += 2;
                    } else if (c == '"') {
                        // Found unescaped closing quote
                        break;
                    } else {
                        bodyEnd++;
                    }
                }
                
                if (bodyEnd <= emailObj.length()) {
                    letter.body = emailObj.substring(bodyStart, bodyEnd);
                    // Unescape JSON sequences
                    letter.body.replace("\\r\\n", "\r\n");
                    letter.body.replace("\\n", "\n");
                    letter.body.replace("\\\"", "\"");
                    letter.body.replace("\\\\", "\\");
                }
            }
        }
        
        // Extract "messageId"
        int msgIdStart = emailObj.indexOf("\"messageId\": \"");
        if (msgIdStart == -1) msgIdStart = emailObj.indexOf("\"messageId\":\"");
        if (msgIdStart != -1) {
            msgIdStart = emailObj.indexOf("\"", msgIdStart + 10) + 1;
            int msgIdEnd = emailObj.indexOf("\"", msgIdStart);
            if (msgIdEnd != -1) {
                letter.messageId = emailObj.substring(msgIdStart, msgIdEnd);
            }
        }
        
        // Extract "displayName" (preferred playername or sender email part)
        int displayNameStart = emailObj.indexOf("\"displayName\": \"");
        if (displayNameStart == -1) displayNameStart = emailObj.indexOf("\"displayName\":\"");
        if (displayNameStart != -1) {
            displayNameStart = emailObj.indexOf("\"", displayNameStart + 13) + 1;
            int displayNameEnd = emailObj.indexOf("\"", displayNameStart);
            if (displayNameEnd != -1) {
                letter.displayName = emailObj.substring(displayNameStart, displayNameEnd);
            }
        }
        
        if (letter.body.length() > 0) {
            letters.push_back(letter);
            Serial.print("[MAIL] SUCCESS: Parsed email from: ");
            Serial.println(letter.from);
            Serial.print("[MAIL] Email subject: ");
            Serial.println(letter.subject);
            emailCount++;
        } else {
            Serial.println("[MAIL] WARNING: Email has empty body, skipping");
        }
        
        pos = objEnd + 1;
    }
    
    Serial.print("[MAIL] Successfully parsed ");
    Serial.print(letters.size());
    Serial.println(" emails");
    Serial.println("========== MAIL FETCH END (SUCCESS) ==========\n");
    return true;
}

/**
 * Check for mail and spawn letter items in the post office
 * Called when player enters the post office
 */
/**
 * Check for mail and spawn letter items in the post office
 * Called when player enters the post office or uses "check mail" command
 * Returns true if mail was found and letters were spawned
 */
bool checkAndSpawnMailLetters(Player &p) {
    Serial.println("");
    Serial.println("========== POST OFFICE MAIL CHECK ==========");
    Serial.print("[MAIL] Checking mail for player: ");
    Serial.println(p.name);
    
    std::vector<Letter> letters;
    
    // Fetch ALL unread emails (not filtered by player)
    if (!fetchMailFromServer(String(p.name), letters)) {
        Serial.println("[MAIL] RESULT: Failed to fetch mail from server");
        Serial.println("========== POST OFFICE MAIL CHECK END ==========\n");
        return false;
    }
    
    if (letters.size() == 0) {
        Serial.println("[MAIL] RESULT: No mail found");
        Serial.println("========== POST OFFICE MAIL CHECK END ==========\n");
        return false;  // No mail found
    }
    
    Serial.print("[MAIL] Found ");
    Serial.print(letters.size());
    Serial.println(" unread letters from server, spawning as items...");
    
    // Use all letters directly - no recipient filtering
    // Each letter will be displayed with the sender's email address part
    
    Serial.print("[MAIL] RESULT: Found ");
    Serial.print(letters.size());
    Serial.println(" letters - announcing to player and spawning items");
    
    // Mail found - announce it
    p.client.println("\nThe Postal Clerk says: \"You've got mail!\" and drops the mail on the floor.");
    
    // Create letter items in the room
    for (int i = 0; i < (int)letters.size(); i++) {
        Letter &letter = letters[i];
        
        Serial.print("[MAIL] Creating letter item ");
        Serial.print(i + 1);
        Serial.print(" of ");
        Serial.println(letters.size());
        
        // Use player's name for the letter (not the sender)
        String letterName = "Letter for " + String(capFirst(p.name));
        
        Serial.print("[MAIL] Letter for player: ");
        Serial.println(p.name);
        
        // Create world item for the letter
        WorldItem letter_item;
        letter_item.name = "letter";
        letter_item.parentName = "";
        letter_item.ownerName = "";  // Not owned - in room
        
        // Position at post office
        letter_item.x = p.roomX;
        letter_item.y = p.roomY;
        letter_item.z = p.roomZ;
        
        Serial.print("[MAIL] Position: (");
        Serial.print(letter_item.x);
        Serial.print(",");
        Serial.print(letter_item.y);
        Serial.print(",");
        Serial.print(letter_item.z);
        Serial.println(")");
        
        // Set attributes
        letter_item.attributes["type"] = std::string("letter");
        letter_item.attributes["name"] = std::string(letterName.c_str());
        letter_item.attributes["desc"] = std::string(letter.body.c_str());
        letter_item.attributes["subject"] = std::string(letter.subject.c_str());
        letter_item.attributes["from"] = std::string(letter.from.c_str());
        letter_item.attributes["weight"] = std::string("0");
        letter_item.attributes["value"] = std::string("0");
        
        // Add to world
        worldItems.push_back(letter_item);
        
        Serial.print("[MAIL] Letter item created and added to world - Total world items: ");
        Serial.println(worldItems.size());
    }
    
    Serial.println("========== POST OFFICE MAIL CHECK END ==========\n");
    return true;  // Mail was found and spawned
}

void updateDrunkennessRecovery(Player &p) {
    unsigned long now = millis();
    
    // Recover 1 drunkenness unit every 60 seconds (60000 ms)
    if (now - p.lastDrunkRecoveryCheck >= 60000UL) {
        p.lastDrunkRecoveryCheck = now;
        if (p.drunkenness < 6) {
            p.drunkenness++;
            if (p.drunkenness == 6) {
                p.client.println("\nYou feel the fog clearing from your mind. You're sober again!");
            }
        }
    }
}

void updateFullnessRecovery(Player &p) {
    unsigned long now = millis();
    
    // Recover 1 fullness unit every 60 seconds (60000 ms)
    if (now - p.lastFullnessRecoveryCheck >= 60000UL) {
        p.lastFullnessRecoveryCheck = now;
        if (p.fullness < 6) {
            p.fullness++;
            if (p.fullness == 6) {
                p.client.println("\nYour stomach feels empty again. You're hungry!");
            }
        }
    }
}

void showShopSign(Player &p, WorldItem &sign) {
    p.client.println("");

    struct Entry { String name; int price; int qty; };
    Entry entries[32];
    int entryCount = 0;

    // Collect unique item types
    for (auto &wi : worldItems) {
        if (wi.parentName != sign.name) continue;

        String itemName = wi.name;

        bool found = false;
        for (int i = 0; i < entryCount; i++) {
            if (entries[i].name == itemName) {
                found = true;
                break;
            }
        }
        if (found) continue;

        String v = wi.getAttr("value", itemDefs);
        int price = (v.length() > 0) ? v.toInt() : 0;
        if (price <= 0) continue;

        entries[entryCount].name = itemName;
        entries[entryCount].price = price;
        entries[entryCount].qty = 0;
        entryCount++;
    }

    // Count quantities
    for (int i = 0; i < entryCount; i++) {
        int qty = 0;
        for (auto &wi : worldItems) {
            if (wi.parentName == sign.name && wi.name == entries[i].name)
                qty++;
        }
        entries[i].qty = qty;
    }

    // Compute longest display name
    int longest = 0;
    String displayNames[32];

    for (int i = 0; i < entryCount; i++) {
        String disp = getItemDisplayName(entries[i].name);
        if (entries[i].qty > 1)
            disp += " (" + String(entries[i].qty) + ")";

        displayNames[i] = disp;

        if (disp.length() > longest)
            longest = disp.length();
    }

    // Print aligned lines
    for (int i = 0; i < entryCount; i++) {
        String line = "  " + displayNames[i];

        int dots = (longest - displayNames[i].length()) + 8;
        for (int d = 0; d < dots; d++)
            line += ".";

        line += " " + String(entries[i].price) + " gold";

        p.client.println(line);
    }
}


int findItemInShop(WorldItem &sign, const String &target) {
    // Normalize target
    String t = target;
    t.trim();
    t.toLowerCase();

    // Full-word match helper (captures only t)
    auto fullWordMatch = [&](const String &s) {
        if (s.length() == 0) return false;

        String lower = s;
        lower.toLowerCase();

        if (lower == t) return true;

        int start = 0;
        while (true) {
            int space = lower.indexOf(' ', start);
            String word = (space == -1)
                ? lower.substring(start)
                : lower.substring(start, space);

            word.trim();
            if (word == t) return true;

            if (space == -1) break;
            start = space + 1;
        }
        return false;
    };

    // Search shop inventory
    for (int i = 0; i < (int)worldItems.size(); i++) {
        WorldItem &wi = worldItems[i];

        if (wi.parentName != sign.name)
            continue;

        String id   = wi.name;
        String disp = getItemDisplayName(wi);

        if (fullWordMatch(id) || fullWordMatch(disp))
            return i;
    }

    return -1;
}



int findItemInInventory(Player &p, const String &target) {
    // Normalize target
    String t = target;
    t.trim();
    t.toLowerCase();

    // Full-word match helper (captures only t)
    auto fullWordMatch = [&](const String &s) {
        if (s.length() == 0) return false;

        String lower = s;
        lower.toLowerCase();

        if (lower == t) return true;

        int start = 0;
        while (true) {
            int space = lower.indexOf(' ', start);
            String word = (space == -1)
                ? lower.substring(start)
                : lower.substring(start, space);

            word.trim();
            if (word == t) return true;

            if (space == -1) break;
            start = space + 1;
        }
        return false;
    };

    // Search inventory
    for (int i = 0; i < p.invCount; i++) {
        int worldIndex = p.invIndices[i];
        if (worldIndex < 0 || worldIndex >= (int)worldItems.size())
            continue;

        WorldItem &wi = worldItems[worldIndex];

        String id   = wi.name;
        String disp = getItemDisplayName(wi);

        if (fullWordMatch(id) || fullWordMatch(disp))
            return worldIndex;
    }

    return -1;
}





int findInventoryItemIndexForShop(Player &p, const String &targetRaw) {
    String t = targetRaw;
    t.trim();
    t.toLowerCase();

    for (int i = 0; i < p.invCount; i++) {
        int idx = p.invIndices[i];
        if (idx < 0 || idx >= (int)worldItems.size()) continue;

        WorldItem &wi = worldItems[idx];

        // Never sell gold piles
        if (wi.name == "gold_coin")
            continue;

        // Never sell immobile items
        String mob = wi.getAttr("mobile", itemDefs);
        if (mob == "0")
            continue;

        String id   = wi.name;
        String disp = getItemDisplayName(wi);

        // Case-insensitive
        id.toLowerCase();
        disp.toLowerCase();

        // Exact match
        if (id == t || disp == t)
            return idx;

        // Partial / word match (same logic as GET)
        if (fullWordMatch(t, id) || fullWordMatch(t, disp))
            return idx;
        
        // Substring match (for commands like "sell sword" matching "Rusty Shortsword")
        if (id.indexOf(t) != -1 || disp.indexOf(t) != -1)
            return idx;
    }

    return -1;
}

bool giveItemToPlayer(Player &p, int worldIndex) {
    // NEVER allow gold coins in inventory
    if (worldIndex >= 0 && worldIndex < (int)worldItems.size()) {
        if (worldItems[worldIndex].name == "gold_coin") {
            return false;  // Reject gold coins from inventory
        }
    }
    
    if (p.invCount >= 32) return false;

    p.invIndices[p.invCount++] = worldIndex;
    worldItems[worldIndex].ownerName = p.name;
    worldItems[worldIndex].parentName = "";

    return true;
}


Player* findPlayerInRoom(Player &p, const String &targetName) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!players[i].active) continue;
        if (&players[i] == &p) continue;

        if (players[i].roomX == p.roomX &&
            players[i].roomY == p.roomY &&
            players[i].roomZ == p.roomZ &&
            targetName.equalsIgnoreCase(players[i].name)) {
            return &players[i];
        }
    }
    return nullptr;
}


bool itemNameMatches(const String &input, const String &display) {
    String a = input;
    String b = display;

    a.toLowerCase();
    b.toLowerCase();

    // exact match
    if (a == b) return true;

    // prefix match
    if (b.startsWith(a)) return true;

    // substring match anywhere
    if (b.indexOf(a) >= 0) return true;

    return false;
}

int findItemInInventoryFuzzy(Player &p, const String &input) {
    for (int i = 0; i < p.invCount; i++) {
        int idxWorld = p.invIndices[i];
        if (idxWorld < 0 || idxWorld >= (int)worldItems.size()) continue;

        WorldItem &wi = worldItems[idxWorld];
        String disp = getItemDisplayName(wi);

        if (itemNameMatches(input, disp))
            return idxWorld;
    }
    return -1;
}


void cmdGive(Player &p, const String &args) {
    int idxTo = args.indexOf(" to ");
    if (idxTo < 0) {
        p.client.println("Give what to whom?");
        return;
    }

    String itemName = args.substring(0, idxTo);
    String targetName = args.substring(idxTo + 4);
    itemName.trim();
    targetName.trim();

    if (itemName.length() == 0 || targetName.length() == 0) {
        p.client.println("Give what to whom?");
        return;
    }

    // ---------------------------------------------------------
    // 1. Resolve target
    // ---------------------------------------------------------
    Player* tp = nullptr;
    NpcInstance* tn = nullptr;

    // Players first
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player &other = players[i];
        if (!other.active) continue;
        if (&other == &p) continue;

        if (other.roomX == p.roomX &&
            other.roomY == p.roomY &&
            other.roomZ == p.roomZ) {

            if (npcNameMatches(other.name, targetName)) {
                tp = &other;
                break;
            }
        }
    }

    // NPCs second
    if (!tp) {
        for (auto &n : npcInstances) {
            if (!n.alive || n.hp <= 0) continue;

            if (n.x == p.roomX && n.y == p.roomY && n.z == p.roomZ) {
                std::string key = n.npcId.c_str();
                auto it = npcDefs.find(key);
                if (it == npcDefs.end()) continue;

                auto &def = it->second;
                String npcName = def.attributes.at("name").c_str();

                if (npcNameMatches(npcName, targetName)) {
                    tn = &n;
                    break;
                }
            }
        }
    }

    if (!tp && !tn) {
        p.client.println("They aren't here.");
        return;
    }

    if (tp == &p) {
        p.client.println("You can't give something to yourself.");
        return;
    }

    // ---------------------------------------------------------
    // 2. COIN GIVE LOGIC
    // ---------------------------------------------------------
    {
        String lower = itemName;
        lower.toLowerCase();

        bool wantsCoins = false;
        int amount = -1;

        if (lower == "coin" || lower == "coins")
            wantsCoins = true;

        int spacePos = lower.indexOf(' ');
        if (spacePos > 0) {
            String first = lower.substring(0, spacePos);
            String rest  = lower.substring(spacePos + 1);

            if (rest == "coin" || rest == "coins") {
                int amt = first.toInt();
                if (amt > 0) {
                    wantsCoins = true;
                    amount = amt;
                }
            }
        }

        if (wantsCoins && amount == -1)
            amount = p.coins;

        if (wantsCoins) {
            if (tn) {
                p.client.println("They have no use for coins.");
                return;
            }

            if (!tp) {
                p.client.println("They aren't here.");
                return;
            }

            if (amount <= 0) {
                p.client.println("Give what?");
                return;
            }

            if (amount > p.coins) {
                p.client.println("You don't have that many coins.");
                return;
            }

            p.coins -= amount;
            tp->coins += amount;

            if (amount == 1) {
                p.client.println("You give 1 gold coin to " + String(tp->name) + ".");
                tp->client.println(String(p.name) + " gives you 1 gold coin.");
            } else {
                p.client.println("You give " + String(amount) + " gold coins to " + String(tp->name) + ".");
                tp->client.println(String(p.name) + " gives you " + String(amount) + " gold coins.");
            }

            return;
        }
    }

    // ---------------------------------------------------------
    // 3. NORMAL ITEM GIVE
    // ---------------------------------------------------------
    int idx = findItemInInventoryFuzzy(p, itemName);
    if (idx < 0 || idx >= (int)worldItems.size()) {
        p.client.println("You aren't carrying that.");
        return;
    }

    WorldItem &wi = worldItems[idx];

    // NPCs can receive items for QUESTS, check first
    if (tn) {
        // Try quest completion with the NPC
        if (onQuestEvent(p, "give", wi.name, tn->npcId, "", 0, 0, 0)) {
            // Quest step was completed!
            // Remove from inventory
            removeFromInventory(p, idx);
            
            // NPC takes the item (mark as consumed)
            wi.ownerName = "quest";
            
            // Don't print message - quest system handles all output
            return;
        }
        
        // Not a quest item, NPC won't take it
        p.client.println("They have no use for that.");
        return;
    }

    // Prevent giving duplicates
    if (playerHasItem(*tp, wi.name)) {
        p.client.println(String(tp->name) + " already has one of those.");
        return;
    }

    // Remove from inventory
    removeFromInventory(p, idx);

    // Transfer ownership
    wi.ownerName = tp->name;
    wi.parentName = "";

    p.client.println("You give " + getItemDisplayName(wi) +
                     " to " + String(tp->name) + ".");
    tp->client.println(String(p.name) + " gives you " +
                       getItemDisplayName(wi) + ".");
}


// =============================================================
// CONTAINER COMMANDS
// get <item> from <container>
// get all from <container>
// put <item> in <container>
// =============================================================

// Find a top-level container either on floor or in inventory


int findContainer(Player &p, const char* containerStr, bool &isInInventory) {
    String target = String(containerStr);
    target.trim();
    target.toLowerCase();

    // -----------------------------------------
    // Helper: full-word + partial match
    // -----------------------------------------
    auto matches = [&](const String &s) {
        if (s.length() == 0) return false;

        String lower = s;
        lower.toLowerCase();

        // Exact match
        if (lower == target) return true;

        // Partial substring match
        if (lower.indexOf(target) != -1) return true;

        // Full-word match
        int start = 0;
        while (true) {
            int space = lower.indexOf(' ', start);
            String word = (space == -1)
                ? lower.substring(start)
                : lower.substring(start, space);

            word.trim();
            if (word == target) return true;

            if (space == -1) break;
            start = space + 1;
        }

        return false;
    };

    // -----------------------------------------
    // 1. SEARCH INVENTORY FIRST
    // -----------------------------------------
    rebuildPlayerInventory(p);

    for (int i = 0; i < p.invCount; i++) {
        int idx = p.invIndices[i];
        if (idx < 0 || idx >= (int)worldItems.size()) continue;

        WorldItem &wi = worldItems[idx];

        // Display name
        String disp = getItemDisplayName(wi);
        if (matches(disp)) {
            isInInventory = true;
            return idx;
        }

        // Template name fallback
        String tname = wi.getAttr("name", itemDefs);
        if (tname.length() == 0) tname = wi.name;
        if (matches(tname)) {
            isInInventory = true;
            return idx;
        }
    }

    // -----------------------------------------
    // 2. SEARCH ROOM (top-level only)
    // -----------------------------------------
    Room &r = p.currentRoom;

    for (int i = 0; i < (int)worldItems.size(); i++) {
        WorldItem &wi = worldItems[i];

        // Must be top-level in room
        if (wi.ownerName.length() != 0) continue;
        if (wi.parentName.length() != 0) continue;
        if (wi.x != r.x || wi.y != r.y || wi.z != r.z) continue;

        // Display name
        String disp = getItemDisplayName(wi);
        if (matches(disp)) {
            isInInventory = false;
            return i;
        }

        // Template name fallback
        String tname = wi.getAttr("name", itemDefs);
        if (tname.length() == 0) tname = wi.name;
        if (matches(tname)) {
            isInInventory = false;
            return i;
        }
    }

    return -1;
}




// Wrapper for cmdGet
void cmdGet(Player &p, const char *input) {
    cmdGet(p, String(input));
}

// =============================================================
// get <item> from <container>
// =============================================================


void cmdGetFrom(Player &p, const String &input) {
    // input is "item from container"
    int pos = input.indexOf(" from ");
    if (pos == -1) {
        p.client.println("Get what from what.");
        return;
    }

    String itemName = input.substring(0, pos);
    String containerName = input.substring(pos + 6);

    itemName.trim();
    containerName.trim();

    int containerIndex = resolveItem(p, containerName);
    if (containerIndex == -1) {
        p.client.println("You don't see that here.");
        return;
    }

    WorldItem *cPtr = getResolvedWorldItem(p, containerIndex);
    if (!cPtr) {
        p.client.println("You don't see that here.");
        return;
    }

    WorldItem &container = *cPtr;

    String c1 = container.getAttr("container", itemDefs);
    String c2 = container.getAttr("can_contain", itemDefs);

    if (!(c1 == "1" || c2 == "1")) {
        p.client.println("You can't get things from that.");
        return;
    }

    int childIndex = resolveItemInContainer(p, container, itemName);
    if (childIndex < 0 || childIndex >= (int)worldItems.size()) {
        p.client.println("You don't see that in there.");
        return;
    }

    if (p.invCount >= 32) {
        p.client.println("You can't carry any more.");
        return;
    }

    // Check weight limit
    int itemWeight = getWeight(worldItems[childIndex]);
    int currentWeight = getCarriedWeight(p);
    int maxWeight = getMaxCarryWeight(p);
    
    if (currentWeight + itemWeight > maxWeight) {
        p.client.println("You cannot carry any more.");
        return;
    }

    // Remove from container
    for (int i = 0; i < (int)container.children.size(); i++) {
        if (container.children[i] == childIndex) {
            container.children.erase(container.children.begin() + i);
            break;
        }
    }

    WorldItem &child = worldItems[childIndex];

    // NEVER allow gold coins in inventory
    if (child.name == "gold_coin") {
        p.client.println("You can't take coins from a container.");
        return;
    }

    // *** FIXED: item enters inventory with correct coordinates ***
    child.parentName = p.name;
    child.ownerName = p.name;
    child.x = p.roomX;
    child.y = p.roomY;
    child.z = p.roomZ;

    p.invIndices[p.invCount++] = childIndex;

    p.client.println("You take " + getItemDisplayName(child) +
                     " from " + getItemDisplayName(container) + ".");
}





// =============================================================
// get all
// =============================================================


void cmdGetAll(Player &p) {
    bool gotAny = false;
    int maxWeight = getMaxCarryWeight(p);

    int i = 0;
    while (i < (int)worldItems.size()) {
        WorldItem &wi = worldItems[i];

        // Must be in room and not owned
        if (wi.ownerName.length() != 0 ||
            wi.x != p.roomX || wi.y != p.roomY || wi.z != p.roomZ)
        {
            i++;
            continue;
        }

        // Skip invisible items
        if (wi.getAttr("invisible", itemDefs) == "1") {
            i++;
            continue;
        }

        // ⭐ COIN SPECIAL CASE
        if (wi.name == "gold_coin") {
            if (!gotAny) {
                p.client.println("You pick up:");
                gotAny = true;
            }

            if (wi.value == 1)
                p.client.println("  1 gold coin");
            else
                p.client.println("  " + String(wi.value) + " gold coins");
            p.coins += wi.value;

            worldItems.erase(worldItems.begin() + i);
            continue; // do NOT increment i
        }

        // Check inventory count limit
        if (p.invCount >= 32) {
            p.client.println("You can't carry any more.");
            break;
        }

        // Check weight limit
        int itemWeight = getWeight(wi);
        int currentWeight = getCarriedWeight(p);
        if (currentWeight + itemWeight > maxWeight) {
            p.client.println("You can't carry any more.");
            break;
        }

        wi.ownerName = p.name;
        wi.parentName = p.name;
        wi.x = wi.y = wi.z = -1;

        p.invIndices[p.invCount++] = i;

        if (!gotAny) {
            p.client.println("You pick up:");
            gotAny = true;
        }

        p.client.println("  " + getItemDisplayName(wi));

        i++;
    }

    if (!gotAny)
        p.client.println("There is nothing here to take.");
}


void cmdGetAllFrom(Player &p, const String &containerName) {
    // Resolve container (room or inventory)
    int containerIndex = resolveItem(p, containerName);
    if (containerIndex == -1) {
        p.client.println("You don't see that here.");
        return;
    }

    WorldItem *cPtr = getResolvedWorldItem(p, containerIndex);
    if (!cPtr) {
        p.client.println("You don't see that here.");
        return;
    }

    WorldItem &container = *cPtr;

    // Must be a container
    String c1 = container.getAttr("container", itemDefs);
    String c2 = container.getAttr("can_contain", itemDefs);
    if (!(c1 == "1" || c2 == "1")) {
        p.client.println("You can't get things from that.");
        return;
    }

    if (container.children.size() == 0) {
        p.client.println("It is empty.");
        return;
    }

    bool gotAny = false;
    int maxWeight = getMaxCarryWeight(p);

    // Iterate children safely
    for (int i = 0; i < (int)container.children.size(); ) {
        int wiIndex = container.children[i];
        if (wiIndex < 0 || wiIndex >= (int)worldItems.size()) {
            container.children.erase(container.children.begin() + i);
            continue;
        }

        WorldItem &item = worldItems[wiIndex];

        // Skip invisible items
        if (item.getAttr("invisible", itemDefs) == "1") {
            i++;
            continue;
        }

        // ⭐ Skip coins — coins never go into inventory
        if (item.name == "gold_coin") {
            i++;
            continue;
        }

        // Inventory full?
        if (p.invCount >= 32) {
            p.client.println("You can't carry any more.");
            break;
        }

        // Check weight limit
        int itemWeight = getWeight(item);
        int currentWeight = getCarriedWeight(p);
        if (currentWeight + itemWeight > maxWeight) {
            p.client.println("You can't carry any more.");
            break;
        }

        // Remove from container
        container.children.erase(container.children.begin() + i);

        // Move to inventory
        item.ownerName = p.name;
        item.parentName = p.name;
        item.x = item.y = item.z = -1;

        p.invIndices[p.invCount++] = wiIndex;

        if (!gotAny) {
            p.client.println("You take:");
            gotAny = true;
        }

        p.client.println("  " + getItemDisplayName(item));
    }

    if (!gotAny)
        p.client.println("There is nothing you can take.");
}


void cmdPutIn(Player &p, const String &input) {
    // input is "<item> in <container>"
    int pos = input.indexOf(" in ");
    if (pos == -1) {
        p.client.println("Put what in what?");
        return;
    }

    String itemName = input.substring(0, pos);
    String containerName = input.substring(pos + 4);

    itemName.trim();
    containerName.trim();

    // ---------------------------------------------------------
    // 1. Find item in inventory
    // ---------------------------------------------------------
    int invSlot = -1;
    int itemIndex = -1;

    for (int i = 0; i < p.invCount; i++) {
        int wiIndex = p.invIndices[i];
        if (wiIndex < 0 || wiIndex >= (int)worldItems.size()) continue;

        WorldItem &wi = worldItems[wiIndex];

        if (nameMatches(wi.name, itemName)) {
            invSlot = i;
            itemIndex = wiIndex;
            break;
        }
    }

    if (itemIndex < 0) {
        p.client.println("You aren't carrying that.");
        return;
    }

    // ---------------------------------------------------------
    // 2. Resolve container (room or inventory)
    // ---------------------------------------------------------
    int containerIndex = resolveItem(p, containerName);
    if (containerIndex == -1) {
        p.client.println("You don't see that here.");
        return;
    }

    WorldItem *cPtr = getResolvedWorldItem(p, containerIndex);
    if (!cPtr) {
        p.client.println("You don't see that here.");
        return;
    }

    WorldItem &container = *cPtr;


    // A container cannot be placed into a container
    String cc = worldItems[itemIndex].getAttr("can_contain", itemDefs);
    if (cc == "1") {
        // item is a container
    }



    // ---------------------------------------------------------
    // 3. Validate container type
    // ---------------------------------------------------------
    String c1 = container.getAttr("container", itemDefs);
    String c2 = container.getAttr("can_contain", itemDefs);

    if (!(c1 == "1" || c2 == "1")) {
        p.client.println("You can't put things in that.");
        return;
    }


    // reject: containers cannot go inside containers except weapons that are containers
    String t = worldItems[itemIndex].getAttr("type", itemDefs);  //exclude type=container
    if((t == "container")) {
        p.client.println("Things that contain, cannot be contained!");
        return;
    }


    // ---------------------------------------------------------
    // 5. Prevent putting coins into containers
    // ---------------------------------------------------------
    String itemNameLower = worldItems[itemIndex].name;
    itemNameLower.toLowerCase();
    if (itemNameLower.indexOf("gold") != -1 && worldItems[itemIndex].value > 0) {
        p.client.println("Gold coins go directly to your coin purse, not containers.");
        return;
    }

    // ---------------------------------------------------------
    // 6. Remove item from inventory
    // ---------------------------------------------------------
    for (int i = invSlot; i < p.invCount - 1; i++)
        p.invIndices[i] = p.invIndices[i + 1];

    p.invCount--;

    // ---------------------------------------------------------
    // 7. Move item into container
    // ---------------------------------------------------------
    WorldItem &item = worldItems[itemIndex];

    item.parentName = container.name;
    item.ownerName = p.name;  // Keep owner as player so it persists in inventory
    item.x = item.y = item.z = -1;

    container.children.push_back(itemIndex);

    // ---------------------------------------------------------
    // 8. Output
    // ---------------------------------------------------------
    p.client.println("You put " +
                     getItemDisplayName(item) +
                     " in " +
                     getItemDisplayName(container) + ".");
}



void cmdPutAll(Player &p, const String &input) {
    String args = input;
    args.trim();

    int inPos = args.indexOf(" in ");
    if (inPos < 0) {
        p.client.println("Put what in what?");
        return;
    }

    String what = args.substring(0, inPos);
    String containerName = args.substring(inPos + 4);

    what.trim();
    containerName.trim();

    // Must be "all"
    if (!what.equalsIgnoreCase("all")) {
        p.client.println("If you want to put everything, use: put all in <container>.");
        return;
    }

    // ---------------------------------------------------------
    // Resolve container
    // ---------------------------------------------------------
    int containerIndex = resolveItem(p, containerName);
    if (containerIndex == -1) {
        p.client.println("You don't see that here.");
        return;
    }

    WorldItem *cPtr = getResolvedWorldItem(p, containerIndex);
    if (!cPtr) {
        p.client.println("You don't see that here.");
        return;
    }

    WorldItem &container = *cPtr;

    // Get the actual world index of the container for comparison
    int actualContainerIndex = -1;
    if (containerIndex & 0x80000000) {
        // Container is in inventory
        int slot = (containerIndex & 0x7FFFFFFF);
        if (slot >= 0 && slot < p.invCount) {
            actualContainerIndex = p.invIndices[slot];
        }
    } else {
        // Container is in room or worn
        actualContainerIndex = containerIndex;
    }

    // Must be a container
    String c1 = container.getAttr("container", itemDefs);
    String c2 = container.getAttr("can_contain", itemDefs);

    if (!(c1 == "1" || c2 == "1")) {
        p.client.println("You can't put things in that.");
        return;
    }

    bool movedAny = false;

    // ---------------------------------------------------------
    // Iterate inventory safely
    // ---------------------------------------------------------
    int i = 0;
    while (i < p.invCount) {
        int wiIndex = p.invIndices[i];
        if (wiIndex < 0 || wiIndex >= (int)worldItems.size()) {
            i++;
            continue;
        }

        WorldItem &item = worldItems[wiIndex];

        // Skip coins - check if name contains 'gold' and has value
        String nameLower = item.name;
        nameLower.toLowerCase();
        if (nameLower.indexOf("gold") != -1 && item.value > 0) {
            i++;
            continue;
        }

        // Skip putting container inside itself
        if (wiIndex == actualContainerIndex) {
            i++;
            continue;
        }

        // Remove from inventory
        for (int j = i; j < p.invCount - 1; j++)
            p.invIndices[j] = p.invIndices[j + 1];
        p.invCount--;

        // Move item into container
        item.parentName = container.name;
        item.ownerName = p.name;  // Keep owner as player so it persists in inventory
        item.x = item.y = item.z = -1;

        container.children.push_back(wiIndex);
        movedAny = true;

        // Do NOT increment i — inventory shifted left
    }

    if (!movedAny)
        p.client.println("You have nothing to put there.");
    else
        p.client.println("You put everything into " +
                         getItemDisplayName(container) + ".");
}




// =============================================================
// DROP <item>  OR  DROP <n> COINS
// =============================================================

// drop <item>
// Wrapper (goes first)
void cmdDrop(Player &p, const char *input) {
    cmdDrop(p, String(input));
}


// =============================================================
// DROP <item>   OR   DROP <n> COINS
// =============================================================

void cmdDrop(Player &p, const String &input) {

    String raw = input;
    raw.trim();

    // -----------------------------------------
    // SPECIAL CASE: DROP <n> COINS
    // -----------------------------------------
    {
        String lower = raw;
        lower.toLowerCase();

        // If the input contains "coin" anywhere, treat it as coin logic
        if (lower.indexOf("coin") >= 0) {

            // Extract the number before "coin"
            int spacePos = lower.indexOf(' ');
            int amount = 0;

            if (spacePos > 0) {
                String num = lower.substring(0, spacePos);
                amount = num.toInt();
            }

            // If no number given, default to ALL coins
            if (amount <= 0) {
                p.client.println("Drop how many coins.");
                return;
            }

            if (amount > p.coins) {
                p.client.println("You don't have that many coins.");
                return;
            }

            p.coins -= amount;

            WorldItem wi;
            wi.name = "gold_coin";
            wi.x = p.roomX;
            wi.y = p.roomY;
            wi.z = p.roomZ;
            wi.ownerName = "";
            wi.parentName = "";
            wi.value = amount;

            worldItems.push_back(wi);

            if (amount == 1)
                p.client.println("You drop 1 gold coin.");
            else
                p.client.println("You drop " + String(amount) + " gold coins.");
            return;
        }
    }

    // -----------------------------------------
    // NORMAL ITEM DROP (partial-name match)
    // -----------------------------------------
    String arg = raw;
    int spacePos = arg.indexOf(' ');
    if (spacePos > 0) {
        arg = arg.substring(0, spacePos);
    }

    String lower = arg;
    lower.toLowerCase();

    int invSlot = -1;
    int worldIndex = -1;

    for (int i = 0; i < p.invCount; i++) {
        int idx = p.invIndices[i];
        if (idx < 0) continue;

        WorldItem &wi = worldItems[idx];

        String id   = wi.name;
        String disp = getItemDisplayName(wi);

        String idLower   = id;   idLower.toLowerCase();
        String dispLower = disp; dispLower.toLowerCase();

        if (idLower.indexOf(lower) >= 0 || dispLower.indexOf(lower) >= 0) {
            invSlot = i;
            worldIndex = idx;
            break;
        }
    }

    if (worldIndex < 0) {
        p.client.println("You aren't carrying that.");
        return;
    }

    WorldItem &wi = worldItems[worldIndex];
    
    // Check if we're in a special room (RECYCLING CENTER)
    if (isRecyclingCenter(p.currentRoom)) {
        // Remove from inventory
        p.invIndices[invSlot] = p.invIndices[p.invCount - 1];
        p.invCount--;
        
        // Recycle the item
        recycleItem(p, worldIndex);
        return;
    }
    
    // Normal drop behavior
    wi.ownerName = "";
    wi.parentName = "";
    wi.x = p.roomX;
    wi.y = p.roomY;
    wi.z = p.roomZ;

    p.invIndices[invSlot] = p.invIndices[p.invCount - 1];
    p.invCount--;

    p.client.println("You drop " + getItemDisplayName(wi) + ".");
}






void cmdDropAll(Player &p, const char *unused) {
    cmdDropAll(p);
}


void cmdDropAll(Player &p) {
    bool droppedAny = false;
    int totalCoins = 0;
    bool inRecyclingCenter = isRecyclingCenter(p.currentRoom);

    // ---------------------------------------------------------
    // 1. Iterate inventory safely
    // ---------------------------------------------------------
    int i = 0;
    while (i < p.invCount) {
        int wiIndex = p.invIndices[i];
        if (wiIndex < 0 || wiIndex >= (int)worldItems.size()) {
            i++;
            continue;
        }

        WorldItem &item = worldItems[wiIndex];

        // ⭐ COIN SPECIAL CASE
        if (item.name == "gold_coin") {
            totalCoins += item.value;

            // Remove coin from inventory
            for (int j = i; j < p.invCount - 1; j++)
                p.invIndices[j] = p.invIndices[j + 1];
            p.invCount--;

            continue; // do NOT increment i
        }

        // Check if in special room (RECYCLING CENTER)
        if (inRecyclingCenter) {
            // Show appreciation message once per dropall
            if (!droppedAny) {
                p.client.println("We appreciate you being eco-conscious!");
                droppedAny = true;
            }
            
            // Each item gets its own "wisks away" message (showAppreciationMsg=false)
            recycleItem(p, wiIndex, false);
            
            // Remove from inventory
            for (int j = i; j < p.invCount - 1; j++)
                p.invIndices[j] = p.invIndices[j + 1];
            p.invCount--;
            
            continue; // do NOT increment i
        }

        // Normal item drop
        item.ownerName = "";
        item.parentName = "";
        item.x = p.roomX;
        item.y = p.roomY;
        item.z = p.roomZ;

        if (!droppedAny) {
            p.client.println("You drop everything.");
            droppedAny = true;
        }

        // Remove from inventory
        for (int j = i; j < p.invCount - 1; j++)
            p.invIndices[j] = p.invIndices[j + 1];
        p.invCount--;

        // Do NOT increment i — inventory shifted left
    }

    // ---------------------------------------------------------
    // 2. Drop merged coin pile (if any)
    // ---------------------------------------------------------
    if (totalCoins > 0) {
        WorldItem wi;
        wi.name = "gold_coin";
        wi.value = totalCoins;
        wi.ownerName = "";
        wi.parentName = "";
        wi.x = p.roomX;
        wi.y = p.roomY;
        wi.z = p.roomZ;

        worldItems.push_back(wi);

        // Merge with any existing piles
        mergeCoinPilesAt(p.roomX, p.roomY, p.roomZ);

        if (!droppedAny) {
            p.client.println("You drop everything.");
            droppedAny = true;
        }
    }

    if (!droppedAny)
        p.client.println("You have nothing to drop.");
}



void cmdDestroy(Player &p, const String &input) {
    if (!p.IsWizard) {
        p.client.println("You lack the power to do that.");
        return;
    }

    String arg = input;
    arg.trim();

    if (arg.length() == 0) {
        p.client.println("Destroy what?");
        return;
    }

    int idx = resolveItem(p, arg);
    if (idx == -1) {
        p.client.println("You don't see that here.");
        return;
    }

    WorldItem *wiPtr = getResolvedWorldItem(p, idx);
    if (!wiPtr) {
        p.client.println("You don't see that here.");
        return;
    }

    int worldIndex = -1;

    // Decode index
    if (idx & 0x80000000) {
        int slot = (idx & 0x7FFFFFFF);
        if (slot < 0 || slot >= p.invCount) {
            p.client.println("You don't see that here.");
            return;
        }
        worldIndex = p.invIndices[slot];

        // Remove from inventory
        for (int i = slot; i < p.invCount - 1; i++) {
            p.invIndices[i] = p.invIndices[i + 1];
        }
        p.invCount--;
    } else {
        worldIndex = idx;
    }

    if (worldIndex < 0 || worldIndex >= (int)worldItems.size()) {
        p.client.println("You don't see that here.");
        return;
    }

    WorldItem &wi = worldItems[worldIndex];

    p.client.println("You destroy " + getItemDisplayName(wi) + ".");

    // Remove this item from any container.children
    for (auto &other : worldItems) {
        for (int i = 0; i < (int)other.children.size(); i++) {
            if (other.children[i] == worldIndex) {
                other.children.erase(other.children.begin() + i);
                break;
            }
        }
    }

    // Mark as removed: simplest is to move it off-map and clear names
    wi.name = "";
    wi.ownerName = "";
    wi.parentName = "";
    wi.x = wi.y = wi.z = -9999;
    wi.children.clear();
}



void cmdDropFrom(Player &p, const String &input) {
    String args = input;
    args.trim();

    int fromPos = args.indexOf(" from ");
    if (fromPos < 0) {
        p.client.println("Drop what from what?");
        return;
    }

    String itemName = args.substring(0, fromPos);
    String containerName = args.substring(fromPos + 6);
    itemName.trim();
    containerName.trim();

    int containerIndex = resolveItem(p, containerName);
    if (containerIndex == -1) {
        p.client.println("You don't see that here.");
        return;
    }

    WorldItem *cPtr = getResolvedWorldItem(p, containerIndex);
    if (!cPtr) {
        p.client.println("You don't see that here.");
        return;
    }

    WorldItem &container = *cPtr;

    if (container.getAttr("container", itemDefs) != "1") {
        p.client.println("You can't drop things from that.");
        return;
    }

    int childIndex = resolveItemInContainer(p, container, itemName);
    if (childIndex < 0 || childIndex >= (int)worldItems.size()) {
        p.client.println("You don't see that in there.");
        return;
    }

    for (int i = 0; i < (int)container.children.size(); i++) {
        if (container.children[i] == childIndex) {
            container.children.erase(container.children.begin() + i);
            break;
        }
    }

    WorldItem &child = worldItems[childIndex];

    child.parentName = "";
    child.ownerName = "";
    child.x = p.roomX;
    child.y = p.roomY;
    child.z = p.roomZ;

    p.client.println("You drop " + getItemDisplayName(child) +
                     " from " + getItemDisplayName(container) + ".");
}






// =============================================================
// WEAR / WIELD (parent/child aware)
// =============================================================

// Helper: can an item with children be worn/wielded?
bool canWearOrWield(WorldItem &wi) {
    // If item has children, check attribute "allow_children"
    std::vector<int> kids;
    getChildrenOf(wi.name, kids);

    if (kids.empty()) return true; // no children → always allowed

    // If children exist, item must explicitly allow it
    String allow = wi.getAttr("allow_children", itemDefs);
    allow.toLowerCase();
    return (allow == "true" || allow == "1" || allow == "yes");
}



void markQuestStep(Player &p, const QuestStep &step);
void checkQuestCompletion(Player &p, int questId);
int questIdToIndex(int questId);
int stepToIndex(int step);
void unloadQuestNpc(const String &npcId);


// =============================================================
// WEAR <item>/ REMOVE <item>
// =============================================================


void cmdWear(Player &p, const String &input) {
    String arg = input;
    arg.trim();

    if (arg.length() == 0) {
        p.client.println("Wear what?");
        return;
    }

    // Find item in inventory
    int idx = -1;
    for (int i = 0; i < p.invCount; i++) {
        int wi = p.invIndices[i];
        if (isMatch(worldItems[wi].name.c_str(), arg.c_str())) {
            idx = wi;
            break;
        }
    }

    if (idx < 0) {
        p.client.println("You aren't carrying that.");
        return;
    }

    WorldItem &wi = worldItems[idx];

    // SAFE lookup
    std::string key = std::string(wi.name.c_str());
    auto it = itemDefs.find(key);
    if (it == itemDefs.end()) {
        p.client.println("You can't wear that.");
        return;
    }

    auto &attrs = it->second.attributes;

    // Must be armor
    auto typeIt = attrs.find("type");
    if (typeIt == attrs.end() || typeIt->second != "armor") {
        p.client.println("You can't wear that.");
        return;
    }

    // Must have slot
    auto slotIt = attrs.find("slot");
    if (slotIt == attrs.end()) {
        p.client.println("You can't wear that.");
        return;
    }

    int slotIndex = atoi(slotIt->second.c_str());
    if (slotIndex < 0 || slotIndex >= SLOT_COUNT) {
        p.client.println("You can't wear that.");
        return;
    }

    // Remove existing item in slot
    if (p.wornItemIndices[slotIndex] != -1) {
        p.client.println(
            "You remove " +
            getItemDisplayName(worldItems[p.wornItemIndices[slotIndex]]) +
            "."
        );
    }

    // Wear new item
    p.wornItemIndices[slotIndex] = idx;

    p.client.println("You wear " + getItemDisplayName(wi) + ".");

    applyEquipmentBonuses(p);
}

void cmdRemove(Player &p, const String &input) {
    String arg = input;
    arg.trim();

    if (arg.length() == 0) {
        p.client.println("Remove what?");
        return;
    }

    for (int s = 0; s < SLOT_COUNT; s++) {
        int idx = p.wornItemIndices[s];
        if (idx == -1) continue;

        WorldItem &wi = worldItems[idx];

        if (isMatch(wi.name.c_str(), arg.c_str())) {
            p.client.println("You remove " + getItemDisplayName(wi) + ".");
            p.wornItemIndices[s] = -1;
            applyEquipmentBonuses(p);
            return;
        }
    }

    p.client.println("You aren't wearing that.");
}



// =============================================================
// WIELD <item>
// =============================================================

void cmdWield(Player &p, const String &input) {
    String arg = input;
    arg.trim();

    if (arg.length() == 0) {
        p.client.println("Wield what?");
        return;
    }

    // Use findInventoryItemIndex which properly checks display names
    int idx = findInventoryItemIndex(p, arg.c_str());

    if (idx < 0) {
        p.client.println("You aren't carrying that.");
        return;
    }

    WorldItem &wi = worldItems[idx];

    // SAFE lookup
    std::string key = wi.name.c_str();
    auto it = itemDefs.find(key);
    if (it == itemDefs.end()) {
        p.client.println("You can't wield that.");
        return;
    }

    // Check if item type is "weapon"
    std::string itemType = it->second.type;
    if (itemType != "weapon") {
        p.client.println("You can't wield that.");
        return;
    }

    if (p.wieldedItemIndex != -1) {
        p.client.println(
            "You stop wielding " +
            getItemDisplayName(worldItems[p.wieldedItemIndex]) +
            "."
        );
    }

    p.wieldedItemIndex = idx;

    p.client.println("You wield " + getItemDisplayName(wi) + ".");

    applyEquipmentBonuses(p);
}


// =============================================================
// UNWIELD
// =============================================================

void cmdUnwield(Player &p) {
    if (p.wieldedItemIndex == -1) {
        p.client.println("You aren't wielding anything.");
        return;
    }

    WorldItem &wi = worldItems[p.wieldedItemIndex];
    p.client.println("You stop wielding " + getItemDisplayName(wi) + ".");
    p.wieldedItemIndex = -1;

    applyEquipmentBonuses(p);
}



// =============================
// Combat utilities
// =============================

void broadcastToRoom(int x, int y, int z, const String &msg, Player *exclude) {
  for (int i = 0; i < MAX_PLAYERS; i++) {
    if (!players[i].active) continue;
    if (&players[i] == exclude) continue;
    if (players[i].roomX == x && players[i].roomY == y && players[i].roomZ == z) {
      players[i].client.println(msg);
    }
  }
}

void handlePlayerDeath(Player &p) {

    // Flavorful death messages
    static const char* deathMsgs[] = {
        "You collapse to the ground, lifeless.",
        "Your vision fades as darkness takes you.",
        "You fall in battle, defeated.",
        "Your body goes limp as you die.",
        "You have been slain!"
    };

    // Dramatic death line
    String msg = deathMsgs[random(5)];
    p.client.println(msg);
    delay(2000);

    broadcastRoomExcept(
        p,
        capFirst(p.name) + " has been slain!",
        p
    );

    // XP penalty: lose 1/3 XP
    long lostXP = p.xp / 3;
    p.xp -= lostXP;
    if (p.xp < 0) p.xp = 0;

    p.client.println("You lose " + String(lostXP) + " experience points!");
    delay(2000);

    // Prevent negative HP
    p.hp = 0;

    // End combat
    p.inCombat = false;
    p.combatTarget = nullptr;

    // ----------------------------------------------------
    // ⭐ Recalculate level based on new XP
    // ----------------------------------------------------
    int newLevel = getLevelFromXp(p.xp);

    if (newLevel < p.level) {
        p.level = newLevel;
        applyLevelBonuses(p);

        p.client.println("You have dropped to level " + String(p.level) + ".");
        delay(2000);

        p.client.println("You are now known as: " +
            String(titles[p.raceId][p.level - 1]));
        delay(2000);
    }

    // ----------------------------------------------------
    // ⭐ Save immediately to prevent relog exploit
    // ----------------------------------------------------
    savePlayerToFS(p);

    // Teleport to spawn room
    int spawnX = 250;
    int spawnY = 250;
    int spawnZ = 50;

    p.client.println("Your spirit drifts back toward the mortal world...");
    delay(2000);

    loadRoomForPlayer(p, spawnX, spawnY, spawnZ);

    p.client.println("You awaken back at the spawn point...");
    delay(2000);

    cmdLook(p);
}

void broadcastRoomExcept(Player &p, const String &msg, Player &exclude) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (!players[i].active || !players[i].loggedIn) continue;
        if (&players[i] == &exclude) continue;

        if (players[i].roomX == p.roomX &&
            players[i].roomY == p.roomY &&
            players[i].roomZ == p.roomZ) {

            players[i].client.println(msg);
        }
    }
}

void spawnGoldAt(int x, int y, int z, int amount) {
    WorldItem wi;
    wi.name = "gold_coin";
    wi.value = amount;
    wi.x = x;
    wi.y = y;
    wi.z = z;
    wi.ownerName = "";
    wi.parentName = "";
    worldItems.push_back(wi);
}

// =============================
// Utility: sanitize input (strip CR/LF/tabs/control chars)
// =============================
String cleanInput(const String &in) {
  String s;
  for (int i = 0; i < in.length(); i++) {
    char c = in[i];
    if (c >= 32 && c <= 126) s += c;  // printable ASCII only
  }
  s.trim();
  return s;
}

// =============================
// Validation helpers
// =============================

bool isValidName(const String &s) {
  if (s.length() < 2 || s.length() > 20) return false;
  for (int i = 0; i < s.length(); i++) {
    if (!isalpha(s[i])) return false;
  }
  return true;
}

bool isValidPassword(const String &s) {
  if (s.length() < 3 || s.length() > 20) return false;
  for (int i = 0; i < s.length(); i++) {
    if (!isalnum(s[i])) return false;
  }
  return true;
}


void parseKeyValuePairs(const String &line, std::map<String, String> &out) {
    out.clear();
    int start = 0;

    while (start < line.length()) {
        int pipePos = line.indexOf('|', start);
        String token;

        if (pipePos == -1) {
            token = line.substring(start);
            start = line.length();
        } else {
            token = line.substring(start, pipePos);
            start = pipePos + 1;
        }

        token.trim();
        if (token.length() == 0) continue;

        int eqPos = token.indexOf('=');
        if (eqPos == -1) continue;

        String key   = token.substring(0, eqPos);
        String value = token.substring(eqPos + 1);

        key.trim();
        value.trim();

        out[key] = value;
    }
}
bool parseXYZ(const String &s, int &x, int &y, int &z) {
    String t = s;
    t.trim();                 // remove spaces + CRLF

    int c1 = t.indexOf(',');
    int c2 = t.indexOf(',', c1 + 1);

    if (c1 < 0 || c2 < 0) {
        x = y = z = 0;
        return false;
    }

    x = t.substring(0, c1).toInt();
    y = t.substring(c1 + 1, c2).toInt();
    z = t.substring(c2 + 1).toInt();
    return true;
}


String unescapeNewlines(const String &s) {
    String out;
    out.reserve(s.length());

    for (int i = 0; i < s.length(); i++) {
        if (s[i] == '\\' && i + 1 < s.length()) {
            char next = s[i + 1];

            if (next == 'n') {
                out += '\n';   // real newline
                i++;           // skip 'n'
                continue;
            }
            if (next == '\\') {
                out += '\\';   // literal backslash
                i++;
                continue;
            }
        }
        out += s[i];
    }

    return out;
}




void loadQuests() {
    quests.clear();

    File f = LittleFS.open("/quests.txt", "r");
    if (!f) {
        Serial.println("No quests.txt found; no quests loaded.");
        return;
    }

    while (f.available()) {
        String line = f.readStringUntil('\n');
        line.trim();
        if (line.length() == 0 || line.startsWith("#")) continue;

        // -----------------------------------------
        // SPLIT PIPE FORMAT: key=value|key=value|...
        // -----------------------------------------
        std::map<String, String> kv;

        int start = 0;
        while (start < line.length()) {
            int pipePos = line.indexOf('|', start);
            String part;

            if (pipePos == -1) {
                part = line.substring(start);
                start = line.length();
            } else {
                part = line.substring(start, pipePos);
                start = pipePos + 1;
            }

            int eqPos = part.indexOf('=');
            if (eqPos > 0) {
                String key = part.substring(0, eqPos);
                String val = part.substring(eqPos + 1);
                key.trim();
                val.trim();
                kv[key] = val;
            }
        }

        // -----------------------------------------
        // MUST HAVE questid
        // -----------------------------------------
        if (!kv.count("questid")) continue;
        int questId = kv["questid"].toInt();
        if (questId < 1 || questId > 10) continue;

        // -----------------------------------------
        // STEP LINE
        // -----------------------------------------
        if (kv.count("step")) {
            QuestStep step;
            step.questId = questId;
            step.step    = kv["step"].toInt();

            if (kv.count("task"))   step.task   = kv["task"];
            if (kv.count("item"))   step.item   = kv["item"];
            if (kv.count("target")) step.target = kv["target"];
            if (kv.count("phrase")) step.phrase = kv["phrase"];

            if (kv.count("targetroom")) {
                parseXYZ(kv["targetroom"], step.targetX, step.targetY, step.targetZ);
            }

            quests[questId].questId = questId;
            quests[questId].steps.push_back(step);
        }

        // -----------------------------------------
        // HEADER LINE
        // -----------------------------------------
        else {
            QuestDef &qd = quests[questId];

            // ⭐ Clear steps ONLY when encountering the header for this quest
            //    This prevents duplicates and prevents wiping steps mid-load.
            qd.steps.clear();

            qd.questId = questId;

            if (kv.count("name"))             qd.name             = kv["name"];
            if (kv.count("description"))      qd.description      = kv["description"];
            if (kv.count("difficulty"))       qd.difficulty       = kv["difficulty"];
            if (kv.count("completiondialog")) qd.completionDialog = kv["completiondialog"];
            if (kv.count("unloadnpcid"))      qd.unloadNpcId      = kv["unloadnpcid"];

            if (kv.count("reward_xp"))        qd.rewardXp         = kv["reward_xp"].toInt();
            if (kv.count("reward_gold"))      qd.rewardGold       = kv["reward_gold"].toInt();
        }
    }

    f.close();
    Serial.printf("Loaded %d quests.\n", (int)quests.size());
}


// =============================================================
// SAVE / LOAD (parent/child aware)
// =============================================================
//
// We save ONLY:
//   - player stats
//   - top-level inventory item IDs
//   - worn item IDs (per slot)
//   - wielded item ID
//
// Children are NOT saved explicitly.
// They restore automatically because:
//   - parentName persists in worldItems
//   - when a parent is restored to inventory, its children remain attached
//
// =============================================================

// Helper: get itemID from worldItems index
String getItemIDFromIndex(int idx) {
    if (idx < 0 || idx >= (int)worldItems.size()) return "";
    return worldItems[idx].name;
}

// Helper: find worldItems index by itemID (top-level only)
int findTopLevelItemByID(const String &id) {
    for (int i = 0; i < (int)worldItems.size(); i++) {
        if (worldItems[i].name == id &&
            worldItems[i].parentName.length() == 0) {
            return i;
        }
    }
    return -1;
}

// =============================================================
// SAVE PLAYER (CLEAN MODERN FORMAT)
// =============================================================

String sanitizeMsg(const String &in) {
    String out = "";
    for (int i = 0; i < in.length(); i++) {
        char c = in[i];
        if (c >= 32 && c <= 126) {  // printable ASCII only
            out += c;
        }
    }
    return out;
}

void savePlayerToFS(Player &p) {
    String path = String("/user_") + String(p.name) + ".txt";

    File f = LittleFS.open(path, "w");
    if (!f) {
        Serial.println("Failed to save player file.");
        return;
    }

    // -----------------------------
    // Basic stats
    // -----------------------------
    f.println(p.storedPassword);
    f.println(p.raceId);
    f.println(p.level);
    f.println(p.xp);
    f.println(p.hp);
    f.println(p.maxHp);
    f.println(p.coins);

    // -----------------------------
    // Wizard options
    // -----------------------------
    f.println(p.IsWizard ? "1" : "0");
    f.println((int)p.debugDest);

    // ⭐ NEW: Wizard stats toggle
    f.println(p.showStats ? "1" : "0");

    // -----------------------------
    // Custom enter/exit messages
    // -----------------------------
    f.println(sanitizeMsg(p.EnterMsg));
    f.println(sanitizeMsg(p.ExitMsg));

    // -----------------------------
    // Wimp mode
    // -----------------------------
    f.println(p.wimpMode ? "1" : "0");

    // -----------------------------
    // Room position
    // -----------------------------
    f.println(p.roomX);
    f.println(p.roomY);
    f.println(p.roomZ);

    // -----------------------------
    // Inventory (save item names)
    // -----------------------------
    f.println(p.invCount);
    for (int i = 0; i < p.invCount; i++) {
        int idx = p.invIndices[i];
        if (idx >= 0 && idx < (int)worldItems.size())
            f.println(worldItems[idx].name);
        else
            f.println("");
    }

    // -----------------------------
    // Wielded item (save name)
    // -----------------------------
    if (p.wieldedItemIndex >= 0 &&
        p.wieldedItemIndex < (int)worldItems.size())
        f.println(worldItems[p.wieldedItemIndex].name);
    else
        f.println("");

    // -----------------------------
    // Worn items (save names)
    // -----------------------------
    for (int s = 0; s < SLOT_COUNT; s++) {
        int idx = p.wornItemIndices[s];
        if (idx >= 0 && idx < (int)worldItems.size())
            f.println(worldItems[idx].name);
        else
            f.println("");
    }

    // -----------------------------
    // QUEST COMPLETION FLAGS
    // -----------------------------
    for (int q = 0; q < 10; q++)
        f.println(p.questCompleted[q] ? "1" : "0");

    // -----------------------------
    // QUEST STEP FLAGS
    // -----------------------------
    for (int q = 0; q < 10; q++)
        for (int s = 0; s < 10; s++)
            f.println(p.questStepDone[q][s] ? "1" : "0");

    f.close();


    saveWorldItems();
}



// =============================================================
// LOAD PLAYER (CLEAN MODERN FORMAT)
// =============================================================


bool loadPlayerFromFS(Player &p, const String &name) {
    String path = "/user_" + name + ".txt";
    if (!LittleFS.exists(path)) return false;

    File f = LittleFS.open(path, "r");
    if (!f) return false;

    auto safeRead = [&](String &out) {
        if (!f.available()) { out = ""; return; }
        out = f.readStringUntil('\n');
        out.trim();
    };

    String tmp;

    // -----------------------------
    // Basic stats
    // -----------------------------
    safeRead(tmp); strncpy(p.storedPassword, tmp.c_str(), sizeof(p.storedPassword)-1);
    p.storedPassword[sizeof(p.storedPassword)-1] = '\0';

    safeRead(tmp); p.raceId = tmp.toInt();
    safeRead(tmp); p.level  = tmp.toInt();
    safeRead(tmp); p.xp     = tmp.toInt();
    safeRead(tmp); p.hp     = tmp.toInt();
    safeRead(tmp); p.maxHp  = tmp.toInt();
    safeRead(tmp); p.coins  = tmp.toInt();

    // Wizard flag
    safeRead(tmp);
    p.IsWizard = (tmp == "1");

    // Debug destination
    safeRead(tmp);
    p.debugDest = (DebugDestination)tmp.toInt();

    // Wizard stats toggle
    safeRead(tmp);
    p.showStats = (tmp == "1");

    // Custom Enter/Exit messages
    safeRead(tmp); p.EnterMsg = sanitizeMsg(tmp);
    safeRead(tmp); p.ExitMsg  = sanitizeMsg(tmp);

    // Wimp mode
    safeRead(tmp);
    p.wimpMode = (tmp == "1");

    // Room position
    safeRead(tmp); p.roomX = tmp.toInt();
    safeRead(tmp); p.roomY = tmp.toInt();
    safeRead(tmp); p.roomZ = tmp.toInt();

    // Inventory count
    safeRead(tmp);
    int invCount = tmp.toInt();
    if (invCount < 0) invCount = 0;
    if (invCount > 32) invCount = 32;
    p.invCount = 0;

    // -----------------------------
    // Inventory items
    // -----------------------------
    // Inventory items
    // Don't clone - use existing items from worldItems if they exist
    // Items should have been loaded from world_items.vxi with ownerName = p.name AND parentName = p.name
    // (parentName = p.name means it's a top-level inventory item, not inside a container)
    // We just rebuild the inventory index array
    // NEVER allow gold coins in inventory (they go to p.coins instead)
    // -----------------------------
    for (int i = 0; i < invCount; i++) {
        safeRead(tmp);
        if (tmp.length() == 0) continue;

        String itemName = tmp;
        
        // NEVER load gold coins into inventory
        if (itemName == "gold_coin") {
            continue;
        }
        
        // Find this item in worldItems with ownerName = p.name AND parentName = p.name (top-level)
        bool found = false;
        for (int j = 0; j < (int)worldItems.size(); j++) {
            if (worldItems[j].name == itemName && 
                worldItems[j].ownerName == p.name &&
                worldItems[j].parentName == p.name) {
                // EXTRA SAFETY: verify it's not a gold coin
                if (worldItems[j].name == "gold_coin") {
                    continue;
                }
                p.invIndices[p.invCount++] = j;
                found = true;
                break;
            }
        }
        
        // If not found in world items, create a clone (for items not in world save)
        if (!found) {
            WorldItem newItem;
            newItem.name = itemName;
            newItem.ownerName = p.name;
            newItem.parentName = p.name;
            newItem.x = newItem.y = newItem.z = -1;

            // SAFE lookup
            std::string key = std::string(itemName.c_str());
            auto it = itemDefs.find(key);
            if (it != itemDefs.end()) {
                for (auto &kv : it->second.attributes)
                    newItem.attributes[kv.first] = kv.second;
            }

            worldItems.push_back(newItem);
            p.invIndices[p.invCount++] = worldItems.size() - 1;
        }
    }

    // -----------------------------
    // Wielded item (REUSE if in worldItems, CLONE if not found)
    // -----------------------------
    safeRead(tmp);
    if (tmp.length() > 0) {
        String itemName = tmp;

        // Search worldItems first
        bool found = false;
        for (int i = 0; i < (int)worldItems.size(); i++) {
            if (worldItems[i].name.length() > 0 && 
                strcmp(worldItems[i].name.c_str(), itemName.c_str()) == 0 &&
                worldItems[i].ownerName == p.name) {
                p.wieldedItemIndex = i;
                found = true;
                break;
            }
        }

        if (!found) {
            WorldItem newItem;
            newItem.name = itemName;
            newItem.ownerName = p.name;
            newItem.parentName = "";
            newItem.x = newItem.y = newItem.z = -1;

            std::string key = std::string(itemName.c_str());
            auto it = itemDefs.find(key);
            if (it != itemDefs.end()) {
                for (auto &kv : it->second.attributes)
                    newItem.attributes[kv.first] = kv.second;
            }

            worldItems.push_back(newItem);
            p.wieldedItemIndex = worldItems.size() - 1;
        }
    } else {
        p.wieldedItemIndex = -1;
    }

    // -----------------------------
    // Worn items (REUSE if in worldItems, CLONE if not found)
    // -----------------------------
    for (int s = 0; s < SLOT_COUNT; s++) {
        safeRead(tmp);

        if (tmp.length() == 0) {
            p.wornItemIndices[s] = -1;
            continue;
        }

        String itemName = tmp;

        // Search worldItems first
        bool found = false;
        for (int i = 0; i < (int)worldItems.size(); i++) {
            if (worldItems[i].name.length() > 0 && 
                strcmp(worldItems[i].name.c_str(), itemName.c_str()) == 0 &&
                worldItems[i].ownerName == p.name) {
                p.wornItemIndices[s] = i;
                found = true;
                break;
            }
        }

        if (!found) {
            WorldItem newItem;
            newItem.name = itemName;
            newItem.ownerName = p.name;
            newItem.parentName = "";
            newItem.x = newItem.y = newItem.z = -1;

            std::string key = std::string(itemName.c_str());
            auto it = itemDefs.find(key);
            if (it != itemDefs.end()) {
                for (auto &kv : it->second.attributes)
                    newItem.attributes[kv.first] = kv.second;
            }

            worldItems.push_back(newItem);
            p.wornItemIndices[s] = worldItems.size() - 1;
        }
    }

    // -----------------------------
    // QUEST COMPLETION FLAGS
    // -----------------------------
    for (int q = 0; q < 10; q++) {
        safeRead(tmp);
        p.questCompleted[q] = (tmp == "1");
    }

    // -----------------------------
    // QUEST STEP FLAGS
    // -----------------------------
    for (int q = 0; q < 10; q++) {
        for (int s = 0; s < 10; s++) {
            safeRead(tmp);
            p.questStepDone[q][s] = (tmp == "1");
        }
    }

    f.close();
    return true;
}



// =============================
// Login stages
// =============================

enum LoginStage {
  LOGIN_NAME = 0,
  LOGIN_PASSWORD,
  LOGIN_NEW_PASSWORD,
  LOGIN_RACE,
  LOGIN_DONE
};

struct LoginState {
  LoginStage stage = LOGIN_NAME;
  unsigned long startTime = 0;
  int passwordAttempts = 0;

    // ADD THESE TWO:
  int newPasswordAttempts = 0;
  int raceAttempts = 0;

  String tempName;
  String tempPassword;
};

LoginState loginState[MAX_PLAYERS];

// =============================
// Begin login for a new connection
// =============================

void startLogin(Player &p, int index) {
  loginState[index] = LoginState();
  loginState[index].startTime = millis();

  p.client.println(GLOBAL_MUD);
  p.client.println(); // blank line

  p.client.println("Enter your name:");
}


void sendWelcome(Player &p) {
    p.client.println(); // blank line
    p.client.println("Welcome, " + capFirst(p.name) + "!");
    for (int i = 0; GLOBAL_WELCOME_LINES[i] != nullptr; i++) {
        p.client.println(GLOBAL_WELCOME_LINES[i]);
    }
    p.client.println(); // blank line
}


// =============================
// Handle login input
// =============================

void sendTelnetNegotiation(WiFiClient &c) {
    const uint8_t seq[] = {
        255, 253, 3,   // IAC DO SUPPRESS-GO-AHEAD
        255, 251, 3,   // IAC WILL SUPPRESS-GO-AHEAD
        255, 253, 1,   // IAC DO ECHO
        255, 251, 1,   // IAC WILL ECHO
        255, 253, 34,  // IAC DO LINEMODE
        255, 250, 34, 1, 0, 255, 240  // IAC SB LINEMODE MODE 0 IAC SE
    };
    c.write(seq, sizeof(seq));
}


void initPlayer(Player &p) {
    p.invCount = 0;
    p.wieldedItemIndex = -1;
    p.isDuplicateLogin = false;  // Reset flag for new session

    for (int s = 0; s < SLOT_COUNT; s++) {
        p.wornItemIndices[s] = -1;
    }

    p.weaponBonus = 0;
    p.armorBonus = 0;

    p.inCombat = false;
    p.combatTarget = nullptr;
    p.nextCombatTime = 0;
}





void handleLogin(Player &p, int index, const String &rawLine) {
    LoginState &st = loginState[index];
    String line = cleanInput(rawLine);

    // Timeout check
    if (millis() - st.startTime > 30000UL) {
        p.client.println("Login timed out.");
        p.client.stop();
        p.active = false;
        return;
    }

    switch (st.stage) {

        // -------------------------
        case LOGIN_NAME:
        // -------------------------
        {
            if (!isValidName(line)) {
                p.client.println("Names must contain letters only (2–20 chars). Try again:");
                return;
            }

            st.tempName = line;
            strncpy(p.name, line.c_str(), sizeof(p.name)-1);

            initPlayer(p);

            if (loadPlayerFromFS(p, line)) {
                // Existing player
                recalcBonuses(p);   // ⭐ ADD THIS LINE ⭐
                
                // Rebuild container children for items in inventory
                linkWorldItemParents();

                st.stage = LOGIN_PASSWORD;
                p.client.println("Enter your password:");
            } else {
                // New player
                st.stage = LOGIN_NEW_PASSWORD;
                p.client.println("New character! Enter a password:");
            }
            return;
        }

        // -------------------------
        case LOGIN_PASSWORD:
        // -------------------------
        {
            if (line.length() == 0) return;  // ignore empty lines

            if (!isValidPassword(line)) {
                p.client.println("Invalid password format. Try again:");
                return;
            }

            if (line != String(p.storedPassword)) {
                st.passwordAttempts++;
                if (st.passwordAttempts >= 3) {
                    p.client.println("Too many failed attempts. Disconnecting.");
                    p.client.stop();
                    p.active = false;
                    return;
                }
                p.client.println("Incorrect password. Try again:");
                return;
            }

            // Check if player is already logged in elsewhere
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (players[i].active && players[i].loggedIn && 
                    strcasecmp(players[i].name, p.name) == 0) {
                    // Found existing session with same name - disconnect it
                    logSessionLogout(players[i].name);  // Log the old session logout
                    players[i].client.println("You have logged in elsewhere. Disconnecting...");
                    players[i].client.stop();
                    players[i].active = false;
                    players[i].loggedIn = false;
                    
                    // Mark new session as duplicate login (was already online)
                    p.isDuplicateLogin = true;
                    break;
                }
            }

            // Successful login
            st.stage = LOGIN_DONE;
            p.loggedIn = true;
            
            // Log session login
            logSessionLogin(p.name);

            // --- Welcome message ---
            sendWelcome(p);
            p.client.println();  // blank line

         
             // Wizard status based on level (level 20+ = wizard)
            if (p.level >= 20) {
                p.IsWizard = true;
            } else {
                p.IsWizard = false;
            }

            // Special case: certain players are always wizards
            const char* wizardExceptions[] = {"Atew", "Ploi", "Dogbowl"};
            for (int i = 0; i < 3; i++) {
                if (!strcasecmp(p.name, wizardExceptions[i])) {
                    p.IsWizard = true;
                    break;
                }
            }


            if (p.IsWizard) {
                p.client.println("=== WIZARD MODE ===");
                p.client.println("ESP32MUD v" + String(ESP32MUD_VERSION));
                p.client.println("Compiled: " + String(COMPILE_DATE) + " " + formatCompileTimeWithTimezone(COMPILE_TIME));
                
                // Program size (flash) - Partition: factory app 0x100000 (1MB)
                uint32_t sketchSize = ESP.getSketchSize();
                uint32_t sketchSpace = 0x100000;  // 1MB factory partition from no_ota.csv
                int programPercent = (sketchSize * 100) / sketchSpace / 2;  // Divide by 2 for actual 2MB partition
                p.client.println("Program Size: (" + String(sketchSize / 1024) + " KB of " + String(sketchSpace / 1024) + " KB) -> " + String(programPercent) + "%");
                
                // LittleFS space - Partition: littlefs 0x2E0000 (~2.8MB)
                size_t fsUsed = LittleFS.usedBytes();
                size_t fsTotal = LittleFS.totalBytes();
                int fsPercent = (fsUsed * 100) / fsTotal;
                p.client.println("LittleFS Space: (" + String(fsUsed / 1024) + " KB of " + String(fsTotal / 1024) + " KB) -> " + String(fsPercent) + "%");
                p.client.println();
                
                if (p.showStats)
                    p.client.println("Wizard stats output is currently: ON");
                else
                    p.client.println("Wizard stats output is currently: OFF");

                p.client.println();  // keep spacing consistent

                // -----------------------------------------
                // Serial fallback warning for wizards
                // -----------------------------------------
                if (NoSerial) {
                    p.client.println("Warning: The Serial connection was not found and was ignored.");
                    p.client.println();  // spacing
                }
            }

            // Determine this player's index in players[]
            int pIndex = -1;
            for (int i = 0; i < MAX_PLAYERS; i++) {
                if (&players[i] == &p) {
                    pIndex = i;
                    break;
                }
            }

            int correctLevel = getLevelFromXp(p.xp);

            if (correctLevel > p.level) {
                p.level = correctLevel;
                applyLevelBonuses(p);

                p.client.println("Congratulations! You have advanced a level " + String(p.level) + "!");
                p.client.println("You are now known as: " + String(titles[p.raceId][p.level - 1]));

                savePlayerToFS(p);
            }


            // Load room based on login type
            int loadX, loadY, loadZ;
            
            if (p.isDuplicateLogin) {
                // Duplicate login while still online: restore to last room
                loadX = p.roomX > 0 ? p.roomX : 250;
                loadY = p.roomY > 0 ? p.roomY : 250;
                loadZ = p.roomZ > 0 ? p.roomZ : 50;
                p.client.println("Already logged in elsewhere! Transforming you to your last room...");
                p.client.println();
            } else {
                // Normal login: restore to saved coordinates (set to spawn on quit)
                loadX = p.roomX > 0 ? p.roomX : 250;
                loadY = p.roomY > 0 ? p.roomY : 250;
                loadZ = p.roomZ > 0 ? p.roomZ : 50;
            }

            if (!loadRoomForPlayer(p, loadX, loadY, loadZ)) {
                // Fallback to spawn room if specified room fails
                if (!loadRoomForPlayer(p, 250, 250, 50)) {
                    p.client.println("ERROR: Could not load spawn room!");
                    Serial.println("[ERROR] Spawn room missing from rooms.txt!");
                } else {
                    cmdLook(p);
                }
            } else {
                cmdLook(p);
            }

            return;
        }

        // -------------------------
        case LOGIN_NEW_PASSWORD:
        // -------------------------
        {
            if (!isValidPassword(line)) {
                st.newPasswordAttempts++;
                if (st.newPasswordAttempts >= 3) {
                    p.client.println("Too many failed attempts. Disconnecting.");
                    p.client.stop();
                    p.active = false;
                    return;
                }
                p.client.println("Passwords must be letters and numbers only (3–20 chars). Try again:");
                return;
            }

            st.tempPassword = line;
            strncpy(p.storedPassword, line.c_str(), sizeof(p.storedPassword)-1);

            // Race selection
            p.client.println("Choose your race:");
            for (int i = 0; i < 5; i++) {
                p.client.print(i);
                p.client.print(") ");
                p.client.println(raceNames[i]);
            }
            st.stage = LOGIN_RACE;
            return;
        }

        // -------------------------
        case LOGIN_RACE:
        // -------------------------
        {
            int r = line.toInt();
            if (r < 0 || r > 4) {
                st.raceAttempts++;
                if (st.raceAttempts >= 3) {
                    p.client.println("Too many invalid choices. Disconnecting.");
                    p.client.stop();
                    p.active = false;
                    return;
                }
                p.client.println("Invalid race. Choose 0–4:");
                return;
            }

            p.raceId = r;
            p.level = 1;
            p.xp = 0;
            p.maxHp = 20;
            p.hp = 20;
            p.coins = 0;
            p.IsWizard = false; // default


            // Initialize quest progress (10 quests × 10 steps)
            for (int q = 0; q < 10; q++) {
                p.questCompleted[q] = false;
                for (int s = 0; s < 10; s++) {
                    p.questStepDone[q][s] = false;
                }
            }

            // Save new character
            savePlayerToFS(p);

            st.stage = LOGIN_DONE;
            p.loggedIn = true;

            // --- Welcome message ---
            sendWelcome(p);
            p.client.println(); // blank line

            // Check for wizard exceptions (new characters)
            const char* wizardExceptions[] = {"Atew", "Ploi", "Dogbowl"};
            for (int i = 0; i < 3; i++) {
                if (!strcasecmp(p.name, wizardExceptions[i])) {
                    p.IsWizard = true;
                    
                    // Show special wizard greeting
                    p.client.println("");
                    p.client.println("╔═══════════════════════════════════════════════════════════╗");
                    p.client.println("║ CONGRATULATIONS! You have been made an honorary Wizard on   ║");
                    p.client.println("║ ESPERTHERU! It's no Nobel Peace Prize, but who would want ║");
                    p.client.println("║ one of those?                                              ║");
                    p.client.println("╚═══════════════════════════════════════════════════════════╝");
                    p.client.println("");
                    
                    break;
                }
            }

            // New characters ALWAYS start at spawn
            if (!loadRoomForPlayer(p, 250, 250, 50)) {
                p.client.println("ERROR: Could not load spawn room!");
                Serial.println("[ERROR] Spawn room missing from rooms.txt!");
            } else {
                cmdLook(p);
            }

            return;
        }

        // -------------------------
        case LOGIN_DONE:
            return;
    }
}



void sendVoxel(Player &p) {
  p.client.print("VOXEL: ");
  p.client.print(p.roomX);
  p.client.print(",");
  p.client.print(p.roomY);
  p.client.print(",");
  p.client.println(p.roomZ);
}

void broadcastToAll(const String &msg) {
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player &p = players[i];
        if (!p.active || !p.loggedIn) continue;
        p.client.println(msg);
    }
}


int extractNumber(const String &s) {
    int num = 0;
    bool found = false;

    for (int i = 0; i < s.length(); i++) {
        char c = s.charAt(i);

        if (c >= '0' && c <= '9') {
            num = num * 10 + (c - '0');
            found = true;
        }
        else if (found) {
            // stop at first non-digit after digits
            break;
        }
    }

    return found ? num : 0;   // 0 means "no number found"
}

bool npcNameMatches(const String &npcName, const String &arg) {
    // Full match
    if (npcName.equalsIgnoreCase(arg)) return true;

    // Split NPC name into words
    std::vector<String> words;
    int start = 0;
    while (true) {
        int space = npcName.indexOf(' ', start);
        if (space < 0) {
            words.push_back(npcName.substring(start));
            break;
        }
        words.push_back(npcName.substring(start, space));
        start = space + 1;
    }

    // Match any word or prefix of a word
    for (auto &w : words) {
        if (w.equalsIgnoreCase(arg)) return true;
        if (w.startsWith(arg)) return true;
    }

    return false;
}


//Item Resolvers//

// ---------------------------------------------------------
// Classic MUD name matching
// ---------------------------------------------------------
bool nameMatches(const String &itemName, const String &searchRaw) {
    String name = itemName;
    String search = searchRaw;

    name.toLowerCase();
    search.toLowerCase();

    if (name == search) return true;
    if (name.startsWith(search)) return true;
    if (name.indexOf(search) != -1) return true;

    return false;
}



// ---------------------------------------------------------
// Unified resolver (Option A: inventory → worn → room)
// ---------------------------------------------------------




// ---------------------------------------------------------
// SEARCH override — prefer room item with children
// ---------------------------------------------------------


int resolveItemForSearch(Player &p, const String &raw) {
    String search = raw;
    search.trim();
    search.toLowerCase();

    // Inventory
    for (int i = 0; i < p.invCount; i++) {
        int wiIndex = p.invIndices[i];
        if (wiIndex < 0 || wiIndex >= (int)worldItems.size()) continue;

        WorldItem &wi = worldItems[wiIndex];

        String base = wi.name; base.toLowerCase();
        String disp = resolveDisplayName(wi); disp.toLowerCase();

        if (nameMatches(base, search) || nameMatches(disp, search))
            return wiIndex;
    }

    // Room items (hidden allowed)
    for (int i = 0; i < (int)worldItems.size(); i++) {
        WorldItem &wi = worldItems[i];

        if (wi.ownerName.length() != 0) continue;
        if (wi.x != p.roomX || wi.y != p.roomY || wi.z != p.roomZ) continue;

        String base = wi.name; base.toLowerCase();
        String disp = resolveDisplayName(wi); disp.toLowerCase();

        if (nameMatches(base, search) || nameMatches(disp, search))
            return i;
    }

    return -1;
}




int resolveItem(Player &p, const String &raw) {
    String search = raw;
    search.trim();
    search.toLowerCase();

    // 1. Inventory (encode as slot | 0x80000000)
    for (int i = 0; i < p.invCount; i++) {
        int wiIndex = p.invIndices[i];
        if (wiIndex < 0 || wiIndex >= (int)worldItems.size()) continue;

        WorldItem &wi = worldItems[wiIndex];
        String disp = resolveDisplayName(wi);

        if (nameMatches(disp, search) || nameMatches(wi.name, search)) {
            return (i | 0x80000000);
        }
    }

    // 2. Worn items (direct world index)
    for (int s = 0; s < SLOT_COUNT; s++) {
        int wiIndex = p.wornItemIndices[s];
        if (wiIndex < 0 || wiIndex >= (int)worldItems.size()) continue;

        WorldItem &wi = worldItems[wiIndex];
        String disp = resolveDisplayName(wi);

        if (nameMatches(disp, search) || nameMatches(wi.name, search)) {
            return wiIndex;
        }
    }

    // 3. Room items (direct world index)
    Room &r = p.currentRoom;
    for (int i = 0; i < (int)worldItems.size(); i++) {
        WorldItem &wi = worldItems[i];

        if (wi.ownerName.length() != 0) continue;
        if (wi.parentName.length() != 0) continue;
        if (wi.x != r.x || wi.y != r.y || wi.z != r.z) continue;

        String disp = resolveDisplayName(wi);

        if (nameMatches(disp, search) || nameMatches(wi.name, search)) {
            return i;
        }
    }

    return -1;
}


WorldItem* getResolvedWorldItem(Player &p, int resolvedIndex) {
    // High-bit encoding: inventory slot
    if (resolvedIndex & 0x80000000) {
        int slot = (resolvedIndex & 0x7FFFFFFF);
        if (slot < 0 || slot >= p.invCount) return nullptr;

        int wiIndex = p.invIndices[slot];
        if (wiIndex < 0 || wiIndex >= (int)worldItems.size()) return nullptr;

        return &worldItems[wiIndex];
    }

    // Direct worldItems index
    if (resolvedIndex < 0 || resolvedIndex >= (int)worldItems.size())
        return nullptr;

    return &worldItems[resolvedIndex];
}


int resolveItemInContainer(Player &p, WorldItem &container, const String &raw) {
    String search = raw;
    search.trim();
    search.toLowerCase();

    for (int childIndex : container.children) {
        if (childIndex < 0 || childIndex >= (int)worldItems.size()) continue;

        WorldItem &child = worldItems[childIndex];

        // Base name
        String base = child.name;
        base.toLowerCase();

        // Display name
        String disp = resolveDisplayName(child);
        disp.toLowerCase();

        if (nameMatches(base, search) || nameMatches(disp, search)) {
            return childIndex;
        }
    }

    return -1;
}




// ---------------------------------------------------------
// findItemAnywhere wrapper
// ---------------------------------------------------------
int findItemAnywhere(Player &p, const String &raw) {
    return resolveItem(p, raw);
}












// -----------------------------------------
// Resolve display name for matching
// -----------------------------------------
String resolveItemNameForMatch(WorldItem &wi) {
    String disp = resolveDisplayName(wi);
    if (disp.length() == 0) disp = wi.name;
    return disp;
}




// -----------------------------------------
// Helper: decode resolved index to WorldItem&
// -----------------------------------------






void debugPrint(Player &p, const String &msg) {
    if (p.debugDest == DEBUG_TO_SERIAL) {
        Serial.println(msg);
    } else {
        p.client.println(msg);
    }
}


// =============================
// Command parser
// =============================

// =============================
// Command parser (SECTION 1/4)
// CORE INTERACTION COMMANDS
// =============================
void handleCommand(Player &p, int index, const String &rawLine) {
    // -----------------------------------------
    // Check if player left the game room (end game if so)
    // -----------------------------------------
    if (index >= 0 && index < MAX_PLAYERS && highLowSessions[index].gameActive) {
        if (p.roomX != highLowSessions[index].gameRoomX || 
            p.roomY != highLowSessions[index].gameRoomY || 
            p.roomZ != highLowSessions[index].gameRoomZ) {
            // Player moved out of the game room - end game
            endHighLowGame(p, index);
            p.client.println("Your High-Low game has ended because you left the room.");
            p.client.println("");
        }
    }

    // -----------------------------------------
    // Handle High-Low continue prompt FIRST (before empty check)
    // -----------------------------------------
    if (index >= 0 && index < MAX_PLAYERS && highLowSessions[index].gameActive) {
        HighLowSession &session = highLowSessions[index];
        
        if (session.awaitingContinue) {
            String trimmed = rawLine;
            trimmed.trim();
            String lowerTrimmed = trimmed;
            lowerTrimmed.toLowerCase();
            
            p.client.println("[DEBUG CONTINUE] trimmed='" + trimmed + "' length=" + String(trimmed.length()));
            
            if (trimmed.length() == 0) {
                // Empty input - start next hand
                p.client.println("[DEBUG] Empty input detected - dealing next hand");
                session.awaitingContinue = false;
                p.client.println("");
                dealHighLowHand(p, index);
                return;
            } else if (lowerTrimmed == "end" || lowerTrimmed == "quit") {
                // End the game
                endHighLowGame(p, index);
                return;
            } else if (lowerTrimmed == "n" || lowerTrimmed == "s" || lowerTrimmed == "e" || lowerTrimmed == "w" || 
                       lowerTrimmed.startsWith("go ")) {
                // Allow movement commands
                session.awaitingContinue = false;
                // Fall through to normal command processing
            } else {
                // Invalid input during continue prompt
                p.client.println("Press [Enter] to continue or type 'end'");
                return;
            }
        }
    }

    // -----------------------------------------
    // Clean and split input
    // -----------------------------------------
    String line = cleanInput(rawLine);
    if (line.length() == 0) return;

    int space = line.indexOf(' ');
    String cmd  = (space == -1) ? line : line.substring(0, space);
    String args = (space == -1) ? ""   : line.substring(space + 1);

    cmd.toLowerCase();
    args.trim();

    // -----------------------------------------
    // LOOK / READ
    // -----------------------------------------
    if (cmd == "l" || cmd == "look") {

        // LOOK IN <container>
        if (args.startsWith("in ")) {
            String containerName = args.substring(3);
            containerName.trim();
            if (containerName.length() == 0) {
                p.client.println("Look in what?");
                return;
            }
            cmdLookIn(p, containerName);
            return;
        }

        // LOOK AT <item>
        if (args.startsWith("at")) {
            String targetName = args.substring(2);
            targetName.trim();
            if (targetName.length() == 0) {
                p.client.println("Look at what?");
                return;
            }
            cmdLookAt(p, targetName);
            return;
        }

        // LOOK <item>
        if (args.length() > 0) {
            cmdLookAt(p, args);
            return;
        }

        // LOOK (room)
        cmdLook(p);
        return;
    }

    if (cmd == "read") {
        if (args.length() == 0) {
            p.client.println("Read what?");
            return;
        }
        
        String argsLower = args;
        argsLower.toLowerCase();
        
        // Prevent "read mail" from causing issues - direct to examining letters instead
        if (argsLower == "mail") {
            p.client.println("You don't see any mail here. Try getting a letter and examining it.");
            return;
        }
        
        // Check if they're reading a shop sign
        if (argsLower == "sign") {
            cmdReadSign(p, args);
            return;
        }
        
        cmdLookAt(p, args);
        return;
    }

    // -----------------------------------------
    // SEARCH / EXAMINE
    // -----------------------------------------
    if (cmd == "search") {
        cmdExamineSearch(p, args, true);
        return;
    }

    if (cmd == "examine" || cmd == "exam") {
        cmdExamineSearch(p, args, false);
        return;
    }

    // -----------------------------------------
    // MOVEMENT
    // -----------------------------------------



    // -----------------------------------------
    // Portal activation  intercept the movement and teleport to using portal command eg.  Church 'enter', etc
    // -----------------------------------------
    if (p.currentRoom.hasPortal) {
        String pcmd = String(p.currentRoom.portalCommand);
        pcmd.trim();
        pcmd.toLowerCase();

        if (cmd == pcmd) {
            cmdPortal(p, index);
            return;
        }
    }

    if (cmd == "n" || cmd == "north" ||
        cmd == "s" || cmd == "south" ||
        cmd == "e" || cmd == "east"  ||
        cmd == "w" || cmd == "west"  ||
        cmd == "ne"|| cmd == "northeast" ||
        cmd == "nw"|| cmd == "northwest" ||
        cmd == "se"|| cmd == "southeast" ||
        cmd == "sw"|| cmd == "southwest" ||
        cmd == "u" || cmd == "up" ||
        cmd == "d" || cmd == "down")
    {
        movePlayer(p, index, normalizeDir(cmd).c_str());
        return;
    }

    // -----------------------------------------
    // INVENTORY
    // -----------------------------------------
    if (cmd == "i" || cmd == "inventory") {
        cmdInventory(p, "");
        return;
    }


// =============================
// Command parser (SECTION 2/4)
// ITEM & CONTAINER COMMANDS
// =============================

// -----------------------------------------
// GET
// -----------------------------------------
    if (cmd == "get") {

        // GET ALL
        if (args.equalsIgnoreCase("all")) {
            cmdGetAll(p);
            return;
        }

        // GET ALL FROM <container>
        if (args.startsWith("all from ")) {
            String containerName = args.substring(9);
            containerName.trim();
            if (containerName.length() == 0) {
                p.client.println("Get all from what?");
                return;
            }
            cmdGetAllFrom(p, containerName);
            return;
        }

        // GET <item> FROM <container>
        int pos = args.indexOf(" from ");
        if (pos >= 0) {
            String itemName = args.substring(0, pos);
            String containerName = args.substring(pos + 6);

            itemName.trim();
            containerName.trim();

            if (itemName.length() == 0) {
                p.client.println("Get what?");
                return;
            }
            if (containerName.length() == 0) {
                p.client.println("Get it from what?");
                return;
            }

            cmdGetFrom(p, itemName + " from " + containerName);
            return;
        }

        // GET <item>
        if (args.length() == 0) {
            p.client.println("Get what?");
            return;
        }

        cmdGet(p, args);
        return;
    }

// -----------------------------------------
// PUT
// -----------------------------------------
    if (cmd == "put") {

        // PUT ALL IN <container>
        if (args.startsWith("all in ")) {
            String containerName = args.substring(7);
            containerName.trim();
            if (containerName.length() == 0) {
                p.client.println("Put all in what?");
                return;
            }
            cmdPutAll(p, "all in " + containerName);
            return;
        }

        // PUT <item> IN <container>
        int pos = args.indexOf(" in ");
        if (pos >= 0) {
            String itemName = args.substring(0, pos);
            String containerName = args.substring(pos + 4);

            itemName.trim();
            containerName.trim();

            if (itemName.length() == 0) {
                p.client.println("Put what?");
                return;
            }
            if (containerName.length() == 0) {
                p.client.println("Put it in what?");
                return;
            }

            // Unified PUT handler
            cmdPutIn(p, itemName + " in " + containerName);
            return;
        }

        p.client.println("Put what in what?");
        return;
    }

// -----------------------------------------
// DROP
// -----------------------------------------
    if (cmd == "drop") {

        // DROP ALL
        if (args.equalsIgnoreCase("all")) {
            cmdDropAll(p);
            return;
        }

        // DROP <item>
        if (args.length() == 0) {
            p.client.println("Drop what?");
            return;
        }

        cmdDrop(p, args);
        return;
    }

// =============================
// Command parser (SECTION 3/4)
// SOCIAL, SHOP, EQUIPMENT, COMBAT, INFO
// =============================

// -----------------------------------------
// SOCIAL: SAY / SHOUT
// -----------------------------------------
    if (cmd == "say") {
        if (args.length() == 0) {
            p.client.println("Say what?");
            return;
        }
        cmdSay(p, args.c_str());
        return;
    }

    if (cmd == "shout") {
        if (args.length() == 0) {
            p.client.println("Shout what?");
            return;
        }
        cmdShout(p, args.c_str());
        return;
    }

    if (cmd == "tell") {
        if (args.length() == 0) {
            p.client.println("Usage: tell [player name] [message]");
            return;
        }
        // Split: first word is player name, rest is message
        int spacePos = args.indexOf(' ');
        String targetName, message;
        if (spacePos < 0) {
            targetName = args;
            message = "";
        } else {
            targetName = args.substring(0, spacePos);
            message = args.substring(spacePos + 1);
        }
        cmdTell(p, targetName, message);
        return;
    }

// -----------------------------------------
// GIVE
// -----------------------------------------
    if (cmd == "give") {
        if (args.length() == 0) {
            p.client.println("Give what to whom?");
            return;
        }
        cmdGive(p, args);
        return;
    }

// -----------------------------------------
// SHOP: BUY / SELL
// -----------------------------------------
    if (cmd == "buy") {
        if (args.length() == 0) {
            p.client.println("Buy what?");
            return;
        }
        cmdBuy(p, args);
        return;
    }

    if (cmd == "sell") {
        if (args.length() == 0) {
            p.client.println("Sell what?");
            return;
        }
        if (args == "all") {
            cmdSellAll(p);
            return;
        }
        cmdSell(p, args);
        return;
    }

// -----------------------------------------
// EAT / DRINK
// -----------------------------------------
    if (cmd == "eat") {
        if (args.length() == 0) {
            p.client.println("Eat what?");
            return;
        }
        cmdEat(p, args.c_str());
        return;
    }

    if (cmd == "drink") {
        if (args.length() == 0) {
            p.client.println("Drink what?");
            return;
        }
        cmdDrink(p, args.c_str());
        return;
    }

// -----------------------------------------
// POST OFFICE: SEND / CHECK MAIL
// -----------------------------------------
    if (cmd == "send") {
        if (args.length() == 0) {
            p.client.println("Send to whom?");
            return;
        }
        cmdSend(p, args);
        return;
    }

    if (cmd == "checkmail" || cmd == "check mail" || cmd == "mail") {
        // Check for mail and spawn letters
        bool hasMailResult = checkAndSpawnMailLetters(p);
        
        // Provide feedback
        if (!hasMailResult) {
            p.client.println("No mail today, sorry.");
        }
        return;
    }

// -----------------------------------------
// GAME PARLOR: HIGH-LOW CARD GAME
// -----------------------------------------
    if (cmd == "play") {
        // Check if player is in Game Parlor
        if (!(p.roomX == 247 && p.roomY == 248 && p.roomZ == 50)) {
            p.client.println("You can only play in the Game Parlor!");
            return;
        }
        
        // If no game specified, show available games
        if (args.length() == 0) {
            p.client.println("Available games:");
            p.client.println("  1. High-Low Card Game (bet on whether 3rd card is inside/outside range)");
            p.client.println("  2. Chess (challenge the engine)");
            p.client.println("Usage: play 1");
            return;
        }
        
        // Find the player index
        int playerIndex = -1;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (&players[i] == &p) {
                playerIndex = i;
                break;
            }
        }
        
        if (playerIndex == -1) {
            p.client.println("Error: Could not find player index.");
            return;
        }
        
        int gameNum = args.toInt();
        
        // Game 1: High-Low
        if (gameNum == 1) {
            HighLowSession &session = highLowSessions[playerIndex];
            
            // If not playing, start a new game
            if (!session.gameActive) {
                initializeHighLowSession(playerIndex);
                session.gameActive = true;
                session.gameRoomX = p.roomX;
                session.gameRoomY = p.roomY;
                session.gameRoomZ = p.roomZ;
                dealHighLowHand(p, playerIndex);
            } else {
                p.client.println("You are already in a game. Use commands like a number or 'pot' to bet.");
            }
        }
        // Game 2: Chess
        else if (gameNum == 2) {
            ChessSession &session = chessSessions[playerIndex];
            
            // If not playing, start a new game
            if (!session.gameActive) {
                startChessGame(p, playerIndex, session);
            } else {
                p.client.println("You are already in a chess game.");
            }
        }
        else {
            p.client.println("Unknown game number. Use 'play' to see available games.");
        }
        return;
    }

    if (cmd == "rules") {
        // Check if player is in Game Parlor
        if (!(p.roomX == 247 && p.roomY == 248 && p.roomZ == 50)) {
            p.client.println("You can only view game rules in the Game Parlor!");
            return;
        }
        
        // If no game specified, show available games
        if (args.length() == 0) {
            p.client.println("Available games:");
            p.client.println("  1. High-Low Card Game");
            p.client.println("  2. Chess");
            p.client.println("Usage: rules 1");
            return;
        }
        
        int gameNum = args.toInt();
        
        // Game 1: High-Low Rules
        if (gameNum == 1) {
            p.client.println("===================== RULES FOR High-Low Card Game =====================");
            p.client.println("The game starts with the dealer dealing you 2 cards.");
            p.client.println("");
            p.client.println("OBJECT:");
            p.client.println("- You are betting if the 3rd card dealt value is between");
            p.client.println("  the first two card values.");
            p.client.println("");
            p.client.println("OUTCOMES:");
            p.client.println("- WIN: 3rd card is STRICTLY INSIDE range [card1, card2]");
            p.client.println("- LOSE: 3rd card is OUTSIDE range");
            p.client.println("- POST: 3rd card equals 1st or 2nd card (lose 2x bet)");
            p.client.println("");
            p.client.println("BETTING:");
            p.client.println("- Minimum bet: 10gp");
            p.client.println("- Maximum bet: up to half your coins (to afford 2x loss)");
            p.client.println("- Special: Type 'pot' to bet the entire pot amount");
            p.client.println("");
            p.client.println("CARD RULES:");
            p.client.println("- First Ace: Player declares HIGH (2) or LOW (1)");
            p.client.println("- Two Aces: 2nd Ace auto-set to opposite (e.g., 1st=LOW, 2nd=HIGH)");
            p.client.println("- Same Value: 2nd card automatically redrawn (e.g., 4♥ and 4♠)");
            p.client.println("- No Gap: 2nd card redrawn if gap of 1 (e.g., 4 and 5, 10 and J)");
            p.client.println("");
            p.client.println("POT: Current POT is at " + String(globalHighLowPot) + "gp");
            p.client.println("");
            p.client.println("NOTE:");
            p.client.println("  * Dealer uses a DOUBLE DECK (104 cards) to draw from");
            p.client.println("  * The deck is shuffled before every game starts");
            p.client.println("  * Dealer draws from deck until depleted,");
            p.client.println("    then shuffles a new deck and continues");
            p.client.println("");
            p.client.println("  *** If you are good at COUNTING CARDS, take advantage! ***");
            p.client.println("========================================================================");
        }
        // Game 2: Chess Rules
        else if (gameNum == 2) {
            p.client.println("===================== RULES FOR CHESS =====================");
            p.client.println("OBJECT:");
            p.client.println("- Defeat the chess engine by checkmating the king");
            p.client.println("- The player is assigned Black or White alternately");
            p.client.println("");
            p.client.println("GAMEPLAY:");
            p.client.println("- Enter moves in algebraic notation: d2d4 (from d2 to d4)");
            p.client.println("- White moves first");
            p.client.println("- Each player has 5 minutes per game");
            p.client.println("");
            p.client.println("BOARD:");
            p.client.println("- Standard 8x8 chess board");
            p.client.println("- Columns: a-h (left to right)");
            p.client.println("- Rows: 1-8 (bottom to top for White, top to bottom for Black)");
            p.client.println("");
            p.client.println("COMMANDS:");
            p.client.println("- Enter move: d2d4");
            p.client.println("- 'resign' : Give up the game");
            p.client.println("- 'end'    : Quit and return to Game Parlor");
            p.client.println("");
            p.client.println("ENGINE STRENGTH: ~1800 ELO rating");
            p.client.println("===========================================================");
        } else {
            p.client.println("Unknown game number. Use 'rules' to see available games.");
        }
        return;
    }

// -----------------------------------------
// EQUIPMENT: WEAR / REMOVE / WIELD / UNWIELD
// -----------------------------------------
    if (cmd == "wear") {
        if (args.length() == 0) {
            p.client.println("Wear what?");
            return;
        }
        cmdWear(p, args);
        return;
    }

    if (cmd == "remove") {
        if (args.length() == 0) {
            p.client.println("Remove what?");
            return;
        }
        cmdRemove(p, args);
        return;
    }

    if (cmd == "wield") {
        if (args.length() == 0) {
            p.client.println("Wield what?");
            return;
        }
        cmdWield(p, args);
        return;
    }

    if (cmd == "unwield") {
        cmdUnwield(p);
        return;
    }

// -----------------------------------------
// COMBAT: ATTACK / KILL
// -----------------------------------------
    if (cmd == "attack" || cmd == "kill") {
        if (args.length() == 0) {
            p.client.println("Attack what?");
            return;
        }
        cmdKill(p, args.c_str());
        return;
    }

// -----------------------------------------
// QUEST / MISC INTERACTION
// -----------------------------------------
    if (cmd == "quest") {
        cmdQuestList(p);
        return;
    }

    if (cmd == "questlist") {
        cmdQuestList(p);
        return;
    }

// -----------------------------------------
// PLAYER INFO: SCORE / LEVELS / ADVANCE /PASSWORD
// -----------------------------------------
    if (cmd == "score" || cmd == "sc") {
        cmdScore(p);
        return;
    }

    if (cmd == "levels") {
        cmdLevels(p);
        return;
    }

    if (cmd == "advance") {
        cmdAdvance(p);
        return;
    }


    if (cmd == "password") {
        cmdPassword(p, index);
        return;
    }


// -----------------------------------------
// HELP / WIZHELP / QRCODE
// -----------------------------------------
    if (cmd == "help") {
        cmdHelp(p);
        return;
    }

    if (cmd == "wizhelp") {
        if (!p.IsWizard) {
            p.client.println("What?");
            return;
        }
        cmdWizHelp(p);
        return;
    }

    if (cmd == "qrcode") {
        cmdQrCode(p, args);
        return;
    }

    if (cmd == "map") {
        cmdMap(p);
        return;
    }

    if (cmd == "townmap") {
        cmdTownMap(p);
        return;
    }

// -----------------------------------------
// SAVE / WIMP / QUIT
// -----------------------------------------
    if (cmd == "save") {
        savePlayerToFS(p);
        p.client.println("Saved.");
        return;
    }

    if (cmd == "wimp") {
        cmdWimp(p);
        return;
    }

    if (cmd == "q" || cmd == "quit") {
        // End any active High-Low game
        if (index >= 0 && index < MAX_PLAYERS && highLowSessions[index].gameActive) {
            highLowSessions[index].gameActive = false;
            highLowSessions[index].awaitingAceDeclaration = false;
            highLowSessions[index].awaitingContinue = false;
        }
        
        // End any active Chess game
        if (index >= 0 && index < MAX_PLAYERS && chessSessions[index].gameActive) {
            chessSessions[index].gameActive = false;
        }
        
        // Set spawn room before saving
        p.roomX = 250;
        p.roomY = 250;
        p.roomZ = 50;
        
        savePlayerToFS(p);
        
        // Log session logout
        logSessionLogout(p.name);

        announceToRoom(
            250, 250, 50,
            String(capFirst(p.name)) + " fades from existence.",
            index
        );

        p.client.println("Goodbye!");
        p.client.stop();
        p.active = false;
        p.loggedIn = false;
        return;
    }


// =============================
// Command parser (SECTION 4/4)
// WIZARD COMMANDS, EMOTES, UNKNOWN
// =============================

// -----------------------------------------
// WIZARD COMMANDS
// -----------------------------------------
    if (cmd == "resetworlditems") {
        if (!p.IsWizard) { p.client.println("What?"); return; }
        cmdResetWorldItems(p, args);
        return;
    }

    if (cmd == "heal") {
        if (!p.IsWizard) { p.client.println("What?"); return; }
        cmdHeal(p, args);
        return;
    }

    if (cmd == "entermsg") {
        if (!p.IsWizard) { p.client.println("What?"); return; }
        cmdEnterMsg(p, args);
        return;
    }

    if (cmd == "exitmsg") {
        if (!p.IsWizard) { p.client.println("What?"); return; }
        cmdExitMsg(p, args);
        return;
    }

    if (cmd == "goto") {
        if (!p.IsWizard) { p.client.println("What?"); return; }
        cmdGoto(p, args);
        return;
    }

    if (cmd == "stats") {
        if (!p.IsWizard) { p.client.println("What?"); return; }

        p.showStats = !p.showStats;
        p.client.println(p.showStats ?
            "Wizard stats output: ON" :
            "Wizard stats output: OFF");
        return;
    }

    if (cmd == "invis") {
        if (!p.IsWizard) { p.client.println("What?"); return; }

        bool wasInvisible = p.IsInvisible;
        p.IsInvisible = !p.IsInvisible;

        // Turning invisible
        if (!wasInvisible && p.IsInvisible) {
            p.client.println("You fade from sight.");
            announceToRoom(
                p.roomX, p.roomY, p.roomZ,
                capFirst(p.name) + " fades from sight.",
                index
            );
            return;
        }

        // Becoming visible again
        if (wasInvisible && !p.IsInvisible) {
            p.client.println("You materialize out of thin air!");
            announceToRoom(
                p.roomX, p.roomY, p.roomZ,
                capFirst(p.name) + " materializes out of thin air!",
                index
            );
            return;
        }
    }

    if (cmd == "clone") {
        if (!p.IsWizard) { p.client.println("What?"); return; }

        // Build categorized lists from ALL item definitions
        std::vector<String> weaponIds, weaponNames;
        std::vector<String> armorIds, armorNames;
        std::vector<String> miscIds, miscNames;
        
        for (auto& pair : itemDefs) {
            String itemId = String(pair.first.c_str());
            String displayName = itemId;  // Default to ID
            
            // Try to get "name" attribute
            auto it = pair.second.attributes.find("name");
            if (it != pair.second.attributes.end()) {
                displayName = String(it->second.c_str());
            }
            
            // Determine category from "type" attribute
            String itemType = "misc";  // Default to misc
            auto typeIt = pair.second.attributes.find("type");
            if (typeIt != pair.second.attributes.end()) {
                itemType = String(typeIt->second.c_str());
            }
            
            if (itemType == "weapon") {
                weaponIds.push_back(itemId);
                weaponNames.push_back(displayName);
            } else if (itemType == "armor") {
                armorIds.push_back(itemId);
                armorNames.push_back(displayName);
            } else {
                miscIds.push_back(itemId);
                miscNames.push_back(displayName);
            }
        }

        // Build combined map for easy lookup by index
        std::vector<String> allItemIds;
        std::vector<String> allItemNames;
        std::vector<String> allItemTypes;
        
        // Add weapons
        for (size_t i = 0; i < weaponIds.size(); i++) {
            allItemIds.push_back(weaponIds[i]);
            allItemNames.push_back(weaponNames[i]);
            allItemTypes.push_back("weapon");
        }
        
        // Add armor
        for (size_t i = 0; i < armorIds.size(); i++) {
            allItemIds.push_back(armorIds[i]);
            allItemNames.push_back(armorNames[i]);
            allItemTypes.push_back("armor");
        }
        
        // Add misc
        for (size_t i = 0; i < miscIds.size(); i++) {
            allItemIds.push_back(miscIds[i]);
            allItemNames.push_back(miscNames[i]);
            allItemTypes.push_back("misc");
        }

        // Build list from ALL NPC definitions (not just instances)
        std::vector<String> allNpcIds;
        std::vector<String> allNpcNames;
        for (auto& pair : npcDefs) {
            String npcId = String(pair.first.c_str());
            String displayName = npcId;  // Default to ID
            
            // Try to get "name" attribute
            auto it = pair.second.attributes.find("name");
            if (it != pair.second.attributes.end()) {
                displayName = String(it->second.c_str());
            }
            
            allNpcIds.push_back(npcId);
            allNpcNames.push_back(displayName);
        }

        // If no argument, show list and usage
        if (args.length() == 0) {
            int enumCounter = 1;
            
            // Display weapons
            if (weaponNames.size() > 0) {
                p.client.println("WEAPONS:");
                String itemLine = "";
                for (size_t i = 0; i < weaponNames.size(); i++) {
                    String entry = String(enumCounter) + ") " + weaponNames[i];
                    if (itemLine.length() + entry.length() + 1 > 80 && itemLine.length() > 0) {
                        p.client.println(itemLine);
                        itemLine = entry;
                    } else {
                        if (itemLine.length() > 0) itemLine += " ";
                        itemLine += entry;
                    }
                    enumCounter++;
                }
                if (itemLine.length() > 0) {
                    p.client.println(itemLine);
                }
                p.client.println("");
            }
            
            // Display armor
            if (armorNames.size() > 0) {
                p.client.println("ARMOR:");
                String itemLine = "";
                for (size_t i = 0; i < armorNames.size(); i++) {
                    String entry = String(enumCounter) + ") " + armorNames[i];
                    if (itemLine.length() + entry.length() + 1 > 80 && itemLine.length() > 0) {
                        p.client.println(itemLine);
                        itemLine = entry;
                    } else {
                        if (itemLine.length() > 0) itemLine += " ";
                        itemLine += entry;
                    }
                    enumCounter++;
                }
                if (itemLine.length() > 0) {
                    p.client.println(itemLine);
                }
                p.client.println("");
            }
            
            // Display misc items
            if (miscNames.size() > 0) {
                p.client.println("MISC:");
                String itemLine = "";
                for (size_t i = 0; i < miscNames.size(); i++) {
                    String entry = String(enumCounter) + ") " + miscNames[i];
                    if (itemLine.length() + entry.length() + 1 > 80 && itemLine.length() > 0) {
                        p.client.println(itemLine);
                        itemLine = entry;
                    } else {
                        if (itemLine.length() > 0) itemLine += " ";
                        itemLine += entry;
                    }
                    enumCounter++;
                }
                if (itemLine.length() > 0) {
                    p.client.println(itemLine);
                }
                p.client.println("");
            }

            // Display available NPCs
            if (allNpcNames.size() > 0) {
                p.client.println("NPCs:");
                String npcLine = "";
                for (size_t i = 0; i < allNpcNames.size(); i++) {
                    String entry = String(enumCounter) + ") " + allNpcNames[i];
                    if (npcLine.length() + entry.length() + 1 > 80 && npcLine.length() > 0) {
                        p.client.println(npcLine);
                        npcLine = entry;
                    } else {
                        if (npcLine.length() > 0) npcLine += " ";
                        npcLine += entry;
                    }
                    enumCounter++;
                }
                if (npcLine.length() > 0) {
                    p.client.println(npcLine);
                }
            }

            p.client.println("");
            p.client.println("Usage: clone <number>");
            return;
        }

        // Parse the number argument
        int cloneNum = args.toInt();
        int totalCount = allItemNames.size() + allNpcNames.size();

        if (cloneNum < 1 || cloneNum > totalCount) {
            p.client.println("Invalid selection.");
            return;
        }

        if (cloneNum <= (int)allItemNames.size()) {
            // Clone an item from itemDefs
            String itemId = allItemIds[cloneNum - 1];
            auto it = itemDefs.find(std::string(itemId.c_str()));
            
            if (it == itemDefs.end()) {
                p.client.println("Item definition not found.");
                return;
            }

            // Create world item from definition
            WorldItem clonedItem;
            clonedItem.x = p.roomX;
            clonedItem.y = p.roomY;
            clonedItem.z = p.roomZ;
            clonedItem.name = itemId;
            clonedItem.ownerName = "";  // In world, not in inventory
            clonedItem.parentName = "";  // Not in a container
            clonedItem.value = 0;
            
            // Copy attributes from definition
            for (auto& attr : it->second.attributes) {
                clonedItem.attributes[attr.first] = attr.second;
            }
            
            worldItems.push_back(clonedItem);
            
            p.client.println("Cloned: " + allItemNames[cloneNum - 1]);
            announceToRoom(
                p.roomX, p.roomY, p.roomZ,
                allItemNames[cloneNum - 1] + " appears in a flash of light!",
                -1
            );
        } else {
            // Clone an NPC from npcDefs
            int npcIdx = cloneNum - (int)allItemNames.size() - 1;
            String npcId = allNpcIds[npcIdx];
            auto it = npcDefs.find(std::string(npcId.c_str()));
            
            if (it == npcDefs.end()) {
                p.client.println("NPC definition not found.");
                return;
            }

            // Create NPC instance from definition
            NpcInstance clonedNPC;
            clonedNPC.x = p.roomX;
            clonedNPC.y = p.roomY;
            clonedNPC.z = p.roomZ;
            clonedNPC.spawnX = p.roomX;
            clonedNPC.spawnY = p.roomY;
            clonedNPC.spawnZ = p.roomZ;
            clonedNPC.npcId = npcId;
            clonedNPC.parentId = "";
            clonedNPC.alive = true;
            clonedNPC.respawnTime = 0;
            clonedNPC.targetPlayer = -1;
            clonedNPC.suppressDeathMessage = false;
            
            // Get stats from attributes
            auto hpIt = it->second.attributes.find("hp");
            clonedNPC.hp = (hpIt != it->second.attributes.end()) ? 
                atoi(hpIt->second.c_str()) : 10;
            
            auto goldIt = it->second.attributes.find("gold");
            clonedNPC.gold = (goldIt != it->second.attributes.end()) ? 
                atoi(goldIt->second.c_str()) : 0;
            
            npcInstances.push_back(clonedNPC);
            
            p.client.println("Cloned: " + allNpcNames[npcIdx]);
            announceToRoom(
                p.roomX, p.roomY, p.roomZ,
                allNpcNames[npcIdx] + " materializes before you!",
                -1
            );
        }

        return;
    }

    if (cmd == "clonegold") {
        if (!p.IsWizard) { p.client.println("What?"); return; }

        if (args.length() == 0) {
            p.client.println("Usage: clonegold <amount>");
            return;
        }

        int amount = args.toInt();
        
        if (amount <= 0) {
            p.client.println("Amount must be greater than 0.");
            return;
        }

        spawnGoldAt(p.roomX, p.roomY, p.roomZ, amount);
        
        p.client.println("Created " + String(amount) + " gold coin(s).");
        announceToRoom(
            p.roomX, p.roomY, p.roomZ,
            String(amount) + " gold coin(s) appear in a flash of light!",
            -1
        );

        return;
    }

    if (cmd == "reboot") {
        if (!p.IsWizard) { p.client.println("What?"); return; }

        broadcastToAll("The world shimmers and reforms...");
        delay(200);

        savePlayerToFS(p);
        saveWorldItems();  // Save world state before reboot
        
        // Reset world to fresh state before reboot
        cmdResetWorldItems(p, "");
        
        safeReboot();
        return;
    }

    if (cmd == "restock" && args == "shop") {
        if (!p.IsWizard) { p.client.println("What?"); return; }

        restockAllShops();
        p.client.println("All shops have been fully restocked.");
        return;
    }





// -----------------------------------------
// Debug commands (Wizard Only)
// -----------------------------------------
if (cmd == "debug") {

    if (!p.IsWizard) {
        p.client.println("What?");
        return;
    }

    String a = args;
    a.trim();

    // -----------------------------------------
    // No argument → show help
    // -----------------------------------------
    if (a.length() == 0) {
        p.client.println("Debug commands:");
        p.client.println("  debug delete <file>      - Delete a LittleFS file");
        p.client.println("  debug destination        - Toggle debug output between SERIAL and TELNET");
        p.client.println("  debug extract <file>     - Backup a single file (for pre-partition save)");
        p.client.println("  debug extractall         - Backup all LittleFS files");
        p.client.println("  debug files              - Dump core data files");
        p.client.println("  debug flashspace         - Show LittleFS total/used/free space");
        p.client.println("  debug items              - Dump world items");
        p.client.println("  debug list               - List all files in LittleFS root");
        p.client.println("  debug npcs               - Dump NPC definitions and instances");
        p.client.println("  debug online             - Show currently logged-in players with stats");
        p.client.println("  debug players            - Dump all player save files");
        p.client.println("  debug questflags         - Show quest flags");
        p.client.println("  debug sessions           - Show last 50 session log records");
        p.client.println("  debug ymodem             - Print YMODEM transfer debug log");
        p.client.println("  debug <player>           - Dump a single player save file");
        return;
    }

    // -----------------------------------------
    // debug destination (toggle)
    // -----------------------------------------
    if (a.equalsIgnoreCase("destination")) {
        if (p.debugDest == DEBUG_TO_SERIAL) {
            p.debugDest = DEBUG_TO_TELNET;
            p.client.println("Debug destination set to: TELNET");
        } else {
            p.debugDest = DEBUG_TO_SERIAL;
            p.client.println("Debug destination set to: SERIAL");
        }
        return;
    }

    // -----------------------------------------
    // debug items
    // -----------------------------------------
    if (a == "items") {
        debugPrint(p, "=== DEBUG: worldItems ===");
        debugDumpItems(p);
        debugPrint(p, "=== END DEBUG ===");
        return;
    }

    // -----------------------------------------
    // debug files
    // -----------------------------------------
    if (a == "files") {
        debugPrint(p, "=== Dumping LittleFS files ===");
        debugDumpFiles(p);
        debugPrint(p, "=== End of dumps ===");
        return;
    }



    // -----------------------------------------
    // debug players
    // -----------------------------------------
    if (a == "players") {
        debugPrint(p, "=== DEBUG: PLAYER FILES ===");
        debugDumpPlayers(p);
        debugPrint(p, "=== END DEBUG ===");
        return;
    }

    // -----------------------------------------
    // debug npcs
    // -----------------------------------------
    if (a == "npcs") {
        debugPrint(p, "=== DEBUG: NPC DEFINITIONS & INSTANCES ===");
        debugDumpNPCs(p);
        debugPrint(p, "=== END DEBUG ===");
        return;
    }

    // -----------------------------------------
    // debug list
    // -----------------------------------------
    if (a == "list") {
        debugListAllFiles(p);
        return;
    }

    // -----------------------------------------
    // debug flashspace
    // -----------------------------------------
    if (a == "flashspace") {
        size_t total = LittleFS.totalBytes();
        size_t used  = LittleFS.usedBytes();
        size_t freeB = total - used;

        debugPrint(p, "=== LittleFS Flash Space ===");
        debugPrint(p, "Total : " + String(total / 1024) + " KB");
        debugPrint(p, "Used  : " + String(used  / 1024) + " KB");
        debugPrint(p, "Free  : " + String(freeB / 1024) + " KB");
        debugPrint(p, "============================");
        return;
    }

    // -----------------------------------------
    // debug sessions
    // -----------------------------------------
    if (a == "sessions") {
        File f = LittleFS.open("/session_log.txt", "r");
        if (!f) {
            debugPrint(p, "No session log found.");
            return;
        }

        // Read all lines into a vector
        std::vector<String> allLines;
        while (f.available()) {
            String line = f.readStringUntil('\n');
            line.trim();
            if (line.length() > 0) {
                allLines.push_back(line);
            }
        }
        f.close();

        // Display last 50 lines in reverse order (most recent first)
        int startIdx = 0;
        if (allLines.size() > 50) {
            startIdx = allLines.size() - 50;
        }

        debugPrint(p, "=== SESSION LOG (Last 50 Records - Most Recent First) ===");
        for (int i = (int)allLines.size() - 1; i >= startIdx; i--) {
            debugPrint(p, allLines[i]);
        }
        debugPrint(p, "==========================================================");
        return;
    }

    // -----------------------------------------
    // debug ymodem
    // -----------------------------------------
    if (a == "ymodem") {
        File f = LittleFS.open("/ymodem_debug.txt", "r");
        if (!f) {
            debugPrint(p, "No YMODEM debug log found.");
            return;
        }

        debugPrint(p, "=== YMODEM DEBUG LOG ===");

        while (f.available()) {
            String line = f.readStringUntil('\n');
            line.trim();
            debugPrint(p, line);
        }

        f.close();
        return;
    }

    // -----------------------------------------
    // debug delete <file>
    // -----------------------------------------
    if (a.startsWith("delete ")) {
        String fname = a.substring(7);
        fname.trim();

        if (!fname.startsWith("/"))
            fname = "/" + fname;

        if (!LittleFS.exists(fname)) {
            debugPrint(p, "File not found: " + fname);
            return;
        }

        if (LittleFS.remove(fname))
            debugPrint(p, "Deleted: " + fname);
        else
            debugPrint(p, "FAILED to delete: " + fname);

        return;
    }

    // -----------------------------------------
    // debug extract <filename> - Backup files before partition changes
    // -----------------------------------------
    if (a.startsWith("extract ")) {
        String fname = a.substring(8);
        fname.trim();

        if (!fname.startsWith("/"))
            fname = "/" + fname;

        if (!LittleFS.exists(fname)) {
            debugPrint(p, "File not found: " + fname);
            return;
        }

        File f = LittleFS.open(fname, "r");
        if (!f) {
            debugPrint(p, "Cannot open: " + fname);
            return;
        }

        size_t fileSize = f.size();
        debugPrint(p, "=== BACKUP: " + fname + " (" + String(fileSize) + " bytes) ===");
        
        // For text files, show as text
        if (fname.endsWith(".txt") || fname.endsWith(".csv") || fname.endsWith(".vxd")) {
            while (f.available()) {
                String line = f.readStringUntil('\n');
                debugPrint(p, line);
            }
        } else {
            // For binary files, show as hex dump
            debugPrint(p, "[HEX DUMP]");
            int bytesPerLine = 16;
            int lineNum = 0;
            while (f.available()) {
                String hexLine = "[" + String(lineNum * bytesPerLine, HEX) + "] ";
                for (int i = 0; i < bytesPerLine && f.available(); i++) {
                    uint8_t b = f.read();
                    if (b < 16) hexLine += "0";
                    hexLine += String(b, HEX) + " ";
                }
                debugPrint(p, hexLine);
                lineNum++;
            }
        }
        
        debugPrint(p, "=== END BACKUP ===");
        f.close();
        return;
    }

    // -----------------------------------------
    // debug extractall - Backup all LittleFS files
    // -----------------------------------------
    if (a == "extractall") {
        debugPrint(p, "=== BACKING UP ALL FILES ===");
        
        File root = LittleFS.open("/");
        File file = root.openNextFile();
        int fileCount = 0;
        
        while (file) {
            String fileName = file.name();
            size_t fileSize = file.size();
            
            debugPrint(p, "");
            debugPrint(p, "=== FILE: " + fileName + " (" + String(fileSize) + " bytes) ===");
            
            // Show text files as text
            if (fileName.endsWith(".txt") || fileName.endsWith(".csv") || fileName.endsWith(".vxd")) {
                file.seek(0);  // Reset to beginning
                while (file.available()) {
                    String line = file.readStringUntil('\n');
                    debugPrint(p, line);
                }
            } else {
                // Binary files as hex
                debugPrint(p, "[HEX DUMP]");
                file.seek(0);
                int bytesPerLine = 16;
                int lineNum = 0;
                while (file.available()) {
                    String hexLine = "[" + String(lineNum * bytesPerLine, HEX) + "] ";
                    for (int i = 0; i < bytesPerLine && file.available(); i++) {
                        uint8_t b = file.read();
                        if (b < 16) hexLine += "0";
                        hexLine += String(b, HEX) + " ";
                    }
                    debugPrint(p, hexLine);
                    lineNum++;
                }
            }
            
            fileCount++;
            file = root.openNextFile();
        }
        
        debugPrint(p, "");
        debugPrint(p, "=== BACKUP COMPLETE: " + String(fileCount) + " files ===");
        file.close();
        root.close();
        return;
    }

    // -----------------------------------------
    // debug online
    // -----------------------------------------
    if (a == "online") {
        debugPrint(p, "=== CURRENTLY LOGGED-IN PLAYERS ===");
        int onlineCount = 0;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (players[i].active && players[i].loggedIn) {
                onlineCount++;
                String status = "";
                status += "  [Slot " + String(i) + "] ";
                status += String(capFirst(players[i].name));
                status += " - Level " + String(players[i].level);
                status += " " + String(raceNames[players[i].raceId]);
                status += " (HP: " + String(players[i].hp) + "/" + String(players[i].maxHp) + ")";
                status += " at (" + String(players[i].roomX) + ","
                       + String(players[i].roomY) + ","
                       + String(players[i].roomZ) + ")";
                if (players[i].IsWizard) status += " [WIZARD]";
                if (players[i].IsInvisible) status += " [INVISIBLE]";
                debugPrint(p, status);
            }
        }
        if (onlineCount == 0) {
            debugPrint(p, "  (No players online)");
        } else {
            debugPrint(p, "Total online: " + String(onlineCount));
        }
        debugPrint(p, "=== END DEBUG ===");
        return;
    }

    // -----------------------------------------
    // debug <playername>
    // -----------------------------------------
    debugDumpSinglePlayer(p, a);
    return;
}

    // -----------------------------------------
    // Mapper voxel toggle
    // -----------------------------------------
    if (cmd == "voxel:") {
        p.sendVoxel = true;
        p.client.println("Voxel output enabled.");
        return;
    }



    // -----------------------------------------
    // Social/Emotes(actions)
    // -----------------------------------------
    if (cmd == "say") {
        cmdSay(p, args.c_str());
        return;
    }
    if (cmd == "shout") {
        cmdShout(p, args.c_str());
        return;
    }
    if (cmd == "actions") {
        cmdActions(p);
        return;
    }

// -----------------------------------------
// EMOTE DISPATCH
// -----------------------------------------
    {
        int e = findEmote(cmd);
        if (e >= 0) {
            executeEmote(p, index, cmd, args);
            return;
        }
    }

// -----------------------------------------
// HIGH-LOW GAME INPUT PROCESSING
// -----------------------------------------
    // Check if player is in an active High-Low game session
    int playerIndex = -1;
    for (int i = 0; i < MAX_PLAYERS; i++) {
        if (&players[i] == &p) {
            playerIndex = i;
            break;
        }
    }
    
    if (playerIndex >= 0 && highLowSessions[playerIndex].gameActive) {
        HighLowSession &session = highLowSessions[playerIndex];
        
        // Check if awaiting continuation after hand
        if (session.awaitingContinue) {
            if (cmd == "" || cmd.length() == 0) {
                // Empty input - start next hand
                session.awaitingContinue = false;
                p.client.println("");
                dealHighLowHand(p, playerIndex);
                return;
            } else if (cmd == "end" || cmd == "quit") {
                // End the game
                endHighLowGame(p, playerIndex);
                return;
            } else {
                // Invalid input during continue prompt
                p.client.println("Press [Enter] to continue or type 'end'");
                return;
            }
        }
        // Check if awaiting Ace declaration
        else if (session.awaitingAceDeclaration) {
            if (cmd == "1") {
                declareAceValue(p, playerIndex, 1);  // Low
                return;
            } else if (cmd == "2") {
                declareAceValue(p, playerIndex, 2);  // High
                return;
            } else if (cmd == "end" || cmd == "quit") {
                endHighLowGame(p, playerIndex);
                return;
            }
        } else {
            // Process betting input
            if (cmd == "end" || cmd == "quit") {
                endHighLowGame(p, playerIndex);
                return;
            } else if (cmd == "pot") {
                // Bet the entire pot
                int potBet = globalHighLowPot;
                if (p.coins < potBet * 2) {
                    // Need double the pot to cover possible losses
                    p.client.println("You need " + String(potBet * 2) + "gp to bet the pot!");
                    p.client.println("Enter bet amount, 'pot' or 'end':");
                    return;
                }
                processHighLowBet(p, playerIndex, potBet, true);  // true = pot bet
                return;
            } else {
                // Try to parse as a number (bet amount)
                int bet = cmd.toInt();
                if (bet > 0 || cmd == "0") {
                    // Valid numeric bet or pass (0)
                    processHighLowBet(p, playerIndex, bet, false);  // false = regular bet
                    return;
                }
            }
        }
    }
    
    // Check if player is in an active Chess game session
    if (playerIndex >= 0 && chessSessions[playerIndex].gameActive) {
        ChessSession &session = chessSessions[playerIndex];
        
        if (cmd == "resign") {
            endChessGame(p, playerIndex);
            return;
        } else if (cmd == "end" || cmd == "quit") {
            endChessGame(p, playerIndex);
            return;
        } else {
            // Chess moves only allowed in the Game Parlor!
            if (!(p.roomX == 247 && p.roomY == 248 && p.roomZ == 50)) {
                p.client.println("Chess moves can only be made in the Game Parlor!");
                return;
            }
            // Try to process as a chess move (combine cmd and args for full move notation)
            String moveStr = cmd;
            if (args.length() > 0) {
                moveStr += args;
            }
            processChessMove(p, playerIndex, session, moveStr);
            return;
        }
    }

// -----------------------------------------
// UNKNOWN COMMAND
// -----------------------------------------
    p.client.println("Unknown command.");
}






















// =============================
// Networking: read lines from client
// =============================
String readClientLine(WiFiClient &c) {
    String line = "";

    unsigned long start = millis();
    while (true) {
        if (millis() - start > 60000UL) break; // timeout

        if (!c.available()) {
            delay(1);
            continue;
        }

        char ch = c.read();

        // Telnet IAC negotiation
        if ((unsigned char)ch == 255) {
            if (c.available()) c.read();
            if (c.available()) c.read();
            continue;
        }

        // Ignore CR
        if (ch == '\r') continue;

        // Ignore backspace and delete
        if (ch == 8 || ch == 127) continue;

        // LF ends the line
        if (ch == '\n') break;

        // Normal character
        line += ch;
    }

    line.trim();
    return line;
}
 



 // ============================
// setup()
// =============================


void setup() {
    Serial.begin(115200);
    
    // Wait for USB CDC to be ready (up to 5 seconds)
    unsigned long startTime = millis();
    while (!Serial && millis() - startTime < 5000) {
        delay(100);
    }
    delay(500);  // Extra buffer for serial to stabilize
    
    // Print early boot signal
    Serial.println("CCCCC");
    
    // Detect Serial availability
    delay(50);
    bool serialAlive = Serial;
    NoSerial = !serialAlive;

    // -------------------------------
    // Mount FS silently
    // -------------------------------
    LittleFS.begin(true);

    // Always start with a clean YMODEM log
    LittleFS.remove("/ymodem_debug.txt");

    // Remove any leftover shops.txt from previous development (not part of current code)
    if (LittleFS.exists("/shops.txt")) {
        LittleFS.remove("/shops.txt");
        Serial.println("[BOOT] Removed legacy shops.txt file");
    }

    // =====================================================
    // YMODEM UPLOAD WINDOW (SKIPPED IF NO SERIAL)
    // =====================================================
    if (!NoSerial) {
        g_inYmodem = true;
        bool gotTransfer = ymodem_receiveSession(5000);
        g_inYmodem = false;

        if (gotTransfer) {
            Serial.println("[YMODEM] Session complete. Rebooting into MUD...");
            delay(500);
            safeReboot();
        }
    } else {
        Serial.println("NoSerial detected — skipping YMODEM window.");
    }

    Serial.println("5 second window reached: Booting MUD");

    // =====================================================
    // WIFI PROVISIONING + FALLBACK LOGIC
    // =====================================================
    const char* credPath = "/credentials.txt";

    bool credsExist = LittleFS.exists(credPath);
    bool wifiConnected = false;

    String ssid, pass, portStr;

    if (credsExist) {
        File f = LittleFS.open(credPath, "r");
        ssid = f.readStringUntil('\n'); ssid.trim();
        pass = f.readStringUntil('\n'); pass.trim();
        portStr = f.readStringUntil('\n'); portStr.trim();
        f.close();

        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), pass.c_str());

        unsigned long start = millis();
        while (WiFi.status() != WL_CONNECTED && millis() - start < 8000) {
            delay(200);
        }

        wifiConnected = (WiFi.status() == WL_CONNECTED);
        
        // If WiFi connected, sync time from NTP
        if (wifiConnected) {
            syncTimeFromNTP();
        }
    }

    if (!credsExist) {
        Serial.println("No credentials found — entering provisioning mode.");
        goto PROVISIONING_MODE;
    }
    else if (wifiConnected) {
        Serial.println("WiFi OK — skipping provisioning.");
    }
    else if (NoSerial) {
        Serial.println("WiFi failed but no Serial — fallback to last known creds.");
    }
    else {
        Serial.println("WiFi failed — entering provisioning mode.");
        goto PROVISIONING_MODE;
    }

    // =====================================================
    // NORMAL CONNECTION MODE
    // =====================================================
    Serial.println(ssid);
    Serial.println(pass);
    Serial.println(portStr);

    if (WiFi.status() != WL_CONNECTED) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), pass.c_str());

        while (WiFi.status() != WL_CONNECTED) {
            delay(500);
            Serial.print(".");
        }
    }

    Serial.println();
    Serial.println("WiFi connected!");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());

    // =====================================================
    // INITIALIZE TIMEZONE FROM TIME SERVER
    // =====================================================
    initializeTimezone();

    {
        int mudPort = portStr.toInt();
        server = new WiFiServer(mudPort);
        server->begin();

        Serial.print("MUD server started on port ");
        Serial.println(mudPort);
    }

    // =====================================================
    // INITIALIZE FILE UPLOAD SERVER (Port 8080)
    // =====================================================
    {
        fileUploadServer = new WiFiServer(FILE_UPLOAD_PORT);
        fileUploadServer->begin();

        Serial.print("File upload server started on port ");
        Serial.println(FILE_UPLOAD_PORT);
        Serial.println("  Access: http://<device-ip>:8080");
    }

    // Initialize players
    for (int i = 0; i < MAX_PLAYERS; i++) {
        players[i].active = false;
        players[i].loggedIn = false;
        players[i].invCount = 0;
        players[i].wieldedItemIndex = -1;
        for (int s = 0; s < SLOT_COUNT; s++)
            players[i].wornItemIndices[s] = -1;
    }

    // =====================================================
    // ⭐ NOW LOAD THE WORLD — AFTER ALL REBOOTS ARE DONE ⭐
    // =====================================================

    resetWorldState();              // wipe world
    loadAllItemDefinitions();       // load item templates
    loadAllWorldItems();            // load items.vxi
    loadAllNPCDefinitions();        // load NPC templates
    loadAllNPCInstances();          // load NPC spawns
    loadQuests();                   // load quests
    buildRoomIndexesIfNeeded();     // build room lookup tables
    initializeShops();              // initialize room-based shops
    initializeTaverns();            // initialize taverns with drinks
    initializePostOffices();        // initialize post offices

    // Initialize 6-hour reboot timer
    nextGlobalRespawn = millis() + GLOBAL_RESPAWN_INTERVAL;
    warned5min  = false;
    warned2min  = false;
    warned1min  = false;
    warned30sec = false;
    warned5sec  = false;

    return;

    // =====================================================
    // PROVISIONING MODE
    // =====================================================
PROVISIONING_MODE:

    {
        int waitCounter = 1;

        while (!LittleFS.exists(credPath)) {
            Serial.print("Waiting for credentials : ");
            Serial.println(waitCounter++);
            delay(5000);

            if (Serial.available() > 0) {
                File f = LittleFS.open(credPath, "w");
                if (f) {
                    delay(200);
                    while (Serial.available() > 0) {
                        f.write(Serial.read());
                    }
                    f.close();
                    Serial.println("SUCCESS: Credentials and Port Saved");
                    delay(500);
                    safeReboot();
                }
            }
        }
    }
}


//MAIN LOOP
void loop() {

    // ============================================================
    // BINARY TRANSFER MODE ALWAYS TAKES PRIORITY (legacy removed)
    // ============================================================
    // (No binary transfer code remains — correct)

    // ============================================================
    // SERIAL COMMANDS (LINE-BASED)
    // ============================================================
    if (Serial.available() > 0) {

        String cmd = Serial.readStringUntil('\n');
        cmd.trim();

        // ============================================================
        // NEW: ESP32 RESET COMMAND (from VB.NET)
        // ============================================================
        if (cmd == "ESP32_RESET") {
            Serial.println("ESP32_RESET_OK");
            delay(200);
            safeReboot();
            return;
        }

        // ============================================================
        // NORMAL SERIAL COMMANDS (LINE-BASED)
        // ============================================================
        if (cmd == "IP_Request") {
            Serial.print("MUD is Active! Connect with telnet at: ");
            Serial.print(WiFi.localIP());
            Serial.println(" 4000");
            return;
        }
    }

    // ============================================================
    // GAME LOGIC BELOW
    // ============================================================

    // Accept new players
    WiFiClient newClient = server->available();
    if (newClient) {

        for (int i = 0; i < MAX_PLAYERS; i++) {
            if (!players[i].active) {
                players[i].client = newClient;
                players[i].active = true;
                players[i].loggedIn = false;
                startLogin(players[i], i);
                break;
            }
        }
    }

    // Process player input
    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player &p = players[i];
        if (!p.active) continue;

        if (!p.client.connected()) {
            p.active = false;
            continue;
        }

        if (p.client.available()) {
            String line = readClientLine(p.client);

            // Check for High-Low continue prompt BEFORE empty line rejection
            if (i >= 0 && i < MAX_PLAYERS && highLowSessions[i].gameActive && highLowSessions[i].awaitingContinue) {
                // Player is waiting to continue a game - allow empty input
                if (line.length() == 0) {
                    // Empty input continues the game
                    highLowSessions[i].awaitingContinue = false;
                    p.client.println("");
                    dealHighLowHand(players[i], i);
                    p.client.print("> ");
                    continue;
                } else if (line == "end" || line == "quit") {
                    // End the game
                    endHighLowGame(players[i], i);
                    p.client.print("> ");
                    continue;
                } else if (line == "n" || line == "s" || line == "e" || line == "w" || line.startsWith("go ")) {
                    // Allow movement - will be processed by handleCommand
                    highLowSessions[i].awaitingContinue = false;
                    // Fall through to normal command processing
                } else {
                    // Invalid input
                    p.client.println("Press [Enter] to continue or type 'end'");
                    p.client.print("> ");
                    continue;
                }
            }

            if (line.length() == 0) {
                if (p.loggedIn) p.client.println("What?");
                continue;
            }

            if (!p.loggedIn) handleLogin(p, i, line);
            else handleCommand(players[i], i, line);
            p.client.print("> ");

        }
    }

    // Combat tick
    unsigned long now = millis();

    // ============================================================
    // ⭐ TIMED REBOOT COUNTDOWN (6-hour cycle)
    // ============================================================
    checkGlobalRebootCountdown(now);

    for (int i = 0; i < MAX_PLAYERS; i++) {
        Player &p = players[i];
        if (!p.active || !p.loggedIn) continue;
        if (p.inCombat && now >= p.nextCombatTime) doCombatRound(p);
    }



    // NPC respawn tick
for (auto &npc : npcInstances) {
    if (!npc.alive && npc.respawnTime > 0 && now >= npc.respawnTime) {

        npc.alive = true;
        npc.suppressDeathMessage = false;

        // SAFE lookup: convert npcId (String) → std::string
        std::string npcKey = std::string(npc.npcId.c_str());
        auto it = npcDefs.find(npcKey);
        if (it == npcDefs.end()) continue;

        NpcDefinition &def = it->second;

        // HP
        int hp = 1;
        auto hpIt = def.attributes.find("hp");
        if (hpIt != def.attributes.end()) {
            hp = strToInt(hpIt->second);
        }
        npc.hp = hp;

        // GOLD
        int gold = 0;
        auto goldIt = def.attributes.find("gold");
        if (goldIt != def.attributes.end()) {
            gold = strToInt(goldIt->second);
        }
        npc.gold = gold;

        npc.x = npc.spawnX;
        npc.y = npc.spawnY;
        npc.z = npc.spawnZ;
        npc.respawnTime = 0;

        npc.dialogIndex = 0;
        npc.dialogOrder[0] = 0;
        npc.dialogOrder[1] = 1;
        npc.dialogOrder[2] = 2;

        for (int i = 0; i < 3; i++) {
            int r = random(0, 3);
            int tmp = npc.dialogOrder[i];
            npc.dialogOrder[i] = npc.dialogOrder[r];
            npc.dialogOrder[r] = tmp;
        }

        npc.nextDialogTime = now + random(3000, 30001);
    }
}

    // NPC dialog tick
    for (auto &npc : npcInstances) {
        if (!npc.alive) continue;

        if (now >= npc.nextDialogTime) {

            // SAFE lookup
            std::string npcKey = std::string(npc.npcId.c_str());
            auto it = npcDefs.find(npcKey);
            if (it == npcDefs.end()) continue;

            NpcDefinition &def = it->second;

            int idx = npc.dialogOrder[npc.dialogIndex];
            String key = "dialog_" + String(idx + 1);

            // Convert key → std::string
            std::string skey = std::string(key.c_str());

            if (def.attributes.count(skey)) {

                // Convert stored std::string → Arduino String
                String line = String(def.attributes.at(skey).c_str());

                // Convert NPC name (std::string) → Arduino String
                String npcName = String(def.attributes.at("name").c_str());

                String msg =
                    "The " + npcName +
                    " says: \"" + line + "\"";

                announceToRoom(
                    npc.x, npc.y, npc.z,
                    msg,
                    -1
                );
            }

            npc.dialogIndex++;
            if (npc.dialogIndex >= 3) {
                npc.dialogIndex = 0;

                for (int i = 0; i < 3; i++) {
                    int r = random(0, 3);
                    int tmp = npc.dialogOrder[i];
                    npc.dialogOrder[i] = npc.dialogOrder[r];
                    npc.dialogOrder[r] = tmp;
                }
            }

            // Increase minimum dialog time in post office to reduce dialog spam
            int minDialogTime = (npc.x == 252 && npc.y == 248 && npc.z == 50) ? 30000 : 8000;
            int maxDialogTime = (npc.x == 252 && npc.y == 248 && npc.z == 50) ? 120001 : 30001;
            npc.nextDialogTime = now + random(minDialogTime, maxDialogTime);
        }
    }

    // Item dialog tick
    for (size_t i = 0; i < worldItems.size(); i++) {
        WorldItem &item = worldItems[i];
        
        // Only process world items (not in inventory)
        if (item.ownerName.length() > 0) continue;
        
        // Check if item has any dialog attributes (using dialog_1, dialog_2, dialog_3 format)
        auto dialog1 = item.attributes.find("dialog_1");
        auto dialog2 = item.attributes.find("dialog_2");
        auto dialog3 = item.attributes.find("dialog_3");
        
        if (dialog1 == item.attributes.end() && 
            dialog2 == item.attributes.end() && 
            dialog3 == item.attributes.end()) {
            continue;  // No dialogs for this item
        }
        
        // Count how many dialogs this item has
        int dialogCount = 0;
        if (dialog1 != item.attributes.end()) dialogCount++;
        if (dialog2 != item.attributes.end()) dialogCount++;
        if (dialog3 != item.attributes.end()) dialogCount++;
        
        // Initialize dialog timer if needed
        if (item.attributes.find("nextDialogTime") == item.attributes.end()) {
            // Increase minimum dialog time in post office to reduce dialog spam
            int minDialogTime = (item.x == 252 && item.y == 248 && item.z == 50) ? 30000 : 8000;
            int maxDialogTime = (item.x == 252 && item.y == 248 && item.z == 50) ? 120001 : 30001;
            item.attributes["nextDialogTime"] = std::to_string(now + random(minDialogTime, maxDialogTime));
        }
        
        unsigned long nextTime = (unsigned long)strtoull(item.attributes["nextDialogTime"].c_str(), NULL, 10);
        
        if (now >= nextTime) {
            // Pick dialog based on cycle order - only cycle through actual dialogs
            int dialogNum = item.dialogOrder[item.dialogIndex] + 1;  // Convert 0-2 to 1-3
            String dialogKey = "dialog_" + String(dialogNum);
            std::string skey = std::string(dialogKey.c_str());
            
            auto dialogIt = item.attributes.find(skey);
            if (dialogIt != item.attributes.end()) {
                String itemName = item.name;
                
                // Check if item has a custom "name" attribute
                auto nameIt = item.attributes.find("name");
                if (nameIt != item.attributes.end()) {
                    itemName = String(nameIt->second.c_str());
                }
                
                String line = String(dialogIt->second.c_str());
                String msg = "The " + itemName + " says: \"" + line + "\"";
                
                announceToRoom(item.x, item.y, item.z, msg, -1);
            }
            
            // Move to next dialog - only cycle through the dialogs that exist
            if (dialogCount == 1) {
                // Single dialog: only repeat if it's the ONLY one
                // Don't cycle, just repeat the same dialog
            } else {
                // Multiple dialogs: cycle through without repeating
                item.dialogIndex++;
                if (item.dialogIndex >= dialogCount) {
                    item.dialogIndex = 0;
                    
                    // Shuffle order for next cycle
                    for (int j = 0; j < dialogCount; j++) {
                        int r = random(0, dialogCount);
                        int tmp = item.dialogOrder[j];
                        item.dialogOrder[j] = item.dialogOrder[r];
                        item.dialogOrder[r] = tmp;
                    }
                }
            }
            
            // Schedule next dialog
            // Increase minimum dialog time in post office to reduce dialog spam
            int minDialogTime = (item.x == 252 && item.y == 248 && item.z == 50) ? 30000 : 8000;
            int maxDialogTime = (item.x == 252 && item.y == 248 && item.z == 50) ? 120001 : 30001;
            item.attributes["nextDialogTime"] = std::to_string(now + random(minDialogTime, maxDialogTime));
        }
    }




    // Natural healing
    static unsigned long lastHealTick = 0;
    if (now - lastHealTick >= 60000UL) {
        lastHealTick = now;
        for (int i = 0; i < MAX_PLAYERS; i++) {
            Player &p = players[i];
            if (!p.active || !p.loggedIn) continue;
            if (p.hp < p.maxHp) {
                p.hp += 1;
                if (p.hp > p.maxHp) p.hp = p.maxHp;
            }
            // Drunkenness recovery
            updateDrunkennessRecovery(p);
            // Fullness recovery
            updateFullnessRecovery(p);
        }
    }

    // Shop restock
    if (now - lastShopRestock >= 60UL * 60UL * 1000UL) {
        restockAllShops();
        lastShopRestock = now;
    }

    // =====================================================
    // HANDLE FILE UPLOADS VIA HTTP (Port 8080)
    // =====================================================
    if (fileUploadServer) {
        WiFiClient uploadClient = fileUploadServer->available();
        if (uploadClient) {
            handleFileUploadRequest(uploadClient);
        }
    }
}

// =====================================================
// FILE UPLOAD HANDLER - HTTP POST endpoint
// =====================================================
void handleFileUploadRequest(WiFiClient &client) {
    String request = "";
    unsigned long timeout = millis() + 2000;  // 2-second timeout
    
    // Read HTTP request headers
    while (client.available() && millis() < timeout) {
        char c = client.read();
        request += c;
        if (request.endsWith("\r\n\r\n")) break;
    }

    // Parse request line
    int firstLine = request.indexOf("\r\n");
    String requestLine = request.substring(0, firstLine);

    if (requestLine.indexOf("POST /upload") >= 0) {
        // Extract Content-Length
        int contentLenIdx = request.indexOf("Content-Length: ");
        int contentLen = 0;
        if (contentLenIdx >= 0) {
            int eol = request.indexOf("\r\n", contentLenIdx);
            String lenStr = request.substring(contentLenIdx + 16, eol);
            contentLen = lenStr.toInt();
        }

        // Find boundary (for multipart/form-data)
        int boundaryIdx = request.indexOf("boundary=");
        String boundary = "";
        if (boundaryIdx >= 0) {
            int end = request.indexOf("\r\n", boundaryIdx);
            boundary = request.substring(boundaryIdx + 9, end);
        }

        // Read file data
        String fileName = "unknown.txt";

        // Read remaining body until boundary
        String fileContent = "";
        while (client.available() && fileContent.length() < contentLen * 2) {
            fileContent += (char)client.read();
        }

        // Parse filename from the multipart Content-Disposition header (in the body)
        // Look for: Content-Disposition: form-data; name="file"; filename="shops.txt"
        int dispIdx = fileContent.indexOf("filename=");
        if (dispIdx >= 0) {
            int start = dispIdx + 9;
            // Skip quote if present
            if (fileContent[start] == '"') start++;
            
            int end = start;
            while (end < fileContent.length()) {
                char c = fileContent[end];
                if (c == '"' || c == ';' || c == '\r' || c == '\n') break;
                end++;
            }
            
            if (end > start) {
                fileName = fileContent.substring(start, end);
                fileName.trim();
                
                // Sanitize filename
                fileName.toLowerCase();
                for (int i = 0; i < fileName.length(); i++) {
                    char c = fileName[i];
                    if (!isalnum(c) && c != '.' && c != '_' && c != '-') {
                        fileName[i] = '_';
                    }
                }
            }
        }

        // Extract actual file data (between boundaries)
        int dataStart = fileContent.indexOf("\r\n\r\n");
        if (dataStart >= 0) {
            dataStart += 4;
            int dataEnd = fileContent.lastIndexOf("\r\n--");
            if (dataEnd > dataStart) {
                String actualData = fileContent.substring(dataStart, dataEnd);

                // Write to LittleFS
                String filePath = "/" + fileName;
                File f = LittleFS.open(filePath, "w");
                if (f) {
                    f.print(actualData);
                    f.close();
                    
                    // Send success response
                    client.println("HTTP/1.1 200 OK");
                    client.println("Content-Type: text/plain");
                    client.println("Connection: close");
                    client.println();
                    client.println("File uploaded successfully!");
                    client.println("File: " + fileName);
                    client.println("Size: " + String(actualData.length()) + " bytes");
                    client.println();
                    client.println("To check the file, use: DEBUG FILES");
                    
                    Serial.println("[FILE UPLOAD] Received: " + filePath + " (" + String(actualData.length()) + " bytes)");
                } else {
                    // Send error
                    client.println("HTTP/1.1 500 Internal Server Error");
                    client.println("Content-Type: text/plain");
                    client.println("Connection: close");
                    client.println();
                    client.println("Failed to write file");
                    Serial.println("[FILE UPLOAD] Failed to open: " + filePath);
                }
            }
        }
    } else if (requestLine.indexOf("GET /") >= 0) {
        // Serve simple upload HTML form
        String html = 
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: text/html\r\n"
            "Connection: close\r\n"
            "\r\n"
            "<!DOCTYPE html><html><body>"
            "<h1>ESP32 MUD - File Upload</h1>"
            "<form method='POST' action='/upload' enctype='multipart/form-data'>"
            "<input type='file' name='file' required>"
            "<button type='submit'>Upload</button>"
            "</form>"
            "<p>Supported files: shops.txt, items.vxd, items.vxi, npcs.vxd, npcs.vxi, quests.txt, rooms.txt</p>"
            "</body></html>";
        client.print(html);
    } else {
        client.println("HTTP/1.1 404 Not Found");
        client.println("Connection: close");
        client.println();
    }

    delay(100);
    client.stop();
}