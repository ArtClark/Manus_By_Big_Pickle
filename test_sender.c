#include <stdio.h>
#include <string.h>
#include <windows.h>
int main() {
    const char *req1 = "Content-Length: 171\r\n\r\n{\"jsonrpc\":\"2.0\",\"id\":1,\"method\":\"initialize\",\"params\":{\"protocolVersion\":\"2024-11-05\",\"capabilities\":{},\"clientInfo\":{\"name\":\"test\",\"version\":\"1.0.0\"}}}";
    const char *req2 = "Content-Length: 42\r\n\r\n{\"jsonrpc\":\"2.0\",\"id\":2,\"method\":\"tools/list\",\"params\":{}}";
    // Write both to pipe
    HANDLE h = GetStdHandle(STD_INPUT_HANDLE);
    // Can't write to stdin - this is the wrong direction
    fprintf(stderr, "This test won't work as expected\n");
    return 0;
}
