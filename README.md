# Manus — Desktop GUI Agent for LLMs

Manus (Latin for "hand") is a lightweight MCP server that gives LLMs the ability to **see** and **control** a Windows desktop. It communicates over stdio via JSON-RPC 2.0 and bundles everything into a single ~190 KB executable with no external dependencies.

Also includes a Chrome extension and native messaging bridge for reading, interacting with, and injecting JavaScript into browser pages.

## Tools (19 total)

| Tool | Description |
|------|-------------|
| `screenshot` | Capture full virtual desktop as PNG (multi-monitor, DPI-aware) |
| `mouse_move` | Move cursor to (x, y) |
| `mouse_down` / `mouse_up` | Press/release mouse button (left/right). Auto-releases after 5s safety timeout |
| `mouse_click` | Click at optional (x, y) |
| `get_cursor_position` | Report current cursor coordinates |
| `show_user` | Circle a point 1.5x (r=30px) to draw user's attention. Uses Windows Ctrl-key accessibility ripple when enabled, software fallback otherwise |
| `get_window_at` | Find and describe window hierarchy at a screen coordinate (handle, class, text, rect, children) |
| `get_text` | Read text from a window by handle (WM_GETTEXT) |
| `read_page` | Select all + copy (Ctrl+A, Ctrl+C, Esc) the active window — works on any app without an extension |
| `type_text` | Type arbitrary text into the active window via keystrokes. `\n` for Enter |
| `get_page_html` | Get full HTML of the active browser page (needs extension + bridge) |
| `capture_browser_tab` | Capture visible browser tab as base64 PNG data URL |
| `get_page_links` | Get all links from the active browser page |
| `get_page_info` | Get title and URL of the active browser page |
| `click_page_link` | Click a link by index from `get_page_links` |
| `inject_page_js` | Execute JavaScript in a browser page |
| `copy` | Send Ctrl+C, return clipboard text |
| `paste` | Send Ctrl+V, optionally set clipboard text first |

## Quick Start

### Prerequisites
- Windows 7+ (64-bit)
- [MinGW-w64](https://www.mingw-w64.org/) or MSVC for building

### Build
```batch
cd c
g++ -O2 desktop_agent.cpp -lgdi32 -lgdiplus -luser32 -lole32 -loleaut32 -o desktop_agent.exe
g++ -O2 -mwindows whack_a_mole.c -luser32 -lgdi32 -o whack_a_mole.exe
```

Or use the included `build.bat`:
```batch
cd c
build
```

### Run as MCP Server
```batch
desktop_agent.exe
```

### MCP Configuration (opencode)
```json
{
  "mcpServers": {
    "manus": {
      "command": "C:\\path\\to\\c\\desktop_agent.exe"
    }
  }
}
```

## Browser Extension Setup

1. Build `native_bridge\native_bridge.exe` (already built):
   ```batch
   cd c\native_bridge
   gcc -O2 native_bridge.c -o native_bridge.exe
   ```
2. Run `install.bat` as Administrator to register the native messaging host
3. Open Chrome → `chrome://extensions/` → Enable "Developer mode" → Load unpacked → select `browser_extension/`
4. Note the extension ID, update `com.manus.bridge.json`, run `install.bat` again

## `show_user` Accessibility Feature

The `show_user` tool can use Windows' built-in "Show location of pointer when I press the CTRL key" ripple:
```powershell
.\toggle_mouse_indicator.ps1 -Action on
```

When enabled, `show_user` moves the cursor to the target and triggers the native ripple. When disabled, it falls back to a software circle animation.

## How an LLM Should Use This

1. Call `screenshot` to capture the current desktop
2. Analyze the PNG to identify UI elements and their coordinates
3. Call `mouse_move` + `mouse_click` to interact with buttons/fields
4. Call `copy` to read text from a selected field
5. Call `paste` to write text into a field
6. Call `show_user` to draw the human's attention to a specific area
7. Call `read_page` to capture full page content without an extension
8. Call `get_page_links` / `click_page_link` for browser navigation
9. Repeat: screenshot → analyze → act

## Project Structure

```
c/
├── desktop_agent.cpp          # MCP server (~980 lines, single file)
├── whack_a_mole.c             # Win32 test game
├── build.bat                  # Build script (MSVC + MinGW)
├── toggle_mouse_indicator.ps1 # Enable/disable Ctrl-key indicator
├── browser_extension/         # Chrome extension (Manifest V3)
│   ├── manifest.json
│   ├── background.js
│   └── content.js
├── native_bridge/             # Native messaging host
│   ├── native_bridge.c
│   ├── com.manus.bridge.json
│   └── install.bat
├── LICENSE
└── .gitignore
```

## Build Dependencies

- **Win32 API**: `windows.h`, GDI+, User32, Ole32, OleAut32
- **No external libraries** — no libcurl, no jsoncpp, no OpenCV
- **JSON parser**: ~130 lines embedded in `desktop_agent.cpp`
- **PNG encoding**: GDI+ (requires C++ compilation)

## License

MIT — do whatever you want, just don't bother the author.
