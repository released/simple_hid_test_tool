param(
    [string]$ProjectDir = (Split-Path -Parent $PSScriptRoot),
    [string]$RcPath = "",
    [string]$HeaderPath = "",
    [int]$MaxRevision = 999,
    [int]$MaxPatch = 9,
    [int]$MaxMinor = 9,
    [int]$MaxMajor = 9
)

$ErrorActionPreference = "Stop"

function Fail([string]$Message) {
    Write-Error $Message
    exit 1
}

$ProjectDir = $ProjectDir.Trim().Trim('"')
$ProjectDir = $ProjectDir.TrimEnd('\', '/')

if ([string]::IsNullOrWhiteSpace($RcPath)) {
    $RcPath = Join-Path $ProjectDir "src\\TCPTestTool.rc"
}
if ([string]::IsNullOrWhiteSpace($HeaderPath)) {
    $HeaderPath = Join-Path $ProjectDir "src\\build_info.h"
}

if (-not (Test-Path -LiteralPath $RcPath)) {
    Fail "RC file not found: $RcPath"
}

$raw = [System.IO.File]::ReadAllText($RcPath)
$m = [System.Text.RegularExpressions.Regex]::Match(
    $raw,
    '(?m)^\s*FILEVERSION\s+(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*$')

if (-not $m.Success) {
    Fail "FILEVERSION line not found in RC."
}

$major = [int]$m.Groups[1].Value
$minor = [int]$m.Groups[2].Value
$patch = [int]$m.Groups[3].Value
$rev = [int]$m.Groups[4].Value
$timestamp = (Get-Date).ToString("yyyy-MM-dd HH:mm:ss")
$versionText = ""
$numeric = ""
$updated = ""
$headerText = ""

$rev = $rev + 1
if ($rev -gt $MaxRevision) {
    $rev = 1
    $patch = $patch + 1
}
if ($patch -gt $MaxPatch) {
    $patch = 0
    $minor = $minor + 1
}
if ($minor -gt $MaxMinor) {
    $minor = 0
    $major = $major + 1
}
if ($major -gt $MaxMajor) {
    Fail "Major overflow: $major"
}

$numeric = "$major,$minor,$patch,$rev"
$versionText = "{0}.{1}.{2}.{3:D3}" -f $major, $minor, $patch, $rev

$updated = $raw
$updated = [System.Text.RegularExpressions.Regex]::Replace(
    $updated,
    '(?m)^\s*FILEVERSION\s+\d+\s*,\s*\d+\s*,\s*\d+\s*,\s*\d+\s*$',
    " FILEVERSION $numeric",
    1)
$updated = [System.Text.RegularExpressions.Regex]::Replace(
    $updated,
    '(?m)^\s*PRODUCTVERSION\s+\d+\s*,\s*\d+\s*,\s*\d+\s*,\s*\d+\s*$',
    " PRODUCTVERSION $numeric",
    1)
$updated = [System.Text.RegularExpressions.Regex]::Replace(
    $updated,
    '(?m)^\s*VALUE\s+"FileVersion",\s*"\d+\.\d+\.\d+\.\d{3}\\0"\s*$',
    "            VALUE `"FileVersion`", `"$versionText\0`"",
    1)
$updated = [System.Text.RegularExpressions.Regex]::Replace(
    $updated,
    '(?m)^\s*VALUE\s+"ProductVersion",\s*"\d+\.\d+\.\d+\.\d{3}\\0"\s*$',
    "            VALUE `"ProductVersion`", `"$versionText\0`"",
    1)

[System.IO.File]::WriteAllText($RcPath, $updated, [System.Text.Encoding]::ASCII)

$headerText = @"
#pragma once

#define TCPTESTTOOL_BUILD_VERSION_MAJOR $major
#define TCPTESTTOOL_BUILD_VERSION_MINOR $minor
#define TCPTESTTOOL_BUILD_PATCH $patch
#define TCPTESTTOOL_BUILD_NUMBER $rev
#define TCPTESTTOOL_BUILD_VERSION_TEXT_W L"$versionText"
#define TCPTESTTOOL_BUILD_TIME_W L"$timestamp"
"@

[System.IO.File]::WriteAllText($HeaderPath, $headerText, [System.Text.Encoding]::ASCII)

Write-Output "[OK] Bumped version to $versionText at $timestamp"
