@echo off
REM Install native messaging host for current user
REM Run as Administrator if installing for all users

set "BRIDGE_DIR=%~dp0"
set "JSON_PATH=%BRIDGE_DIR%com.manus.bridge.json"

REM Update JSON with absolute path to the exe
powershell -Command "(Get-Content '%JSON_PATH%') -replace '\"native_bridge.exe\"', '\""%BRIDGE_DIR%native_bridge.exe"\"" | Set-Content '%JSON_PATH%'"

REM Register in registry (HKCU = current user only)
reg add "HKCU\Software\Google\Chrome\NativeMessagingHosts\com.manus.bridge" /ve /t REG_SZ /d "%JSON_PATH%" /f

echo Installed native messaging host: com.manus.bridge
echo.
echo Next steps:
echo 1. Open Chrome Extensions (chrome://extensions/)
echo 2. Enable "Developer mode"
echo 3. Load unpacked extension from: %BRIDGE_DIR%..\browser_extension
echo 4. Note the extension ID (e.g. abcdefghijklmnopabcdefghijklmn)
echo 5. Update com.manus.bridge.json with that extension ID
echo 6. Run install.bat again
