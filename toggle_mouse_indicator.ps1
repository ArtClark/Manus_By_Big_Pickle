param(
    [ValidateSet("on","off","status")]
    [string]$Action = "status"
)

$path = "HKCU:\Control Panel\Mouse"
$name = "ShowLocation"

switch ($Action) {
    "on" {
        Set-ItemProperty -Path $path -Name $name -Value "1" -Type DWord
        Write-Host "Mouse indicator enabled. Press Ctrl to test." -ForegroundColor Green
    }
    "off" {
        Remove-ItemProperty -Path $path -Name $name -ErrorAction SilentlyContinue
        Write-Host "Mouse indicator disabled." -ForegroundColor Yellow
    }
    "status" {
        $val = Get-ItemProperty -Path $path -Name $name -ErrorAction SilentlyContinue
        if ($val -and $val.$name -eq 1) {
            Write-Host "Mouse indicator: ON" -ForegroundColor Green
        } else {
            Write-Host "Mouse indicator: OFF" -ForegroundColor Gray
        }
    }
}
