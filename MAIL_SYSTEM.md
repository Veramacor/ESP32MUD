# Post Office Email System - Complete Implementation Guide

**Last Updated:** January 18, 2026 (Email retrieval v2.0)  
**Status:** Fully implemented and tested with real email delivery

## Overview

The Post Office is a fully functional email system allowing players to send and receive real emails. Emails sent via the `send` command are delivered to a real inbox, and incoming emails are automatically retrieved and spawned as letter items in the Post Office.

- **Location**: Voxel coordinates **252, 248, 50**
- **Email Account**: `esperthertu_post_office@storyboardacs.com`
- **POP3 Server**: `mail.storyboardacs.com:110`
- **Cost to Send**: 10 gold coins per email

## System Components

### 1. Server-Side: retrieveESP32mail.php (301 lines)

**Location**: `https://www.storyboardacs.com/retrieveESP32mail.php`  
**Protocol**: HTTP GET endpoint called by ESP32  
**Purpose**: POP3 email retrieval, parsing, and JSON response formatting

#### Key Features

- **POP3 Connection**: Uses fsockopen to mail.storyboardacs.com:110
- **Authentication**: PASS with hardcoded credentials
- **Email Filtering**:
  - Only processes emails TO: esperthertu_post_office@storyboardacs.com
  - Skips emails FROM: esperthertu_post_office@storyboardacs.com (system emails)
  - Only processes unread emails (NEW state)
- **MIME Parsing**: Extracts text/plain from multipart emails
- **Quoted-Printable Decoding**: Handles encoded email bodies
- **Smart Content Extraction**:
  - Removes quoted text (lines starting with ">")
  - Removes signatures ("--" delimiter)
  - Removes "On [date]..." quoted headers
  - Keeps only the actual reply text

#### Response Format

```json
{
  "success": true,
  "emails": [
    {
      "to": "esperthertu_post_office@storyboardacs.com",
      "from": "player@example.com",
      "subject": "Hello from the Real World",
      "body": "A letter from player@example.com.\n\nIt reads,\n\nThis is the actual message text without quotes or signatures.",
      "messageId": "<unique-message-id>",
      "displayName": "player"
    }
  ],
  "count": 1
}
```

### 2. Server-Side: sendESP32mail.php (existing)

**Purpose**: Sends player-composed emails via PHP mail() function  
**Triggered by**: `send [email] [message]` command  
**Cost**: 10 gold coins

Email format sent to recipients:
```
From: esperthertu_post_office@storyboardacs.com
To: [recipient@email.com]
Subject: Esperthertu Post Office Delivery!

A letter from sender@email.com.

It reads,

[Player's message text]
```

### 3. ESP32 Functions (ESP32MUD.cpp)

#### fetchMailFromServer() [Lines ~8435-8580]

**Purpose**: HTTP GET to retrieveESP32mail.php, parse JSON response  
**Returns**: true if successful, false on error

**Process**:
1. Build HTTPS request: `GET /retrieveESP32mail.php HTTP/1.0`
2. Connect with 5s timeout, socket 10s timeout, max 15s total
3. Parse response headers and body
4. Extract JSON portion
5. Unescape JSON special sequences: `\\r\\n`, `\\n`, `\\"`, `\\\\`
6. Split by `"emails":[` and extract array
7. Parse individual email objects: to, from, subject, body, messageId, displayName
8. Handle both compact JSON `"success":true` and spaced `"success": true` formats
9. Log comprehensive debug output with `========== MAIL FETCH ==========` markers

**Error Handling**:
- Connection timeout: Logs error, returns false
- Invalid JSON: Logs parsing errors with serial output
- Server errors: Checks for `"success":false`, logs reason

#### checkAndSpawnMailLetters(Player &p) [Lines ~8659-8730]

**Purpose**: Check for mail and spawn letters in the Post Office  
**Triggers**: 
- Player enters Post Office (automatic)
- `mail` / `checkmail` / `check mail` command (manual)
**Returns**: bool (true if mail found and spawned, false if no mail)

