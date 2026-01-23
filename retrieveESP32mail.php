<?php
/**
 * retrieveESP32mail.php
 * 
 * Retrieves unread emails from the Post Office POP3 mailbox
 * and marks them as read for the next sync.
 * 
 * Called by ESP32 MUD when a player enters the Post Office
 * 
 * Query parameters:
 *   player=<playername> (optional, for logging)
 * 
 * Returns JSON array of email objects:
 * [
 *   {
 *     "to": "recipient@example.com",
 *     "from": "sender@example.com",
 *     "subject": "...",
 *     "body": "...",
 *     "messageId": "unique ID for tracking"
 *   },
 *   ...
 * ]
 */

// POP3 Configuration
$pop3_host = 'mail.storyboardacs.com';
$pop3_port = 110;  // Standard POP3 (or 995 for POP3S)
$pop3_user = 'esperthertu_post_office@storyboardacs.com';
$pop3_pass = 'VXN_jhn8bfe5cve7dve';  // Same as SMTP password

// Timeout values
$connect_timeout = 5;
$socket_timeout = 10;

// Response array
$emails = [];
$errors = [];

try {
    // Log incoming request
    $player = isset($_GET['player']) ? $_GET['player'] : 'unknown';
    error_log("[POP3] Mail check request from player: $player");
    
    // Connect to POP3 server
    error_log("[POP3] Connecting to $pop3_host:$pop3_port");
    
    $socket = @fsockopen($pop3_host, $pop3_port, $errno, $errstr, $connect_timeout);
    if (!$socket) {
        throw new Exception("Connection failed: $errstr (code: $errno)");
    }
    
    // Set socket timeout
    stream_set_timeout($socket, $socket_timeout);
    
    // Read server greeting
    $response = fgets($socket);
    if (strpos($response, '+OK') === false) {
        throw new Exception("Server greeting failed: $response");
    }
    error_log("[POP3] Server greeting received");
    
    // Authenticate
    error_log("[POP3] Authenticating as $pop3_user");
    
    // Send USER command
    fputs($socket, "USER $pop3_user\r\n");
    $response = fgets($socket);
    if (strpos($response, '+OK') === false) {
        throw new Exception("USER command failed: $response");
    }
    
    // Send PASS command
    fputs($socket, "PASS $pop3_pass\r\n");
    $response = fgets($socket);
    if (strpos($response, '+OK') === false) {
        throw new Exception("PASS command failed: $response");
    }
    error_log("[POP3] Authentication successful");
    
    // Get message count
    fputs($socket, "STAT\r\n");
    $response = fgets($socket);
    if (preg_match('/\+OK (\d+) \d+/', $response, $matches)) {
        $message_count = $matches[1];
        error_log("[POP3] Found $message_count messages");
    } else {
        throw new Exception("STAT command failed: $response");
    }
    
    // Retrieve each unread message
    for ($msg_num = 1; $msg_num <= $message_count; $msg_num++) {
        error_log("[POP3] Retrieving message $msg_num");
        
        // RETR command retrieves the full message
        fputs($socket, "RETR $msg_num\r\n");
        $response = fgets($socket);
        if (strpos($response, '+OK') === false) {
            error_log("[POP3] Failed to retrieve message $msg_num");
            continue;
        }
        
        // Read message body (lines until a line with just ".")
        $message_lines = [];
        while (true) {
            $line = fgets($socket);
            if ($line === false || $line === ".\r\n" || $line === ".\n") {
                break;
            }
            // Remove the trailing \r\n
            $message_lines[] = rtrim($line, "\r\n");
        }
        
        // Parse email headers and body
        $email = parseEmail($message_lines);
        
        if ($email) {
            // Check if this is a marked-as-read email (look for custom header or flag)
            if (isEmailRead($email)) {
                error_log("[POP3] Skipping already-read message $msg_num");
                continue;
            }
            
            // FILTER: Only accept emails TO the Post Office address (replies to our outgoing mail)
            // This ensures we only get emails that are replies TO esperthertu_post_office@storyboardacs.com
            if (stripos($email['to'], 'esperthertu_post_office') === false) {
                error_log("[POP3] Email not addressed to Post Office. To: " . $email['to'] . " - leaving untouched");
                continue;
            }
            
            // FILTER: Only accept emails NOT from the post office itself
            // We only want player REPLIES, not our outgoing system emails
            if (stripos($email['from'], 'esperthertu_post_office') !== false) {
                error_log("[POP3] Skipping system email from: " . $email['from'] . " - leaving untouched");
                continue;
            }
            
            // Extract sender address (everything before the @ sign)
            // This will be used as fallback letter sender display name
            $from_email = trim($email['from']);
            // Handle format: "Name <email@domain>" or just "email@domain"
            if (preg_match('/<(.+?)@/', $from_email, $matches)) {
                $sender_part = $matches[1];
            } else if (preg_match('/(.+?)@/', $from_email, $matches)) {
                $sender_part = $matches[1];
            } else {
                // Fallback: use entire from address if parsing fails
                $sender_part = $from_email;
            }
            
            // Determine display name: prefer extracted playername from body, fallback to sender email part
            $displayName = !empty($email['recipient']) ? $email['recipient'] : $sender_part;
            
            error_log("[POP3] Processing new message $msg_num from: " . $email['from'] . " (display as: " . $displayName . ")");
            
            // Store this email with display name
            $email['displayName'] = $displayName;
            $email['messageId'] = "$msg_num-" . time();
            $emails[] = $email;
            
            // Mark as deleted ONLY if we successfully processed it
            error_log("[POP3] Marking message $msg_num as deleted");
            fputs($socket, "DELE $msg_num\r\n");
            $response = fgets($socket);
            if (strpos($response, '+OK') === false) {
                error_log("[POP3] Warning: Could not mark message $msg_num as deleted");
            }
        }
    }
    
    // Close connection (QUIT to commit deletions)
    error_log("[POP3] Closing connection");
    fputs($socket, "QUIT\r\n");
    fclose($socket);
    
    error_log("[POP3] Successfully retrieved " . count($emails) . " new messages");
    
} catch (Exception $e) {
    error_log("[POP3] Error: " . $e->getMessage());
    $errors[] = $e->getMessage();
}

