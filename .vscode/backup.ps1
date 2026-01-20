`$timestamp = Get-Date -Format 'HHmmss'
mkdir -Force 'c:\issues\VsCode' | Out-Null
`$backupPath = "c:\issues\VsCode\ESP32MUD_backup_`$timestamp.zip"
`$projectPath = 'c:\Users\jwein\Documents\PlatformIO\Projects\260118-110624-seeed_xiao_esp32c3'
Compress-Archive -Path `$projectPath -DestinationPath `$backupPath -Force
Write-Host "`n Backup created successfully!" -ForegroundColor Green
Write-Host "Location: `$backupPath" -ForegroundColor Cyan
