# ESP32MUD - Push to GitHub Script
# Usage: .\push-to-github.ps1 "Your commit message"
# Or: .\push-to-github.ps1 (uses default timestamped message)

param(
    [string]$CommitMessage = ""
)

# Color output
function Write-Success { Write-Host $args -ForegroundColor Green }
function Write-Error { Write-Host $args -ForegroundColor Red }
function Write-Info { Write-Host $args -ForegroundColor Cyan }

# Get repository root
$repoRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $repoRoot

Write-Info "`n[GIT] ESP32MUD GitHub Push Script"
Write-Info "[GIT] Repository: $repoRoot`n"

# Check if git is available
if (-not (Get-Command git -ErrorAction SilentlyContinue)) {
    Write-Error "[ERROR] Git is not installed or not in PATH"
    exit 1
}

# Check if we're in a git repository
if (-not (Test-Path .git)) {
    Write-Error "[ERROR] Not a git repository. Initialize with: git init"
    exit 1
}

# Check for changes
$status = git status --porcelain
if (-not $status) {
    Write-Info "[GIT] No changes to commit"
    exit 0
}

# Generate commit message if not provided
if ([string]::IsNullOrWhiteSpace($CommitMessage)) {
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    $CommitMessage = "v1.1.0: Update $(Get-Date -Format 'MMdd-HHmm')"
}

Write-Info "`n[GIT] Changes detected:"
Write-Info $status

Write-Info "`n[GIT] Staging all changes..."
git add .
if ($LASTEXITCODE -ne 0) {
    Write-Error "[ERROR] Failed to stage changes"
    exit 1
}
Write-Success "[GIT] Changes staged"

Write-Info "`n[GIT] Committing with message: '$CommitMessage'"
git commit -m "$CommitMessage"
if ($LASTEXITCODE -ne 0) {
    Write-Error "[ERROR] Failed to commit"
    exit 1
}
Write-Success "[GIT] Commit successful"

Write-Info "`n[GIT] Pushing to GitHub..."
git push -u origin main --force
if ($LASTEXITCODE -ne 0) {
    Write-Error "[ERROR] Failed to push to GitHub"
    Write-Error "[ERROR] Check your remote: git remote -v"
    exit 1
}
Write-Success "[GIT] Push successful!"

Write-Success "`n[GIT] Upload complete: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss')`n"