// Return JSON response
header('Content-Type: application/json');
echo json_encode([
    'success' => count($errors) === 0,
    'emails' => $emails,
    'count' => count($emails),
    'errors' => $errors
]);

/**
 * Parse a full email message into structured data
 */
function parseEmail($lines) {
    $email = [
        'to' => '',
        'from' => '',
        'subject' => '',
        'body' => '',
        'recipient' => ''
    ];
    
    $in_headers = true;
    $body_lines = [];
    $mime_boundary = null;
    $text_content = '';
    $in_text_part = false;
    
    foreach ($lines as $line) {
        if ($in_headers) {
            // Empty line marks end of headers
            if (trim($line) === '') {
                $in_headers = false;
                continue;
            }
            
            // Parse header lines
            if (preg_match('/^To:\s*(.+)$/i', $line, $m)) {
                $email['to'] = trim($m[1]);
            } elseif (preg_match('/^From:\s*(.+)$/i', $line, $m)) {
                $email['from'] = trim($m[1]);
            } elseif (preg_match('/^Subject:\s*(.+)$/i', $line, $m)) {
                $email['subject'] = trim($m[1]);
            } elseif (preg_match('/boundary="([^"]+)"/i', $line, $m)) {
                $mime_boundary = $m[1];
            }
        } else {
            // Processing body
            $trimmed = trim($line);
            
            // Handle MIME boundaries
            if ($mime_boundary && strpos($trimmed, '--' . $mime_boundary) === 0) {
                // Check if this starts a text/plain section
                $in_text_part = false;
                continue;
            }
            
            // Look for start of text/plain content type
            if (preg_match('/Content-Type:\s*text\/plain/i', $line)) {
                $in_text_part = true;
                continue;
            }
            
            // Skip MIME header lines after Content-Type
            if ($in_text_part && preg_match('/^[A-Z\-]+:\s*(.*)$/i', $line)) {
                continue;
            }
            
            // Skip empty line after MIME headers
            if ($in_text_part && $trimmed === '') {
                continue;
            }
            
            // Collect actual message content
            if (!$mime_boundary) {
                // Simple email, no MIME
                $body_lines[] = $line;
            } elseif ($in_text_part && $trimmed !== '') {
                $body_lines[] = $line;
            }
        }
    }
    
    $raw_body = trim(implode("\r\n", $body_lines));
    
    // Decode quoted-printable if needed
    if (preg_match('/=\r?\n/', $raw_body) || preg_match('/=[\dA-F]{2}/', $raw_body)) {
        $raw_body = quoted_printable_decode($raw_body);
    }
    
    // Extract only the sender's reply (before quoted text begins)
    // Look for "On [date]..." which indicates the start of quoted content
    $reply_text = $raw_body;
    $quote_markers = [
        '/On\s+\w+,\s+\w+\s+\d+,\s+\d{4}/',  // "On Fri, Jan 23, 2026"
        '/^>/m',                                // Lines starting with >
        '/^--\s*$/m',                           // Signature separator
        '/^Best regards,/i',                    // Signature start
        '/^Applied Computer Services/i',       // Company name (signature)
    ];
    
    foreach ($quote_markers as $marker) {
        if (preg_match($marker, $reply_text)) {
            // Find position of quote marker
            if (preg_match($marker, $reply_text, $matches, PREG_OFFSET_CAPTURE)) {
                $pos = $matches[0][1];
                $reply_text = substr($reply_text, 0, $pos);
            }
            break;
        }
    }
    
    // Clean up trailing whitespace and newlines
    $reply_text = trim($reply_text);
    
    // Format the body: "A letter from [email]. It reads, [blank line] [reply]"
    $from_email = trim($email['from']);
    $email['body'] = "A letter from " . $from_email . ".\n\nIt reads,\n\n" . $reply_text;
    
    // Extract recipient from body
    if (preg_match('/a message from\s+(\w+)/i', $email['body'], $m)) {
        $email['recipient'] = trim($m[1]);
        error_log("[POP3] Extracted recipient: " . $email['recipient']);
    }
    
    // Validate email has required fields
    if (empty($email['from']) || empty($email['body'])) {
        error_log("[POP3] Email missing from or body. From: " . $email['from'] . ", Body length: " . strlen($email['body']));
        return null;
    }
    
    return $email;
}

/**
 * Check if an email has already been read
 * (For now, we'll assume all retrieved emails are unread)
 */
function isEmailRead($email) {
    // Could check for "X-Read: true" header or similar
    return false;
}
?>
