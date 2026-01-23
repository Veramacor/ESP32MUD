# Mail Retrieval System - Implementation Notes

## Overview
The Post Office now includes a complete email retrieval system that automatically fetches incoming emails when a player enters the Post Office and spawns them as letter items in the world.

## System Architecture

### Components

#### 1. **retrieveESP32mail.php** (PHP Endpoint)
- **Location**: Must be uploaded to `https://www.storyboardacs.com/retrieveESP32mail.php`
- **Purpose**: Retrieves unread emails from POP3 mailbox, marks them as read
- **Protocol**: HTTP GET with optional `player` parameter for logging
- **Response**: JSON array of email objects
  ```json
  {
    "success": true,
    "emails": [
      {
        "to": "recipient@example.com",
        "from": "sender@example.com", 
        "subject": "Subject Line",
        "body": "Email body text",
        "messageId": "unique-id"
      }
    ],
    "count": 1,
    "errors": []
  }
  ```

#### 2. **ESP32 Mail Functions**

**fetchMailFromServer(playerName, &letters)**
- Makes HTTP GET request to PHP endpoint
- Parses JSON response containing email array
- Handles JSON unescaping for special characters
- Returns `true` if successful, populates letters vector
- Includes error logging for debugging

**checkAndSpawnMailLetters(player)**
- Called automatically when player enters Post Office
- Fetches mail from server for that player
- Extracts recipient name from email body by searching for "a message from [name]"
- Creates world letter items:
  - itemid = 'letter'
  - type = 'letter'
  - name = "A Letter addressed to [playername]" or "An unaddressed Letter"
  - desc = email body text
  - Additional attributes: subject, from
- Spawns items in post office room (not in inventory)
- Announces letter arrival to player

**extractPlayerNameFromEmail(emailBody)**
- Helper function to parse email body
- Searches for "a message from [playername]" (case-insensitive)
- Extracts the name between "a message from " and the next delimiter (-, :, newline)
- Returns empty string if pattern not found

### Workflow

1. **Player enters Post Office** → `movePlayer()` called
2. **Player room loaded** → `loadRoomForPlayer()` completes
3. **Check triggered** → `checkAndSpawnMailLetters()` called
4. **HTTP request** → ESP32 calls `https://www.storyboardacs.com/retrieveESP32mail.php?player=playername`
5. **PHP connects** → Connects to POP3 server (mail.storyboardacs.com:110)
6. **Emails retrieved** → Fetches all unread messages
7. **Messages marked** → Emails deleted from POP3 (marked as read)
8. **JSON response** → PHP returns array of email objects
9. **Letters spawned** → ESP32 creates letter items in post office room
10. **Player notified** → "You see A Letter addressed to [player] lying in the post office."

### Email Message Format

The email system expects emails sent from `sendESP32mail.php` to have this body format:

```
A message from PlayerName - The PlayerTitle:

Here Ye, Here Ye. A message from the realm of Esperthertu has been delivered to you!

[Player's actual message text]
```

The `extractPlayerNameFromEmail()` function searches for this pattern to identify the recipient.

### Letter Items

Created with these attributes:
- **itemid**: 'letter'
- **type**: 'letter'
- **name**: Display name (e.g., "A Letter addressed to Atew")
- **desc**: Full email body text
- **subject**: Email subject line (stored but not displayed when examining)
- **from**: Sender email address (stored but not displayed)
- **weight**: 0 (no carrying penalty)
- **value**: 0 (cannot be sold)
- **x, y, z**: Post Office location (252, 248, 50)

### Player Interaction

1. Player enters Post Office
2. Letters appear: "You see A Letter addressed to [player] lying in the post office."
3. Player uses `get letter` to pick up the letter
4. Player uses `examine letter` or `look at letter` to read the contents
5. Letter shows in `inventory` as "letter"
6. **Letters don't persist through reboot** unless in player inventory

### Unread Email Handling

- PHP retrieves only unread emails from POP3
- After retrieval, emails are marked as deleted (DELE command)
- Next sync will not re-deliver same emails
- Empty mailbox on next visit (no new mail)

## Deployment Checklist

- [ ] Upload `retrieveESP32mail.php` to `https://www.storyboardacs.com/retrieveESP32mail.php`
- [ ] Test PHP file by visiting URL in browser (should see JSON response)
- [ ] Verify POP3 credentials in PHP file match email account
- [ ] Upload `sendESP32mail.php` (for mail sending) if not already done
- [ ] ESP32 code built and uploaded (61.3% flash usage)
- [ ] Test by sending email from external account to esperthertu_post_office@storyboardacs.com
- [ ] Enter post office in MUD and verify letter appears

## Testing

1. **Send test email**
   ```
   To: esperthertu_post_office@storyboardacs.com
   Subject: Test Message
   Body: A message from TestPlayer - The Adventurer:
         
   Here Ye, Here Ye. A message from the realm of Esperthertu has been delivered to you!
   
   This is a test message body.
   ```

2. **MUD Test**
   - Login as TestPlayer
   - Go to Post Office
   - Should see: "You see A Letter addressed to TestPlayer lying in the post office."
   - `get letter`
   - `examine letter` → Should display full email body

## Notes

- HTTP timeout: 15 seconds max (5s connection, 10s socket timeout)
- JSON parsing is basic but robust to well-formed responses
- Letter items don't require itemDefs entries (they're created dynamically)
- The system is silent to other players (mail checks are invisible)
- Dialog timing for Post Office (30-120s) prevents chat spam

## Future Enhancements

- Delete letter from inventory to trash them
- Forward letters to other players
- Archive old letters
- Maximum letter storage limit
- Letter expiration (auto-delete after X days)
- Multiple letter subjects/organization
