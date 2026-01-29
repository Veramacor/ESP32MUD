<?php
// File download/list handler for ESP32 MUD
// Place this in public_html/download.php
// Usage:
//   /download.php                    -> List all files as JSON
//   /download.php?file=[filename]    -> Download single file (binary)
//   /download.php?all=1              -> Download all files with contents as JSON

// Get requested filename from query parameter
$file = isset($_GET['file']) ? basename($_GET['file']) : '';
$all = isset($_GET['all']) ? (int)$_GET['all'] : 0;
$basedir = dirname(__FILE__);
$filepath = $basedir . '/mud_files/';

// LIST mode: if no file and no all specified, return list of all files as JSON
if (empty($file) && $all === 0) {
    header('Content-Type: application/json');
    $files = array();
    
    if (is_dir($filepath)) {
        $dir_files = scandir($filepath);
        foreach ($dir_files as $fname) {
            if ($fname !== '.' && $fname !== '..') {
                $full_path = $filepath . $fname;
                if (is_file($full_path)) {
                    $files[] = array(
                        'name' => $fname,
                        'size' => filesize($full_path),
                        'modified' => date('Y-m-d H:i:s', filemtime($full_path))
                    );
                }
            }
        }
    }
    echo json_encode($files, JSON_PRETTY_PRINT);
    exit;
}

// DOWNLOAD ALL FILES mode: return all files with their contents as JSON
if ($all === 1) {
    header('Content-Type: application/json');
    
    if (!is_dir($filepath)) {
        http_response_code(404);
        die(json_encode(['error' => 'mud_files directory not found']));
    }
    
    $result = array(
        'success' => true,
        'timestamp' => date('Y-m-d H:i:s'),
        'files' => array()
    );
    
    $dir_files = scandir($filepath);
    
    foreach ($dir_files as $fname) {
        if ($fname !== '.' && $fname !== '..') {
            $full_path = $filepath . $fname;
            if (is_file($full_path)) {
                // Read file content
                $content = file_get_contents($full_path);
                
                if ($content !== false) {
                    $result['files'][] = array(
                        'name' => $fname,
                        'size' => filesize($full_path),
                        'modified' => date('Y-m-d H:i:s', filemtime($full_path)),
                        'content' => base64_encode($content)  // Base64 encode binary-safe content
                    );
                }
            }
        }
    }
    
    if (count($result['files']) === 0) {
        http_response_code(404);
        die(json_encode(['error' => 'No files found in mud_files directory']));
    }
    
    echo json_encode($result, JSON_PRETTY_PRINT);
    exit;
}

// DOWNLOAD SINGLE FILE mode: verify file exists and stream it
if (!empty($file)) {
    // Security: prevent directory traversal
    if (strpos($file, '..') !== false || strpos($file, '/') !== false) {
        http_response_code(403);
        echo "Error: Invalid filename";
        exit;
    }

    $full_path = $filepath . $file;

    if (!file_exists($full_path)) {
        http_response_code(404);
        echo "Error: File not found";
        exit;
    }

    // Send binary file to client
    header('Content-Type: application/octet-stream');
    header('Cache-Control: no-cache, no-store, must-revalidate');
    
    $filesize = filesize($full_path);
    header('Content-Length: ' . $filesize);
    header('Content-Disposition: attachment; filename="' . $file . '"');

    // Stream the file
    if (readfile($full_path) === false) {
        http_response_code(500);
        echo "Error: Could not read file";
        exit;
    }
    exit;
}

// Invalid request
http_response_code(400);
echo json_encode(['error' => 'Invalid request. Use ?file=[name] for single file, ?all=1 for all files, or no params to list']);
?>


