# ESP32MUD - Git Setup Script
# Run this once to configure git for your repository

Write-Host "`n=== ESP32MUD Git Configuration ===" -ForegroundColor Cyan

# Get repository root
$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $repoRoot

Write-Host "`nRepository: $repoRoot" -ForegroundColor Cyan

# Check if git is installed
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Host "ERROR: Git is not installed" -ForegroundColor Red
    Write-Host "Install from: https://git-scm.com/download/win" -ForegroundColor Yellow
    exit 1
}

Write-Host "Git version: $(git --version)" -ForegroundColor Green

# Initialize git if needed
if (-not (Test-Path .git)) {
    Write-Host "`nInitializing git repository..." -ForegroundColor Cyan
    git init
    Write-Host "Git initialized" -ForegroundColor Green
}

# Check for existing remote
$remoteUrl = git config --get remote.origin.url
if ($remoteUrl) {
    Write-Host "`nExisting remote found: $remoteUrl" -ForegroundColor Green
    $changeRemote = Read-Host "Change remote? (y/n)"
    if ($changeRemote -eq 'y') {
        $newUrl = Read-Host "Enter GitHub repository URL (https://github.com/username/ESP32MUD.git)"
        git remote remove origin
        git remote add origin $newUrl
        Write-Host "Remote updated" -ForegroundColor Green
    }
} else {
    Write-Host "`nNo remote found. Setting up GitHub connection..." -ForegroundColor Cyan
    $githubUrl = Read-Host "Enter your GitHub repository URL (https://github.com/username/ESP32MUD.git)"
    git remote add origin $githubUrl
    Write-Host "Remote configured" -ForegroundColor Green
}

# Verify remote
Write-Host "`nVerifying remote:" -ForegroundColor Cyan
git remote -v

# Create .gitignore if it doesn't exist
if (-not (Test-Path .gitignore)) {
    Write-Host "`nCreating .gitignore..." -ForegroundColor Cyan
    @"
# PlatformIO
.pio
.platformio
.pioenvs
.piolibdeps
*.o
*.a

# Build artifacts
build/
dist/
*.bin
*.elf
*.map

# IDE
.vscode
.vscode-workspace
*.code-workspace

# OS
.DS_Store
Thumbs.db
*.swp
*.swo
*~

# Temporary files
temp/
tmp/
*.tmp
*.bak

# Python
__pycache__/
*.py[cod]
*$py.class
.Python
env/
venv/

# Logs
*.log

# Exclude large personal files (optional)
# /data/player_*.txt
# /data/session_log.txt

"@ | Out-File -Encoding UTF8 .gitignore
    Write-Host ".gitignore created" -ForegroundColor Green
}

# Show git config summary
Write-Host "`nGit Configuration Summary:" -ForegroundColor Cyan
Write-Host "Remote URL:" -ForegroundColor Yellow
git remote -v

Write-Host "`n✓ Git setup complete!" -ForegroundColor Green
Write-Host "`nNext steps:" -ForegroundColor Cyan
Write-Host "1. Run: .\push-to-github.ps1 ""First commit message""" -ForegroundColor Gray
Write-Host "2. Or just: .\push-to-github.ps1" -ForegroundColor Gray
Write-Host "`n"
