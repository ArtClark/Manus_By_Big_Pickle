@echo off
echo Building Desktop Agent MCP Server...
echo.

REM Try MSVC first
where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [MSVC] Building desktop_agent.exe ...
    cl /nologo /O2 /W4 /EHsc desktop_agent.cpp /link gdi32.lib gdiplus.lib user32.lib ole32.lib oleaut32.lib /out:desktop_agent.exe
    if %ERRORLEVEL% equ 0 (
        echo [OK] desktop_agent.exe
    ) else (
        echo [FAIL] MSVC build failed
    )
) else (
    echo [SKIP] MSVC (cl.exe) not found
)

echo.
echo Building Whack-a-Mole Test Game...
echo.

where cl >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [MSVC] Building whack_a_mole.exe ...
    cl /nologo /O2 /W4 whack_a_mole.c /link user32.lib gdi32.lib /out:whack_a_mole.exe
    if %ERRORLEVEL% equ 0 (
        echo [OK] whack_a_mole.exe
    ) else (
        echo [FAIL] MSVC build failed
    )
) else (
    echo [SKIP] MSVC (cl.exe) not found
)

echo.
echo Trying MinGW (g++) if available...
where g++ >nul 2>nul
if %ERRORLEVEL% equ 0 (
    echo [MinGW] Building resource ...
    windres manus.rc -O coff -o manus.res
    echo [MinGW] Building desktop_agent.exe ...
    g++ -O2 -Wall desktop_agent.cpp manus.res -lgdi32 -lgdiplus -luser32 -lole32 -loleaut32 -o desktop_agent.exe
    if %ERRORLEVEL% equ 0 ( echo [OK] desktop_agent.exe ) else ( echo [FAIL] )

    echo [MinGW] Building whack_a_mole.exe ...
    gcc -O2 -Wall whack_a_mole.c -lgdi32 -luser32 -o whack_a_mole.exe
    if %ERRORLEVEL% equ 0 ( echo [OK] whack_a_mole.exe ) else ( echo [FAIL] )
) else (
    echo [SKIP] MinGW not found
)

echo.
echo Done.