**Process**:
1. Call `fetchMailFromServer()` to retrieve emails
2. For each email received:
   - Extract sender's email address part (before @)
   - Use player name from matching `displayName` field if available
   - Create WorldItem letter at Post Office (252, 248, 50)
   - Set letter attributes:
     - `itemid`: "letter"
     - `type`: "letter"
     - `name`: "Letter for [PlayerName]" (capFirst applied)
     - `desc`: Full email body text
   - Add to world item vector
3. Save world items to `/items.vxi`
4. Announce mail arrival:
   - If mail found: Postal Clerk says "You've got mail!"
   - If no mail: Silent (no announcement)
5. Return true/false based on mail count

#### extractPlayerNameFromEmail(String emailBody) [Lines ~8618-8650]

**Purpose**: Parse email body to extract recipient player name  
**Searches for**: "a message from [playername]" pattern (case-insensitive)  
**Returns**: Extracted name or empty string if not found

**Pattern Matching**:
```
Email: "A message from Atew - The Warrior:\n\nHere Ye..."
Extract: "Atew"
```

Note: This function is for reference but letter naming now uses the current player's name directly.

#### movePlayer() Integration [Line ~4400]

```cpp
// Check for mail when entering post office
if (x == 252 && y == 248) {
    checkAndSpawnMailLetters(p);
}
```

Automatically triggers mail check after room description is displayed.

### 4. Post Office Room Description [Lines ~6489-6510]

```
"Post Office"
"You find yourself in a small but cheerful post office. A elderly man with thick 
spectacles sits behind the counter, sorting through letters. The smell of old parchment 
and ink fills the air. A sign above the counter reads 'Esperthertu Post Office'."
```

- **Shop**: Available (can buy postcards if implemented)
- **Dialog Trigger**: "You see the Postal Clerk sorting through the morning's mail."
- **Automatic Mail Check**: Triggered on room entry

## Player Workflow

### Sending Email

1. Go to Post Office (252, 248, 50)
2. Use command: `send player@example.com Hello from Esperthertu!`
3. Pay 10 gold coins
4. Email is sent via PHP mail function
5. Recipient sees email with Esperthertu Post Office header

### Receiving Email

1. Real email arrives at esperthertu_post_office@storyboardacs.com (POP3)
2. Player enters Post Office
3. Email is automatically retrieved via `checkAndSpawnMailLetters()`
4. Letter item spawned in room
5. Player sees: "You see A Letter for [PlayerName] lying in the post office."
6. Player commands:
   - `get letter` - Pick up the letter
   - `examine letter` - Read the email contents
   - `drop letter` - Leave it behind
   - `look` - See it in room description

### Manual Mail Check

1. Player at Post Office
2. Command: `mail` or `checkmail` or `check mail`
3. Response: "You've got mail!" (if new mail) or "No mail today, sorry." (if none)

## Data Flow Diagram

```
External Email
    ↓
POP3 Server (mail.storyboardacs.com)
    ↓
retrieveESP32mail.php (HTTP GET)
    ↓
JSON Response (compact format)
    ↓
fetchMailFromServer() (unescapes JSON)
    ↓
checkAndSpawnMailLetters() (creates letters)
    ↓
Letter Items in Post Office Room
    ↓
Player: `get letter` → `examine letter` → Read email
```

## Letter Item Attributes

Letters are created dynamically with these attributes:

| Attribute | Value | Purpose |
|-----------|-------|---------|
| itemid | "letter" | Item type identifier |
| type | "letter" | Room display filtering |
| name | "Letter for [PlayerName]" | Display name |
| desc | Email body text | Content shown when examined |
| weight | 0 | No carrying penalty |
| value | 0 | Cannot be sold |
| x, y, z | 252, 248, 50 | Post Office location |
| attributes.from | sender@email.com | Stored but not displayed |
| attributes.subject | Email subject | Stored but not displayed |

## Email Body Formatting

