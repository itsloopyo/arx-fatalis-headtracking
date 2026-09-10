#!/usr/bin/env pwsh
#Requires -Version 5.1
<#
.SYNOPSIS
    Configure, build and run the native unit tests.
.DESCRIPTION
    The test targets are behind ARX_BUILD_TESTS, which is OFF in the build
    tree `pixi run build` uses, so they get their own tree in build-tests/ and
    the shipped .asi is never linked against a test build.

    Non-interactive: exits 0 when every test passes, non-zero with ctest's own
    output on the first failure.
.NOTES
    Run via: pixi run test
#>
param(
    [ValidateSet('Debug', 'Release')]
    [string]$Configuration = 'Debug'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$projectRoot = Split-Path -Parent $PSScriptRoot
$buildDir = Join-Path $projectRoot 'build-tests'

& cmake -S $projectRoot -B $buildDir -A Win32 -DARX_BUILD_TESTS=ON
if ($LASTEXITCODE -ne 0) { Write-Host 'ERROR: cmake configure failed' -ForegroundColor Red; exit 1 }

& cmake --build $buildDir --config $Configuration
if ($LASTEXITCODE -ne 0) { Write-Host 'ERROR: test build failed' -ForegroundColor Red; exit 1 }

& ctest --test-dir $buildDir --build-config $Configuration --output-on-failure
if ($LASTEXITCODE -ne 0) { Write-Host 'ERROR: tests failed' -ForegroundColor Red; exit 1 }

Write-Host 'All tests passed.' -ForegroundColor Green
