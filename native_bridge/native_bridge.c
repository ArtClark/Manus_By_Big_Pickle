#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PIPE_NAME "\\\\.\\pipe\\manus_bridge"
#define BUF_SIZE 65536
#define TIMEOUT_MS 10000

static HANDLE g_event_cmd, g_event_resp;
static char g_cmd_buf[BUF_SIZE];
static char g_resp_buf[BUF_SIZE];
static int g_cmd_len, g_resp_len;
static CRITICAL_SECTION g_cs;

/* Write a 4-byte length-prefixed message to stdout (Chrome native messaging) */
static int write_stdout(const char *msg, int len) {
    unsigned char hdr[4] = {
        (unsigned char)(len & 0xff),
        (unsigned char)((len >> 8) & 0xff),
        (unsigned char)((len >> 16) & 0xff),
        (unsigned char)((len >> 24) & 0xff)
    };
    DWORD wrote;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (!WriteFile(h, hdr, 4, &wrote, NULL) || wrote != 4) return -1;
    if (!WriteFile(h, msg, len, &wrote, NULL) || wrote != (DWORD)len) return -1;
    return 0;
}

/* Read a 4-byte length-prefixed message from stdin (Chrome native messaging) */
static int read_stdin(char *buf, int cap) {
    unsigned char hdr[4];
    DWORD read;
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    if (!ReadFile(h, hdr, 4, &read, NULL) || read != 4) return -1;
    int len = hdr[0] | (hdr[1] << 8) | (hdr[2] << 16) | (hdr[3] << 24);
    if (len < 0 || len >= cap) return -1;
    if (!ReadFile(h, buf, len, &read, NULL) || read != (DWORD)len) return -1;
    buf[len] = 0;
    return len;
}

/* Thread: listens on named pipe for MCP connections */
static DWORD WINAPI pipe_thread(LPVOID arg) {
    (void)arg;
    while (1) {
        HANDLE hPipe = CreateNamedPipeA(PIPE_NAME,
            PIPE_ACCESS_DUPLEX,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT,
            PIPE_UNLIMITED_INSTANCES,
            BUF_SIZE, BUF_SIZE,
            0, NULL);
        if (hPipe == INVALID_HANDLE_VALUE) {
            Sleep(1000);
            continue;
        }
        if (!ConnectNamedPipe(hPipe, NULL)) {
            CloseHandle(hPipe);
            continue;
        }

        /* Read command from MCP */
        DWORD read;
        if (!ReadFile(hPipe, g_cmd_buf, BUF_SIZE - 1, &read, NULL)) {
            CloseHandle(hPipe);
            continue;
        }
        g_cmd_buf[read] = 0;
        g_cmd_len = (int)read;

        EnterCriticalSection(&g_cs);
        g_cmd_len = (int)read;
        LeaveCriticalSection(&g_cs);
        SetEvent(g_event_cmd);

        /* Wait for response (from main thread) */
        if (WaitForSingleObject(g_event_resp, TIMEOUT_MS) == WAIT_OBJECT_0) {
            DWORD wrote;
            WriteFile(hPipe, g_resp_buf, g_resp_len, &wrote, NULL);
        } else {
            const char *err = "{\"error\":\"Timeout waiting for Chrome response\"}";
            DWORD wrote;
            WriteFile(hPipe, err, (DWORD)strlen(err), &wrote, NULL);
        }
        FlushFileBuffers(hPipe);
        DisconnectNamedPipe(hPipe);
        CloseHandle(hPipe);
    }
    return 0;
}

int main(void) {
    /* Don't show console window for Chrome native messaging */
    /* Chrome expects silent stdin/stdout communication */

    InitializeCriticalSection(&g_cs);
    g_event_cmd = CreateEventA(NULL, FALSE, FALSE, NULL);
    g_event_resp = CreateEventA(NULL, FALSE, FALSE, NULL);

    HANDLE hPipeThread = CreateThread(NULL, 0, pipe_thread, NULL, 0, NULL);

    /* Main loop: read from stdin (Chrome messages), foward responses to pipe thread */
    while (1) {
        HANDLE events[2] = { g_event_cmd, GetStdHandle(STD_INPUT_HANDLE) };
        /* Wait for either a command to forward or data from Chrome */
        /* Can't overlapped stdin easily, so use alternating strategy */

        /* First, check if command is pending */
        if (WaitForSingleObject(g_event_cmd, 50) == WAIT_OBJECT_0) {
            /* Send command to Chrome */
            EnterCriticalSection(&g_cs);
            int len = g_cmd_len;
            LeaveCriticalSection(&g_cs);

            if (write_stdout(g_cmd_buf, len) != 0) {
                /* Chrome disconnected */
                break;
            }

            /* Read response from Chrome */
            int rlen = read_stdin(g_resp_buf, BUF_SIZE);
            if (rlen < 0) break;

            EnterCriticalSection(&g_cs);
            g_resp_len = (int)rlen;
            LeaveCriticalSection(&g_cs);
            SetEvent(g_event_resp);
        } else {
            /* Poll: try reading from stdin non-blockingly */
            /* We use PeekNamedPipe to check if data is available */
            HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
            DWORD avail = 0;
            if (PeekNamedPipe(hStdIn, NULL, 0, NULL, &avail, NULL) && avail > 0) {
                /* Unsolicited Chrome message - ignore for now */
                char tmp[4096];
                int len = read_stdin(tmp, sizeof(tmp));
                if (len < 0) break;
                /* Could log or handle here */
            }
        }
    }

    CloseHandle(g_event_cmd);
    CloseHandle(g_event_resp);
    DeleteCriticalSection(&g_cs);
    TerminateThread(hPipeThread, 0);
    CloseHandle(hPipeThread);
    return 0;
}
