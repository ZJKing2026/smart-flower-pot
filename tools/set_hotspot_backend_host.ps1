[CmdletBinding(SupportsShouldProcess)]
param(
  [string]$HostAddress,
  [switch]$Apply
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$secretsPath = Join-Path $projectRoot "wifi_bridge_test\wifi_secrets.h"

function GetPreferredIPv4Address {
  # Returns a suitable IPv4 address for a local hotspot network.
  $addresses = foreach ($line in (ipconfig)) {
    if ($line -match 'IPv4[^:]*:\s*([0-9.]+)') {
      $Matches[1]
    }
  }

  $preferred = $addresses | Where-Object {
    $_ -notlike "127.*" -and $_ -notlike "169.254.*"
  } | Select-Object -First 1

  if ([string]::IsNullOrWhiteSpace($preferred)) {
    throw "No usable IPv4 address was found. Connect to the hotspot or provide -HostAddress."
  }

  return $preferred
}

function UpdateBackendHost {
  param(
    [string]$Path,
    [string]$Address
  )

  if (-not (Test-Path $Path)) {
    throw "ESP32 configuration file was not found: $Path"
  }

  $content = Get-Content -Raw -Encoding UTF8 $Path
  $pattern = 'constexpr char BACKEND_HOST\[\] = "[^"]+";'
  $replacement = "constexpr char BACKEND_HOST[] = `"$Address`";"

  if ($content -notmatch $pattern) {
    throw "BACKEND_HOST was not found in the configuration file."
  }

  $updated = [regex]::Replace($content, $pattern, $replacement, 1)
  Set-Content -Encoding UTF8 -NoNewline -Path $Path -Value $updated
}

if ([string]::IsNullOrWhiteSpace($HostAddress)) {
  $HostAddress = GetPreferredIPv4Address
}

$parsedAddress = $null
if (-not [System.Net.IPAddress]::TryParse($HostAddress, [ref]$parsedAddress) -or
    $parsedAddress.AddressFamily -ne [System.Net.Sockets.AddressFamily]::InterNetwork) {
  throw "HostAddress must be a valid IPv4 address."
}

Write-Output "Suggested BACKEND_HOST: $HostAddress"
Write-Output "Mobile page URL: http://${HostAddress}:8000"

if (-not $Apply) {
  Write-Output "No file was changed. Run .\tools\set_hotspot_backend_host.ps1 -Apply to update BACKEND_HOST."
  exit 0
}

if ($PSCmdlet.ShouldProcess($secretsPath, "Update BACKEND_HOST to $HostAddress")) {
  UpdateBackendHost -Path $secretsPath -Address $HostAddress
  Write-Output "BACKEND_HOST updated. Re-upload wifi_bridge_test with Arduino IDE."
}
