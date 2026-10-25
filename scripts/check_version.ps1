# Version consistency check: CHANGELOG.md top section is the single source,
# src/appinfo.h and pkg.iss must match it (all normalized: optional "v" prefix ignored).
# Outputs two lines for CI: "version=X.Y.Z" and "tag=vX.Y.Z". Throws on any mismatch.
$ErrorActionPreference = "Stop"

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

# --- source: CHANGELOG.md top section ---
$raw = Get-Content (Join-Path $Root "CHANGELOG.md") -Raw -Encoding utf8
$m = [regex]::Match($raw, '(?m)^# Changelog[^\n]*?→\s*v?([\d.]+)')
if (-not $m.Success) { throw "check_version: cannot parse version from CHANGELOG.md (expected line: '# Changelog ... → vA → vB')" }
$version = $m.Groups[1].Value

# --- appinfo.h: inline QString version() { return "v2.8.0"; } ---
$ai = Get-Content (Join-Path $Root "src/appinfo.h") -Raw -Encoding utf8
$am = [regex]::Match($ai, 'version\(\)\s*\{\s*return\s+"v?([\d.]+)"')
if (-not $am.Success) { throw "check_version: cannot parse version() from src/appinfo.h" }
$appVer = $am.Groups[1].Value

# --- pkg.iss: #define MyAppVersion "2.8.0" (inside #ifndef fallback) ---
$iss = Get-Content (Join-Path $Root "pkg.iss") -Raw -Encoding utf8
$pm = [regex]::Match($iss, '(?m)^\s*#define\s+MyAppVersion\s+"v?([\d.]+)"')
if (-not $pm.Success) { throw "check_version: cannot parse MyAppVersion from pkg.iss" }
$pkgVer = $pm.Groups[1].Value

# --- compare ---
if ($appVer -ne $version) {
    throw "check_version: src/appinfo.h version '$appVer' != CHANGELOG.md '$version'. CHANGELOG top section is the single source; bump it in sync."
}
if ($pkgVer -ne $version) {
    throw "check_version: pkg.iss MyAppVersion '$pkgVer' != CHANGELOG.md '$version'. CHANGELOG top section is the single source; bump it in sync."
}

# Status line goes to stderr: stdout is reserved for the version=/tag= lines so that
# `check_version.ps1 | Out-File $GITHUB_OUTPUT` never writes an invalid output entry.
[Console]::Error.WriteLine("check_version OK: CHANGELOG=$version appinfo=$appVer pkg.iss=$pkgVer")

"version=$version"
"tag=v$version"
