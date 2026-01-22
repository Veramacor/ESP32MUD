# =====================================================
# ESP32 MUD - Batch File Upload Script
# =====================================================
# Usage: .\upload-files.ps1 -ip 192.168.1.100
# Or edit $DeviceIP below and run: .\upload-files.ps1

param(
    [string]$ip = $null
)

# Configuration
$DeviceIP = if ($ip) { $ip } else { "192.168.1.100" }  # Change this to your device IP
$Port = 8080
$UploadUrl = "http://$DeviceIP`:$Port/upload"
$DataFolder = ".\data"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "   ESP32 MUD - Batch File Uploader" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""
Write-Host "Target Device: $DeviceIP`:$Port" -ForegroundColor Green
Write-Host "Data Folder: $DataFolder" -ForegroundColor Green
Write-Host ""

# Check if data folder exists
if (-not (Test-Path $DataFolder)) {
    Write-Host "ERROR: Data folder not found at $DataFolder" -ForegroundColor Red
    exit 1
}

# Get all files to upload
$files = Get-ChildItem -Path $DataFolder -File
$fileCount = $files.Count

if ($fileCount -eq 0) {
    Write-Host "No files found to upload!" -ForegroundColor Yellow
    exit 1
}

Write-Host "Found $fileCount file(s) to upload:" -ForegroundColor Yellow
$files | ForEach-Object { Write-Host "  • $_" }
Write-Host ""

# Confirm before uploading
$confirm = Read-Host "Upload all files? (Y/n)"
if ($confirm -ne "" -and $confirm -ne "Y" -and $confirm -ne "y") {
    Write-Host "Cancelled." -ForegroundColor Yellow
    exit 0
}

Write-Host ""
Write-Host "Uploading files..." -ForegroundColor Cyan
Write-Host ""

$successCount = 0
$failCount = 0

foreach ($file in $files) {
    $filePath = $file.FullName
    $fileName = $file.Name
    
    try {
        Write-Host "Uploading: $fileName ... " -NoNewline -ForegroundColor White
        
        # Create multipart form data
        $form = @{
            file = Get-Item -Path $filePath
        }
        
        # Upload file
        $response = Invoke-WebRequest -Uri $UploadUrl `
            -Method Post `
            -Form $form `
            -TimeoutSec 10 `
            -ErrorAction Stop
        
        Write-Host "✓ OK" -ForegroundColor Green
        $successCount++
    }
    catch {
        Write-Host "✗ FAILED" -ForegroundColor Red
        Write-Host "  Error: $($_.Exception.Message)" -ForegroundColor Red
        $failCount++
    }
    
    Start-Sleep -Milliseconds 500  # Slight delay between uploads
}

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "Upload Summary:" -ForegroundColor Cyan
Write-Host "  Success: $successCount file(s)" -ForegroundColor Green
Write-Host "  Failed:  $failCount file(s)" -ForegroundColor $(if ($failCount -gt 0) { "Red" } else { "Green" })
Write-Host "========================================" -ForegroundColor Cyan

if ($failCount -eq 0) {
    Write-Host ""
    Write-Host "All files uploaded successfully!" -ForegroundColor Green
    Write-Host "Check them on device with: DEBUG FILES" -ForegroundColor Yellow
}
