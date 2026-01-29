<?php
// File upload handler for ESP32 MUD Mapper
// Place this in public_html/upload.php
// Usage: POST to /upload.php?file=[filename] with file content in request body

// Security: Ensure mud_files directory exists
$basedir = dirname(__FILE__);
$filepath = $basedir . '/mud_files/';

// Create directory if it doesn't exist
if (!is_dir($filepath)) {
    if (!mkdir($filepath, 0755, true)) {
        http_response_code(500);
        die(json_encode(['error' => 'Failed to create mud_files directory']));
    }
}

// Get requested filename from query parameter
$file = isset($_GET['file']) ? basename($_GET['file']) : '';

// Validate filename
if (empty($file)) {
    http_response_code(400);
    die(json_encode(['error' => 'Missing file parameter']));
}

// Security: prevent directory traversal
if (strpos($file, '..') !== false || strpos($file, '/') !== false || strpos($file, '\\') !== false) {
    http_response_code(403);
    die(json_encode(['error' => 'Invalid filename']));
}

// Only allow POST requests
if ($_SERVER['REQUEST_METHOD'] !== 'POST') {
    http_response_code(405);
    die(json_encode(['error' => 'Only POST requests allowed']));
}

// Get the raw file content from request body
$input = file_get_contents('php://input');

if ($input === false || strlen($input) === 0) {
    http_response_code(400);
    die(json_encode(['error' => 'No file content provided']));
}

$full_path = $filepath . $file;

// Write file (will overwrite existing file)
if (file_put_contents($full_path, $input, LOCK_EX) === false) {
    http_response_code(500);
    die(json_encode(['error' => 'Failed to write file']));
}

// Success response
http_response_code(200);
echo json_encode([
    'success' => true,
    'message' => "File '$file' uploaded successfully",
    'file' => $file,
    'size' => filesize($full_path),
    'timestamp' => date('Y-m-d H:i:s', filemtime($full_path))
]);
?>
