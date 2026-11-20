# Runs idf.py with ESP-IDF v6.1 loaded, from ANY PowerShell (including the one Claude Code uses),
# on a Windows PC where ESP-IDF was installed with the Espressif Installation Manager (EIM).
#   ./tools/idf.ps1 --version
#   ./tools/idf.ps1 build
#   ./tools/idf.ps1 -p COM5 flash monitor
#   ./tools/idf.ps1 size
# Which install: the newest ESP-IDF 6.1.x listed in C:\Espressif\tools\eim_idf.json, read from each
# install's tools/cmake/version.cmake (EIM's "selected" install and its names can't be trusted:
# names may be commit hashes). Override with $env:GW_IDF_PATH = 'C:\esp\v6.1.1\esp-idf'.
# idf.py runs in a child PowerShell, so your own shell's PATH/venv/prompt stay untouched.
$ErrorActionPreference = 'Stop'
$WantMajor = 6; $WantMinor = 1
$json = if ($env:GW_EIM_JSON) { $env:GW_EIM_JSON } else { 'C:\Espressif\tools\eim_idf.json' }
if (-not (Test-Path $json)) {
    Write-Error "ESP-IDF (EIM) not found: $json is missing. Install ESP-IDF v$WantMajor.$WantMinor with EIM."
    exit 1
}
$eim = Get-Content $json -Raw | ConvertFrom-Json

# Real version from the install's tools/cmake/version.cmake; if that can't be read, the version
# in the install's EIM name or path (e.g. "v6.1", "C:\esp\v5.5.1\esp-idf").
function Get-IdfVersion($install) {
    # (not Join-Path: it throws when the drive doesn't exist, e.g. an unplugged E:)
    $f = ($install.path.TrimEnd('\') + '\tools\cmake\version.cmake')
    if (Test-Path $f -ErrorAction SilentlyContinue) {
        $t = Get-Content $f -Raw
        $v = foreach ($k in 'MAJOR', 'MINOR', 'PATCH') {
            if ($t -match "IDF_VERSION_$k\s+(\d+)") { [int]$Matches[1] } else { 0 }
        }
        return [version]::new($v[0], $v[1], $v[2])
    }
    foreach ($s in @($install.name, $install.path)) {
        if ($s -match 'v(\d+)\.(\d+)(?:\.(\d+))?') {
            $p = if ($Matches[3]) { [int]$Matches[3] } else { 0 }
            return [version]::new([int]$Matches[1], [int]$Matches[2], $p)
        }
    }
    return $null
}

$installs = foreach ($i in $eim.idfInstalled) {
    [pscustomobject]@{ Path = $i.path; Activate = $i.activationScript; Version = Get-IdfVersion $i }
}
if ($env:GW_IDF_PATH) {
    $idf = $installs | Where-Object { $_.Path -eq $env:GW_IDF_PATH } | Select-Object -First 1
    if (-not $idf) { Write-Error "GW_IDF_PATH=$env:GW_IDF_PATH is not an EIM install listed in $json"; exit 1 }
} else {
    $idf = $installs | Where-Object { $_.Version -and $_.Version.Major -eq $WantMajor -and $_.Version.Minor -eq $WantMinor } |
        Sort-Object Version -Descending | Select-Object -First 1
    if (-not $idf) {
        $found = ($installs | ForEach-Object { "$($_.Version) at $($_.Path)" }) -join '; '
        Write-Error "No ESP-IDF $WantMajor.$WantMinor.x install found via EIM (found: $found). Install v$WantMajor.$WantMinor with EIM or set GW_IDF_PATH."
        exit 1
    }
}

Write-Host "[idf.ps1] ESP-IDF $($idf.Version) at $($idf.Path)" -ForegroundColor DarkGray
if (-not (Test-Path $idf.Activate -ErrorAction SilentlyContinue)) { Write-Error "activation script missing: $($idf.Activate)"; exit 1 }

# Child process: loads the install's activation script (venv, toolchains, IDF_PATH), runs idf.py
# with our arguments, and exits — nothing leaks into the caller's session. Arguments travel as
# JSON inside an -EncodedCommand, so quotes and spaces survive.
$argJson = ConvertTo-Json -Compress -InputObject @($args | ForEach-Object { [string]$_ })
$child = @"
`$ErrorActionPreference = 'Continue'
`$ProgressPreference = 'SilentlyContinue'
. '$($idf.Activate -replace "'", "''")' *> `$null
`$a = @(ConvertFrom-Json '$($argJson -replace "'", "''")')
& idf.py @a
exit `$LASTEXITCODE
"@
$encoded = [Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($child))
# -OutputFormat Text: plain text even when the output is piped (else PowerShell emits CLIXML).
powershell.exe -NoProfile -ExecutionPolicy Bypass -OutputFormat Text -EncodedCommand $encoded
exit $LASTEXITCODE
