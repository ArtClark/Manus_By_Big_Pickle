#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static char *read_msg(void) {
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    unsigned long cl = 0;
    char line[4096];
    size_t pos = 0;
    int eol_count = 0;
    
    while (1) {
        char c; DWORD r;
        fprintf(stderr, "Reading byte...\n");
        if (!ReadFile(h, &c, 1, &r, NULL) || r == 0) {
            fprintf(stderr, "ReadFile returned 0 or failed, err=%lu\n", GetLastError());
            return NULL;
        }
        fprintf(stderr, "  Got: 0x%02x '%c'\n", (unsigned char)c, c >= 32 ? c : '.');
        
        if (c == '\r') { fprintf(stderr, "  (skipping CR)\n"); continue; }
        if (c == '\n') {
            if (pos == 0) {
                fprintf(stderr, "  Empty line - end of headers. Content-Length=%lu\n", cl);
                break;
            }
            line[pos] = 0;
            fprintf(stderr, "  Header line: '%s'\n", line);
            if (sscanf(line, "Content-Length: %lu", &cl) == 1) {
                fprintf(stderr, "  Parsed Content-Length: %lu\n", cl);
            }
            pos = 0;
        } else {
            if (pos < sizeof(line) - 1) line[pos++] = c;
        }
    }
    
    if (cl > 0) {
        char *body = (char*)malloc(cl + 1);
        DWORD total = 0;
        while (total < cl) {
            DWORD r;
            fprintf(stderr, "  Reading body chunk at offset %lu, size %lu\n", total, cl - total);
            if (!ReadFile(h, body + total, cl - total, &r, NULL) || r == 0) {
                fprintf(stderr, "  Body read failed at offset %lu (r=%lu, err=%lu)\n", total, r, GetLastError());
                free(body);
                return NULL;
            }
            fprintf(stderr, "  Read %lu bytes\n", r);
            total += r;
        }
        body[cl] = 0;
        fprintf(stderr, "  Body: '%.50s...'\n", body);
        return body;
    }
    return NULL;
}

int main() {
    fprintf(stderr, "=== Starting read test ===\n");
    int count = 0;
    while (1) {
        fprintf(stderr, "\n--- Calling read_msg #%d ---\n", count + 1);
        char *msg = read_msg();
        if (!msg) {
            fprintf(stderr, "read_msg returned NULL\n");
            break;
        }
        fprintf(stderr, "Got message #%d: %s\n", count + 1, msg);
        free(msg);
        count++;
        if (count >= 3) break;
    }
    fprintf(stderr, "=== Done, read %d messages ===\n", count);
    return 0;
}
