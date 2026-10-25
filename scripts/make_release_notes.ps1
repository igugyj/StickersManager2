$ErrorActionPreference = "Stop"

$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$ChangelogPath = Join-Path $Root "CHANGELOG.md"
$OutPath = Join-Path $Root "release_notes.md"

# 版本一致性校验：CHANGELOG 顶部为单源，appinfo.h / pkg.iss 必须一致，不一致直接终止
$null = & (Join-Path $PSScriptRoot "check_version.ps1")

$raw = Get-Content $ChangelogPath -Raw -Encoding utf8

$match = [regex]::Match($raw, '(?m)^# Changelog[^\n]*?v?([\d.]+)\s*→\s*v?([\d.]+)(?:\s*\((\d{4}\.\d{2}\.\d{2})\))?')
if (-not $match.Success) {
    throw "Cannot parse version section from CHANGELOG.md"
}

$prevTag = "v$($match.Groups[1].Value)"
$curTag = "v$($match.Groups[2].Value)"
$curVer = $match.Groups[2].Value
$date = if ($match.Groups[3].Success) { $match.Groups[3].Value } else { Get-Date -Format "yyyy.MM.dd" }

$bodyStart = $match.Index + $match.Length
$body = $raw.Substring($bodyStart)
$parts = $body -split '(?m)^# ', 2
$body = $parts[0].Trim()
$body = $body -replace '(?m)^\*\*Full Changelog\*\*:.*$', ''
$body = $body.Trim()

$notes = @"
# $curTag - $date

$body

### Packaging

portable: **StickersManager_$curVer .7z**
installer: **StickersManager_$curVer.exe**

**Full Changelog**: https://github.com/igugyj/StickersManager2/compare/$prevTag...$curTag
"@

Set-Content $OutPath $notes -Encoding UTF8
Write-Host "Release notes written: $OutPath"