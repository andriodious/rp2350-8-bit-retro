#pragma once

// Core-0-only counters. The renderer's last conversion time is collected only
// after its frame_ready hand-off; no shared 64-bit counters are used.
#ifndef C64_PROFILE
#define C64_PROFILE 0
#endif
#if C64_PROFILE
#include <stdint.h>
#include <stdio.h>
#include "pico/stdlib.h"
namespace c64_profile {
enum Stage { CPU, VIC, SID, CIA, EMULATION, CONVERSION, WAIT, FRONTEND, PACING, COUNT };
struct Sample {
    uint64_t us[COUNT] = {};
    uint64_t elapsed = 0;
    uint32_t frames = 0, rendered = 0;
    bool turbo = false;
};
inline Sample active, last;
inline uint64_t started;
inline bool running;
inline uint32_t sequence;
inline void reset() { active = {}; running = false; }
inline void begin(uint64_t now, bool turbo) {
    if (!running || active.turbo != turbo) {
        active = {}; active.turbo = turbo; started = now; running = true;
    }
}
inline void add(Stage stage, uint32_t us) { active.us[stage] += us; }
inline bool finish(uint64_t now, bool rendered) {
    ++active.frames;
    active.rendered += rendered;
    if (now - started < 5000000) return false;
    active.elapsed = now - started; last = active; ++sequence;
    active = {}; active.turbo = last.turbo; started = now;
    return true;
}
// Only complete snapshots are exposed; status polling never resets counters.
inline void json(char *out, size_t size) {
    if (!size) return;
    int n = snprintf(out,size,
        ",\"c64_profile\":{\"sequence\":%lu,\"window_us\":%llu,\"frames\":%lu,\"rendered\":%lu,\"turbo\":%u,\"cpu_us\":%llu,\"vic_us\":%llu,\"sid_us\":%llu,\"cia_us\":%llu,\"emulation_us\":%llu,\"conversion_us\":%llu,\"wait_us\":%llu,\"frontend_us\":%llu,\"pacing_us\":%llu}",
        (unsigned long)sequence, (unsigned long long)last.elapsed,
        (unsigned long)last.frames, (unsigned long)last.rendered, last.turbo,
        (unsigned long long)last.us[CPU], (unsigned long long)last.us[VIC],
        (unsigned long long)last.us[SID], (unsigned long long)last.us[CIA],
        (unsigned long long)last.us[EMULATION], (unsigned long long)last.us[CONVERSION],
        (unsigned long long)last.us[WAIT], (unsigned long long)last.us[FRONTEND],
        (unsigned long long)last.us[PACING]);
    if (n < 0 || (size_t)n >= size) out[0] = 0;
}
inline void print() {
    printf("C64PROF window_us=%llu frames=%lu rendered=%lu turbo=%u cpu_us=%llu vic_us=%llu sid_us=%llu cia_us=%llu emulation_us=%llu conversion_us=%llu wait_us=%llu frontend_us=%llu pacing_us=%llu\n",
        (unsigned long long)last.elapsed, (unsigned long)last.frames,
        (unsigned long)last.rendered, last.turbo,
        (unsigned long long)last.us[CPU], (unsigned long long)last.us[VIC],
        (unsigned long long)last.us[SID], (unsigned long long)last.us[CIA],
        (unsigned long long)last.us[EMULATION], (unsigned long long)last.us[CONVERSION],
        (unsigned long long)last.us[WAIT], (unsigned long long)last.us[FRONTEND],
        (unsigned long long)last.us[PACING]);
}
}
#define C64_PROFILE_CALL(stage, ...) do { \
    uint32_t c64_profile_start = time_us_32(); \
    __VA_ARGS__; \
    c64_profile::add(c64_profile::stage, time_us_32() - c64_profile_start); \
} while (0)
#else
#define C64_PROFILE_CALL(stage, ...) do { __VA_ARGS__; } while (0)
#endif
