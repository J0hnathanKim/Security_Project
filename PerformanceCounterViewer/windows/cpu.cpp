// cpu_usage_windows.c
// Print total CPU usage (%) every 1 second using GetSystemTimes().
//
// Build (MSVC):
//   cl /W4 /O2 cpu_usage_windows.c
//
// Build (MinGW-w64):
//   gcc -O2 -Wall -Wextra cpu_usage_windows.c -o cpu_usage_windows.exe

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdint.h>

// Convert FILETIME to unsigned 64-bit integer (100-nanosecond units)
static uint64_t filetime_to_u64(const FILETIME *ft) {
    ULARGE_INTEGER u;
    u.LowPart  = ft->dwLowDateTime;
    u.HighPart = ft->dwHighDateTime;
    return (uint64_t)u.QuadPart;
}

// Returns 1 on success, 0 on failure
static int read_cpu_times(uint64_t *idle, uint64_t *kernel, uint64_t *user) {
    FILETIME ftIdle, ftKernel, ftUser;
    if (!GetSystemTimes(&ftIdle, &ftKernel, &ftUser)) {
        return 0;
    }
    *idle   = filetime_to_u64(&ftIdle);
    *kernel = filetime_to_u64(&ftKernel);
    *user   = filetime_to_u64(&ftUser);
    return 1;
}

// Compute CPU usage percentage between two samples.
// Note: In GetSystemTimes, kernel time includes idle time.
// total = (kernel + user) delta
// busy  = total - idle delta
static double cpu_usage_percent(uint64_t idle0, uint64_t kernel0, uint64_t user0,
                                uint64_t idle1, uint64_t kernel1, uint64_t user1) {
    uint64_t idle_delta   = idle1   - idle0;
    uint64_t kernel_delta = kernel1 - kernel0;
    uint64_t user_delta   = user1   - user0;

    uint64_t total = kernel_delta + user_delta;
    if (total == 0) return 0.0;

    uint64_t busy = total - idle_delta;
    return (double)busy * 100.0 / (double)total;
}

int main(void) {
    uint64_t idle_prev = 0, kernel_prev = 0, user_prev = 0;

    if (!read_cpu_times(&idle_prev, &kernel_prev, &user_prev)) {
        fprintf(stderr, "GetSystemTimes() failed. Error=%lu\n", GetLastError());
        return 1;
    }

    printf("Measuring total CPU usage... (press Ctrl+C to stop)\n");

    for (;;) {
        Sleep(1000);

        uint64_t idle_now = 0, kernel_now = 0, user_now = 0;
        if (!read_cpu_times(&idle_now, &kernel_now, &user_now)) {
            fprintf(stderr, "GetSystemTimes() failed. Error=%lu\n", GetLastError());
            return 1;
        }

        double usage = cpu_usage_percent(
            idle_prev, kernel_prev, user_prev,
            idle_now,  kernel_now,  user_now
        );

        printf("CPU Usage: %6.2f %%\n", usage);

        idle_prev = idle_now;
        kernel_prev = kernel_now;
        user_prev = user_now;
    }

    // unreachable
    // return 0;
}