### Sent Email (to recipient)
```
From: esperthertu_post_office@storyboardacs.com
Subject: Esperthertu Post Office Delivery!

A letter from sender@email.com.

It reads,

[message body]
```

### Received Email (in letter)
```
A letter from sender@email.com.

It reads,

[reply text only - no quotes or signatures]
```

### Smart Parsing

The PHP `parseEmail()` function intelligently extracts reply text:

1. **MIME Boundaries**: Detects and skips multipart sections
2. **Quoted Text**: Removes lines starting with ">"
3. **Signatures**: Strips content after "--" delimiter
4. **Headers**: Removes "On [date]..., [person] wrote:" format
5. **Encoding**: Decodes quoted-printable (=3D, =20, etc.)

## Configuration

### Hard-coded Credentials (retrieveESP32mail.php)

```php
$mailserver = "mail.storyboardacs.com";
$mailport = 110;
$mailuser = "esperthertu_post_office@storyboardacs.com";
$mailpass = "VXN_jhn8bfe5cve7dve";
```

### HTTP Timeouts (ESP32MUD.cpp)

```cpp
// fetchMailFromServer()
Connection Timeout: 5 seconds
Socket Timeout: 10 seconds
Max Total: 15 seconds
```

## Testing & Verification

### 1. Send Email Test
```bash
Command: send test@example.com Hello World
Expected: "Mail sent to test@example.com for 10 gold."
Check: test@example.com receives email
```

### 2. Receive Email Test
```bash
Send email to: esperthertu_post_office@storyboardacs.com
Subject: Test Message
Body: A message from TestPlayer - The Adventurer:

Reply text here

Expected in MUD: Letter item appears in Post Office
```

### 3. Mail Command Test
```bash
At Post Office, type: mail
Expected: "You've got mail!" (if mail exists) or "No mail today, sorry."
```

## Flash Memory Usage

- **Current**: 61.5% (1,289,194 / 2,097,152 bytes)
- **Remaining**: ~789 KB
- **Components**:
  - Main game engine (11,400+ lines C++)
  - Email system functions (~300 lines)
  - HTTP/JSON parsing (Bluetooth, WiFi, SSL)
  - Item/NPC/Room definitions
  - LittleFS support

## Limitations & Notes

1. **Single Post Office**: One location (252, 248, 50) with email account
2. **Letters Don't Persist**: Unless carried in inventory across reboot
3. **Recipient Matching**: Uses player name for "Letter for [Name]"
4. **No Email Verification**: Accepts any email TO the account
5. **POP3 Only**: No IMAP or SMTP direct support (uses PHP)
6. **MIME Parsing**: Handles multipart/alternative emails correctly
7. **Encoding**: Supports quoted-printable and basic UTF-8

## Future Enhancements

- [ ] Letter archiving system (store in player file)
- [ ] Letter expiration (auto-delete after N days)
- [ ] Multiple Post Offices per realm
- [ ] Forwarding letters to other players
- [ ] Letter attachment support (QR codes)
- [ ] Email storage limit per player
- [ ] Read/unread letter tracking
- [ ] Reply-to functionality

## Troubleshooting

### Issue: "No mail today, sorry" when mail should exist
- **Check**: Is email TO esperthertu_post_office@storyboardacs.com?
- **Check**: Is email marked as NEW/unread in mailbox?
- **Check**: PHP file accessible at https://www.storyboardacs.com/retrieveESP32mail.php?
- **Solution**: Test PHP endpoint directly in browser

### Issue: Letter shows empty body
- **Cause**: MIME parsing not extracting text/plain correctly
- **Solution**: Check email source for multipart structure
- **Debug**: Enable serial logging, check parseEmail() output

### Issue: Player name not extracting
- **Cause**: Email body doesn't match "A message from [Name]" pattern
- **Solution**: Use correct email format when sending
- **Fallback**: Uses sender email part (before @) if name not found

### Issue: HTTP timeout errors
- **Cause**: Network connectivity or slow server
- **Solution**: Check WiFi signal strength
- **Debug**: Monitor serial output for connection details
