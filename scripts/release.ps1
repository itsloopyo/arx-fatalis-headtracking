#!/usr/bin/env pwsh
#Requires -Version 5.1
<#
.SYNOPSIS
    Cut a release of Arx Fatalis Head Tracking.
.DESCRIPTION
    Validates preconditions, writes the new version into CMakeLists.txt and the
    two files derived from it, generates CHANGELOG.md from the commits since the
    last tag, builds, commits, tags and pushes. The pushed tag triggers
    .github/workflows/release.yml.

    Runs unattended end to end. Typing the command is the authorization: there
    is no confirmation prompt and no -Yes switch. Every safety property is a
    deterministic precondition that exits non-zero with a one-line diagnostic -
    on main, clean tree, tag absent, semver valid, notices in sync.

    Never destructive: no force push, no amend, no tag overwrite.
.NOTES
    Run via: pixi run release <major|minor|patch|nightly|X.Y.Z>
#>
param(
    [Parameter(Position = 0)]
    [string]$Version = '',

    # Ship a release even when every commit since the last tag was filtered as
    # noise (writes a maintenance changelog entry instead of aborting).
    [switch]$Force
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot   = Split-Path -Parent $PSScriptRoot
$changelogPath = Join-Path $projectRoot 'CHANGELOG.md'

# Rolling dev pre-release takes a different path entirely.
if ($Version -eq 'nightly') {
    & (Join-Path $PSScriptRoot 'release-nightly.ps1')
    exit $LASTEXITCODE
}

Import-Module (Join-Path $projectRoot 'cameraunlock-core\powershell\ReleaseWorkflow.psm1') -Force
Import-Module (Join-Path $PSScriptRoot 'Version.psm1') -Force

# Which files carry the version is Version.psm1's to know; the commit below
# stages exactly what Set-ModVersion writes.
$versionFiles = Get-ModVersionPaths -ProjectRoot $projectRoot

# Mirrors New-ChangelogFromCommits' insertion so a -Force maintenance entry
# lands in the same place with the same shape.
function Add-MaintenanceChangelogEntry {
    param(
        [Parameter(Mandatory = $true)][string]$Path,
        [Parameter(Mandatory = $true)][string]$NewVersion
    )
    $date = Get-Date -Format 'yyyy-MM-dd'
    $entry = "## [$NewVersion] - $date`n`n### Changed`n`n- Maintenance release (no user-facing changes).`n`n"
    $changelog = Get-Content -LiteralPath $Path -Raw
    $changelog = $changelog -replace '(?s)(# Changelog.*?\n\n)', "`$1$entry"
    Set-Content -LiteralPath $Path -Value ($changelog.TrimEnd() + "`n") -NoNewline
}

Write-Host '=== Arx Fatalis Head Tracking Release ===' -ForegroundColor Cyan
Write-Host ''

$currentVersion = Get-ModVersion -ProjectRoot $projectRoot

if ([string]::IsNullOrWhiteSpace($Version)) {
    Write-Host "Current version: $currentVersion" -ForegroundColor White
    Write-Host 'Usage: pixi run release <major|minor|patch|nightly|X.Y.Z>' -ForegroundColor Yellow
    exit 0
}

try {
    $Version = Resolve-ReleaseVersion -Argument $Version -CurrentVersion $currentVersion
} catch {
    Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
    exit 1
}

if ($Version -notmatch '^\d+\.\d+\.\d+$') {
    Write-Host "Error: '$Version' is not a X.Y.Z release version" -ForegroundColor Red
    exit 1
}

$tagName = "v$Version"

$currentBranch = git -C $projectRoot rev-parse --abbrev-ref HEAD
if ($currentBranch -ne 'main') {
    Write-Host "Error: releases are cut from 'main' (currently on '$currentBranch')" -ForegroundColor Red
    exit 1
}

# EVERYTHING from here down runs with the working directory inside this repo, and the
# scope is the point. Several of the calls below shell out to git without -C -
# Test-CleanGitStatus, Test-GitTagExists, New-ChangelogFromCommits and, worst of all,
# New-ReleaseTag, which runs `git tag` and two `git push`es - and `pixi run build-release`
# resolves its workspace by walking up from the working directory. Invoked by path from
# inside another checkout, a narrower guard is worse than none: the gates would correctly
# pass against THIS repo, and the tag would then be created and pushed on the OTHER one.
Push-Location $projectRoot
try {

if (-not (Test-CleanGitStatus)) {
    Write-Host 'Error: working tree has uncommitted changes' -ForegroundColor Red
    exit 1
}

if (Test-GitTagExists -Tag $tagName) {
    Write-Host "Error: tag '$tagName' already exists" -ForegroundColor Red
    exit 1
}

# With the other preconditions, ahead of every write. build.yml runs these on each push, but
# a release is cut from a local tree and the pushed tag is what publishes, so without this
# the one path that reaches users is the one path nothing tested. Placed here rather than
# beside the build because everything below mutates the repo: a gate that fires after the
# CHANGELOG has been rewritten and the version bumped leaves four modified files behind, and
# the re-run then aborts on the clean-tree check above.
Write-Host 'Running unit tests...' -ForegroundColor Cyan
& pixi run test
if ($LASTEXITCODE -ne 0) {
    Write-Host 'Error: unit tests failed' -ForegroundColor Red
    exit 1
}

# THIRD-PARTY-NOTICES.md names the cameraunlock-core commit compiled into the
# release ZIPs, and bumping the submodule does not touch it. Packaging refuses
# to ship that mismatch, so re-sync it here and let this release carry the
# correction rather than failing in CI after the tag is already pushed.
#
# Below the three preconditions, not above them: this is the first thing in the
# script that writes to the repository, and running it first meant a release
# stopped by the on-main, clean-tree or tag-exists gate had already left a
# commit behind on the user's branch. Test-CleanGitStatus has passed by here, so
# any diff this leaves is one sync-core-notices.ps1 just made.
& (Join-Path $projectRoot 'cameraunlock-core\scripts\sync-core-notices.ps1') -Repo $projectRoot
if ($LASTEXITCODE -ne 0) { throw "sync-core-notices.ps1 exited $LASTEXITCODE - fix THIRD-PARTY-NOTICES.md before releasing." }
& git -C $projectRoot diff --quiet -- THIRD-PARTY-NOTICES.md
if ($LASTEXITCODE -ne 0) {
    & git -C $projectRoot commit -q -m 'chore: record the cameraunlock-core commit this build compiles' -- THIRD-PARTY-NOTICES.md
    if ($LASTEXITCODE -ne 0) { throw "Could not commit the re-synced THIRD-PARTY-NOTICES.md." }
    Write-Host 'THIRD-PARTY-NOTICES.md re-synced to the pinned cameraunlock-core commit.' -ForegroundColor Yellow
}

Write-Host "Current version: $currentVersion" -ForegroundColor Gray
Write-Host "New version:     $Version" -ForegroundColor Green
Write-Host ''

# CHANGELOG first. This is the gate that aborts when every commit since the last
# tag was noise, so running it before any file is mutated leaves a clean tree on
# abort instead of a half-applied version bump with no tag.
Write-Host 'Generating CHANGELOG from commits...' -ForegroundColor Cyan
if (-not (git -C $projectRoot tag -l 'v*')) {
    # No tag yet, but the repo can still carry a hand-written CHANGELOG - this one does, with
    # the whole 0.0.0 feature list in it. Overwriting would delete that and ship a four-word
    # stub inside the release ZIP; prepending a second "First release." above it would ship
    # an empty entry sitting on top of the real one. So RETITLE the untagged section when it
    # is there, and only write a fresh entry when it is not.
    #
    # -Encoding UTF8 on both sides: CHANGELOG.md is UTF-8 and Windows PowerShell 5.1 reads
    # and writes with the ANSI codepage unless told otherwise.
    $date = Get-Date -Format 'yyyy-MM-dd'
    $existing = if (Test-Path -LiteralPath $changelogPath) {
        Get-Content -LiteralPath $changelogPath -Raw -Encoding UTF8
    } else { '' }

    if ($existing -match "\[$([regex]::Escape($Version))\]") {
        Write-Host "CHANGELOG already has an entry for $Version; leaving it alone." -ForegroundColor Yellow
    } elseif ($existing -match '(?m)^##\s*\[\d+\.\d+\.\d+\]') {
        # The first heading is the untagged section this release is publishing. Give it this
        # version and today's date rather than burying it under a stub.
        $updated = [regex]::Replace($existing, '(?m)^##\s*\[\d+\.\d+\.\d+\].*$',
                                    "## [$Version] - $date", 1)
        Set-Content -LiteralPath $changelogPath -Value $updated -NoNewline -Encoding UTF8
    } else {
        $entry = "## [$Version] - $date`n`nFirst release.`n"
        $body = $existing -replace '(?s)^#\s*Changelog\s*\r?\n\r?\n', ''
        $head = if ($body) { "# Changelog`n`n$entry`n$body" } else { "# Changelog`n`n$entry" }
        Set-Content -LiteralPath $changelogPath -Value $head -NoNewline -Encoding UTF8
    }
} else {
    try {
        New-ChangelogFromCommits `
            -ChangelogPath $changelogPath `
            -Version $Version `
            -ArtifactPaths @('src/', 'CMakeLists.txt', 'cameraunlock-core/', 'scripts/install.cmd', 'scripts/uninstall.cmd')
    } catch {
        if (-not $Force) {
            Write-Host "Error: $($_.Exception.Message)" -ForegroundColor Red
            Write-Host 'No user-facing changes to release. Re-run with -Force for a maintenance release.' -ForegroundColor Yellow
            exit 1
        }
        Write-Host 'No user-facing commits since last tag - writing maintenance entry (-Force).' -ForegroundColor Yellow
        Add-MaintenanceChangelogEntry -Path $changelogPath -NewVersion $Version
    }
}

Write-Host "Updating version to $Version..." -ForegroundColor Cyan
Set-ModVersion -ProjectRoot $projectRoot -Version $Version

Write-Host 'Building Release configuration...' -ForegroundColor Cyan
& pixi run build-release
if ($LASTEXITCODE -ne 0) {
    Write-Host 'Error: build-release failed' -ForegroundColor Red
    exit 1
}

Write-Host "Committing Release v$Version..." -ForegroundColor Cyan
git -C $projectRoot add -- $versionFiles.CMakeLists $versionFiles.InstallCmd $versionFiles.PixiToml $changelogPath
if ($LASTEXITCODE -ne 0) { throw 'git add failed' }
git -C $projectRoot commit -m "Release v$Version"
if ($LASTEXITCODE -ne 0) { throw 'git commit failed' }

New-ReleaseTag -Version $Version -Message "Arx Fatalis Head Tracking v$Version" -Branch 'main'

Write-Host ''
Write-Host "Release $tagName pushed. CI:" -ForegroundColor Green
Write-Host '  https://github.com/itsloopyo/arx-fatalis-headtracking/actions' -ForegroundColor Cyan

} finally {
    Pop-Location
}
