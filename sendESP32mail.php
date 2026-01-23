<?php
header('Content-Type: application/json');

// Get the JSON payload from the request body
$input = file_get_contents('php://input');
$data = json_decode($input, true);

// Log for debugging
error_log("Email API called. Payload: " . $input);

// Validate required fields
if (!isset($data['to']) || !isset($data['subject']) || !isset($data['body'])) {
    http_response_code(400);
    echo json_encode(['success' => false, 'error' => 'Missing required fields: to, subject, body']);
    error_log("Email API missing required fields. Data: " . print_r($data, true));
    exit;
}

$to = $data['to'];
$subject = $data['subject'];
$body = $data['body'];
$from = isset($data['from']) ? $data['from'] : 'esperthertu_post_office@storyboardacs.com';

// Validate email address
if (!filter_var($to, FILTER_VALIDATE_EMAIL)) {
    http_response_code(400);
    echo json_encode(['success' => false, 'error' => 'Invalid recipient email address']);
    exit;
}

// Set headers for proper email formatting
$headers = "MIME-Version: 1.0\r\n";
$headers .= "Content-type: text/plain; charset=UTF-8\r\n";
$headers .= "From: " . $from . "\r\n";

// Send the email using PHP's mail() function
$result = mail($to, $subject, $body, $headers);

if ($result) {
    http_response_code(200);
    echo json_encode(['success' => true, 'message' => 'Email sent successfully']);
    error_log("Email sent successfully to: " . $to);
} else {
    http_response_code(500);
    echo json_encode(['success' => false, 'error' => 'Failed to send email - mail() function returned false']);
    error_log("Email failed to send to: " . $to);
}
?>
