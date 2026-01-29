<?php
$filename = $_GET['file'];
$filepath = "/public_html/mud_files/" . basename($filename);

if (file_exists($filepath)) {
    header('Content-Type: application/octet-stream');
    header('Content-Length: ' . filesize($filepath));
    readfile($filepath);
} else {
    http_response_code(404);
}
?>