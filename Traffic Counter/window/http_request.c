// http_counter_win.c
// Windows: Count incoming HTTP requests by accepting TCP connections (no log parsing).
//
// Build (MSVC):
//   cl /W4 /O2 http_counter_win.c /link ws2_32.lib
//
// Build (MinGW-w64):
//   gcc -O2 -Wall -Wextra http_counter_win.c -lws2_32 -o http_counter_win.exe
//
// Run:
//   http_counter_win.exe
// Then test from another terminal:
//   curl http://127.0.0.1:8080/
//   ab -n 1000 -c 50 http://127.0.0.1:8080/   (ApacheBench installed if available)

#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <stdio.h>

#pragma comment(lib, "ws2_32.lib")

static volatile LONG g_total_requests = 0;

// Very small HTTP response
static const char *RESP =
    "HTTP/1.1 200 OK\r\n"
    "Content-Type: text/plain\r\n"
    "Content-Length: 2\r\n"
    "Connection: close\r\n"
    "\r\n"
    "OK";

DWORD WINAPI reporter_thread(LPVOID unused) {
    (void)unused;
    LONG prev_total = 0;

    for (;;) {
        Sleep(1000);

        LONG total = InterlockedCompareExchange(&g_total_requests, 0, 0);
        LONG rps = total - prev_total;
        prev_total = total;

        printf("[Traffic] RPS=%ld, Total=%ld\n", rps, total);
        fflush(stdout);
    }
    // unreachable
    // return 0;
}

static int init_winsock(void) {
    WSADATA wsa;
    int rc = WSAStartup(MAKEWORD(2, 2), &wsa);
    if (rc != 0) {
        fprintf(stderr, "WSAStartup failed: %d\n", rc);
        return 0;
    }
    return 1;
}

int main(void) {
    if (!init_winsock()) return 1;

    // Start reporter
    HANDLE hReporter = CreateThread(NULL, 0, reporter_thread, NULL, 0, NULL);
    if (!hReporter) {
        fprintf(stderr, "CreateThread failed: %lu\n", GetLastError());
        WSACleanup();
        return 1;
    }

    SOCKET listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listen_sock == INVALID_SOCKET) {
        fprintf(stderr, "socket() failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    // Allow quick restart
    BOOL opt = TRUE;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt));

    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(8080);
    addr.sin_addr.s_addr = htonl(INADDR_ANY); // 0.0.0.0

    if (bind(listen_sock, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        fprintf(stderr, "bind() failed: %d\n", WSAGetLastError());
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }

    if (listen(listen_sock, SOMAXCONN) == SOCKET_ERROR) {
        fprintf(stderr, "listen() failed: %d\n", WSAGetLastError());
        closesocket(listen_sock);
        WSACleanup();
        return 1;
    }

    printf("HTTP counter listening on http://0.0.0.0:8080/\n");
    printf("Press Ctrl+C to stop.\n");

    for (;;) {
        struct sockaddr_in client;
        int clen = sizeof(client);
        SOCKET client_sock = accept(listen_sock, (struct sockaddr*)&client, &clen);
        if (client_sock == INVALID_SOCKET) {
            int err = WSAGetLastError();
            // transient errors can happen; keep running
            fprintf(stderr, "accept() failed: %d\n", err);
            continue;
        }

        // Read *some* bytes to confirm we got an HTTP request.
        // For counting traffic, "a connection with request bytes" counts as 1 request (simple model).
        char buf[2048];
        int n = recv(client_sock, buf, (int)sizeof(buf), 0);
        if (n > 0) {
            InterlockedIncrement(&g_total_requests);

            // Respond (best-effort)
            send(client_sock, RESP, (int)strlen(RESP), 0);
        }

        closesocket(client_sock);
    }

    // unreachable cleanup
    // closesocket(listen_sock);
    // WSACleanup();
    // return 0;
}
