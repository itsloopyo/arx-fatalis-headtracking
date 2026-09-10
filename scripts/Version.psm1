#Requires -Version 5.1
<#
.SYNOPSIS
    The mod version: where it is read from, where it is written to, and what
    has to agree with it.
.DESCRIPTION
    CMakeLists.txt's project(... VERSION ...) is canonical. Two other files
    carry the same number and are derived from it:

      CMakeLists.txt      project(VERSION)  - what the release tag is checked against
      scripts/install.cmd MOD_VERSION       - what the installer writes to the state file
      pixi.toml           workspace.version - what the workspace reports

    .github/workflows/release.yml passes `version-source: cmake` and
    `version-path: CMakeLists.txt` to cameraunlock-core's release-mod.yml, which
    reads the tag's version out of the same declaration. Moving canonical
    anywhere else silently splits the local release path from the CI one.

    Before this module each of release.ps1, package-release.ps1 and
    release-nightly.ps1 carried its own copy of the regexes, so the set of files
    that carry the version was written down three times and read back
    differently in each. It is written down once, here.
#>

Set-StrictMode -Version Latest

# Matches cameraunlock-core's Get-ProjectVersion -Source cmake, which is what CI
# reads. Duplicated rather than delegated so this module stands alone: two of
# its three callers import it before (or without) ReleaseWorkflow.psm1.
$script:CMakeVersionPattern = '(?is)\bproject\s*\(\s*ArxFatalisHeadTracking\s+VERSION\s+(\d+\.\d+\.\d+)'

<#
.SYNOPSIS
    The files that carry the mod version, so no caller has to name them again.
#>
function Get-ModVersionPaths {
    param([Parameter(Mandatory = $true)][string]$ProjectRoot)

    return [pscustomobject]@{
        CMakeLists = Join-Path $ProjectRoot 'CMakeLists.txt'
        InstallCmd = Join-Path $ProjectRoot 'scripts\install.cmd'
        PixiToml   = Join-Path $ProjectRoot 'pixi.toml'
    }
}

<#
.SYNOPSIS
    Reads X.Y.Z from CMakeLists.txt.
#>
function Get-ModVersion {
    param([Parameter(Mandatory = $true)][string]$ProjectRoot)

    $path = (Get-ModVersionPaths -ProjectRoot $ProjectRoot).CMakeLists
    if (-not (Test-Path -LiteralPath $path)) { throw "CMakeLists.txt not found: $path" }

    $version = [regex]::Match((Get-Content -LiteralPath $path -Raw), $script:CMakeVersionPattern).Groups[1].Value
    if (-not $version) {
        throw "Could not parse project(ArxFatalisHeadTracking VERSION X.Y.Z) from $path"
    }
    return $version
}

<#
.SYNOPSIS
    Writes a new version into CMakeLists.txt and the two files derived from it.
#>
function Set-ModVersion {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$Version
    )

    # Checked here rather than trusted from the caller: this function rewrites
    # three tracked files, and a version that is not X.Y.Z writes a number the
    # release tag will not match.
    if ($Version -notmatch '^\d+\.\d+\.\d+$') {
        throw "Set-ModVersion expects a X.Y.Z version, got '$Version'."
    }

    $paths = Get-ModVersionPaths -ProjectRoot $ProjectRoot

    $cmake = Get-Content -LiteralPath $paths.CMakeLists -Raw
    $cmake = $cmake -replace '(?i)(project\(ArxFatalisHeadTracking\s+VERSION\s+)\d+\.\d+\.\d+', "`${1}$Version"
    Set-Content -LiteralPath $paths.CMakeLists -Value $cmake -NoNewline

    $install = Get-Content -LiteralPath $paths.InstallCmd -Raw
    $install = $install -replace 'set "MOD_VERSION=[^"]*"', "set `"MOD_VERSION=$Version`""
    Set-Content -LiteralPath $paths.InstallCmd -Value $install -NoNewline

    # Anchored to the workspace table's own key so a dependency's version, or a
    # version written inside a task string, is never the thing rewritten.
    $pixi = Get-Content -LiteralPath $paths.PixiToml -Raw
    $pixi = $pixi -replace '(?m)^version\s*=\s*"[^"]*"', "version = `"$Version`""
    Set-Content -LiteralPath $paths.PixiToml -Value $pixi -NoNewline
}

<#
.SYNOPSIS
    Throws unless every derived copy of the version matches CMakeLists.txt.
.DESCRIPTION
    This is what makes "derived from CMakeLists.txt" true rather than intended.
    Set-ModVersion writes all three in one pass, so a mismatch means one of them
    was hand-edited; shipping it would tag a release against one number and
    install a different one.
#>
function Assert-ModVersionsInSync {
    param(
        [Parameter(Mandatory = $true)][string]$ProjectRoot,
        [Parameter(Mandatory = $true)][string]$Version
    )

    $paths = Get-ModVersionPaths -ProjectRoot $ProjectRoot

    $installCmdVersion = [regex]::Match(
        (Get-Content -LiteralPath $paths.InstallCmd -Raw),
        'set "MOD_VERSION=([^"]*)"').Groups[1].Value
    if ($installCmdVersion -ne $Version) {
        throw "install.cmd records MOD_VERSION=$installCmdVersion but CMakeLists.txt says $Version. The installer would write a state file the launcher then reads as the wrong version."
    }

    $pixiVersion = [regex]::Match(
        (Get-Content -LiteralPath $paths.PixiToml -Raw),
        '(?m)^version\s*=\s*"([^"]*)"').Groups[1].Value
    if ($pixiVersion -ne $Version) {
        throw "pixi.toml declares version $pixiVersion but CMakeLists.txt says $Version."
    }
}

Export-ModuleMember -Function @(
    'Get-ModVersionPaths',
    'Get-ModVersion',
    'Set-ModVersion',
    'Assert-ModVersionsInSync'
)
