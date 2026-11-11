// A bounded power-on banner. The upload staging area is idle until the HTTP
// listener is enabled after this function returns. No permanent framebuffer RAM.
#include "boot_banner.h"
#include "board.h"
#include "drive.h"
#include "machine_if.h"
#include "net_if.h"
#include "pico/stdlib.h"
#include "hardware/watchdog.h"
#include "hardware/sync.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if NIMBUS_TEST
#include "presto_bluetooth.h"
#endif
#include "boot_logo.h"
#if WITH_SOUND
#include "sound.h"
#endif
#ifndef RETRO_BUILD_ID
#define RETRO_BUILD_ID "development"
#endif

// Original compact 5x7 uppercase bitmap alphabet, one row per byte.
static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789 .:-/";
static const uint8_t glyphs[][7] = {
 {14,17,17,31,17,17,17},{30,17,17,30,17,17,30},{14,17,16,16,16,17,14},
 {30,17,17,17,17,17,30},{31,16,16,30,16,16,31},{31,16,16,30,16,16,16},
 {14,17,16,23,17,17,15},{17,17,17,31,17,17,17},{14,4,4,4,4,4,14},
 {7,2,2,2,18,18,12},{17,18,20,24,20,18,17},{16,16,16,16,16,16,31},
 {17,27,21,21,17,17,17},{17,25,21,19,17,17,17},{14,17,17,17,17,17,14},
 {30,17,17,30,16,16,16},{14,17,17,17,21,18,13},{30,17,17,30,20,18,17},
 {15,16,16,14,1,1,30},{31,4,4,4,4,4,4},{17,17,17,17,17,17,14},
 {17,17,17,17,17,10,4},{17,17,17,21,21,21,10},{17,17,10,4,10,17,17},
 {17,17,10,4,4,4,4},{31,1,2,4,8,16,31},
 {14,17,19,21,25,17,14},{4,12,4,4,4,4,14},{14,17,1,2,4,8,31},
 {30,1,1,14,1,1,30},{2,6,10,18,31,2,2},{31,16,16,30,1,1,30},
 {14,16,16,30,17,17,14},{31,1,2,4,8,8,8},{14,17,17,14,17,17,14},
 {14,17,17,15,1,1,14},{0,0,0,0,0,0,0},{0,0,0,0,0,12,12},
 {0,12,12,0,12,12,0},{0,0,0,31,0,0,0},{1,2,2,4,8,8,16}
};
#define ROW_BYTES (BOARD_DISPLAY_ROW_WORDS * 4)
#define BANNER_ROWS 240
_Static_assert(2 * BANNER_ROWS * ROW_BYTES <= DRIVE_IMAGE_MAX, "banner exceeds upload staging");

static void pixel(uint32_t *fb, int x, int y, unsigned colour) {
    if (x < 0 || x >= 480 || y < 0 || y >= BANNER_ROWS) return;
#if MACHINE_C64 || MACHINE_SPECTRUM
    uint32_t rgb = ((colour & 4) ? 0xf800 : 0) | ((colour & 2) ? 0x7e0 : 0) | ((colour & 1) ? 31 : 0);
    uint32_t pins;
    __asm("rbit %0, %1" : "=r"(pins) : "r"(rgb << 16));
    ((uint16_t *)(fb + y * BOARD_DISPLAY_ROW_WORDS))[x] = pins;
#else
    uint32_t *p = fb + y * BOARD_DISPLAY_ROW_WORDS + x / 8;
    unsigned shift = (x % 8) * 4;
    *p = (*p & ~(15u << shift)) | ((colour & 7u) << shift);
#endif
}
static void text(uint32_t *fb, int x, int y, const char *s) {
    for (; *s && x + 10 <= 464; s++, x += 12) {
        const char *g = strchr(alphabet, toupper((unsigned char)*s));
        if (!g) continue;
        for (int yy = 0; yy < 7; yy++) for (int xx = 0; xx < 5; xx++)
            if (glyphs[g-alphabet][yy] & (16 >> xx)) {
                pixel(fb, x+xx*2, y+yy, 7); pixel(fb, x+xx*2+1, y+yy, 7);
            }
    }
}
static void read_status(char *value, size_t size, char *line, size_t line_size) {
#if BOARD_HAS_NET
    net_boot_status(value, size);
#else
    snprintf(value, size, "UNAVAILABLE");
#endif
#if NIMBUS_TEST
    snprintf(line, line_size, "JOYSTICK: %s", presto_bluetooth_boot_status());
#else
    snprintf(line, line_size, "JOYSTICK: DISABLED IN BUILD");
#endif
}
static void status(uint32_t *fb, const char *value, const char *line) {
    // Only rewrite status rows; the logo and build text stay stable.
    for (int y = 110; y < 180; y++) memset(fb + y * BOARD_DISPLAY_ROW_WORDS, 0, ROW_BYTES - 4);
    text(fb, 20, 114, "DHCP IP ADDRESS");
    text(fb, 20, 127, value);
    text(fb, 20, 155, line);
}
static void retire_display_table(void) {
    uint32_t frame = board_display_frames();
    while ((uint32_t)(board_display_frames() - frame) < 2) {
        watchdog_update();
        sleep_ms(1);
    }
}
void boot_banner_show(void) {
    // Web machine switches use a watchdog restart: leave their network and
    // controller startup path untouched, and skip recovery restarts too.
    if (watchdog_caused_reboot()) return;
    // Recovery paths without an initialized display/staging area safely skip it.
    uint32_t *fb = (uint32_t *)drive0_staging();
    const uint16_t *const *previous = board_display_get_rows();
    if (!fb || !previous) return;
    const uint16_t **rows = malloc(2 * (BOARD_DISPLAY_HEIGHT + 1) * sizeof *rows);
    if (!rows) return; // Failure must never prevent the emulator from booting.
    const unsigned words = BANNER_ROWS * BOARD_DISPLAY_ROW_WORDS;
    uint32_t *buffers[2] = {fb, fb + words};
    const uint16_t **tables[2] = {rows, rows + BOARD_DISPLAY_HEIGHT + 1};
    memset(fb, 0, 2 * BANNER_ROWS * ROW_BYTES);
    for (int y = 0; y < BANNER_ROWS; y++) {
#if !MACHINE_C64 && !MACHINE_SPECTRUM
        fb[(y + 1) * BOARD_DISPLAY_ROW_WORDS - 1] = BOARD_DISPLAY_ROW_END;
#endif
        rows[y*2] = rows[y*2+1] = (const uint16_t *)(fb + y * BOARD_DISPLAY_ROW_WORDS);
    }
    rows[480] = NULL;
    for (int y = 0; y < 480; y++) tables[1][y] = (const uint16_t *)(buffers[1] + (y/2) * BOARD_DISPLAY_ROW_WORDS);
    tables[1][480] = NULL;
    for (int y=0; y<48; y++) for (int x=0; x<48; x++) {
        pixel(fb, 16+x*2, 8+y, boot_logo[y*48+x]);
        pixel(fb, 17+x*2, 8+y, boot_logo[y*48+x]);
    }
    text(fb, 132, 20, "RP2350");
    text(fb, 132, 34, "8-BIT RETRO");
    text(fb, 20, 72, "BUILD " RETRO_BUILD_ID);
    text(fb, 20, 86, __DATE__ " " __TIME__);
    text(fb, 20, 196, "WEB: 8BITRETRO.LOCAL");
    text(fb, 20, 216, "STARTING EMULATOR...");
    char value[48], line[64], last_value[48], last_line[64];
    read_status(value, sizeof value, line, sizeof line);
    status(fb, value, line);
    strcpy(last_value, value); strcpy(last_line, line);
    // Never expose an old emulator scanline/cursor while changing row tables.
    board_display_backlight(0);
    board_display_set_rows(rows);
    retire_display_table();
    board_display_backlight(255);
    unsigned active = 0;
    const uint64_t started = time_us_64();
    const uint64_t minimum_until = started + 3000000;
    const uint64_t network_timeout = started + 30000000;
    uint64_t address_until = 0;
    bool wait_for_address = false;
#if BOARD_HAS_NET
    wait_for_address = net_boot_configured();
#endif
    do {
        watchdog_update();
#if BOARD_HAS_NET
        net_boot_poll();
#endif
#if NIMBUS_TEST
        presto_bluetooth_boot_poll();
#endif
        read_status(value, sizeof value, line, sizeof line);
        if (strcmp(value, last_value) || strcmp(line, last_line)) {
            unsigned next = active ^ 1u;
            memcpy(buffers[next], buffers[active], BANNER_ROWS * ROW_BYTES);
            status(buffers[next], value, line);
            board_display_set_rows(tables[next]);
            // The previous buffer must leave DMA before it can be drawn into.
            retire_display_table();
            active = next;
            strcpy(last_value, value); strcpy(last_line, line);
        }
#if BOARD_HAS_NET
        // Start the reading interval only after the IP text has reached scanout.
        if (wait_for_address && !address_until && net_boot_has_address())
            address_until = time_us_64() + 2000000;
#endif
        uint64_t now = time_us_64();
        bool network_done = !wait_for_address ||
            (address_until ? now >= address_until : now >= network_timeout);
        if (now >= minimum_until && network_done) break;
        sleep_ms(20);
    } while (true);
    board_display_backlight(0);
    board_display_set_rows(previous);
    retire_display_table();
    board_display_backlight(255);
    free(rows);
}

#if BOOT_STATUS_TRIAL
#include "boot_button.h"
#include "firmware_identity.h"
// Runtime buffers must not borrow the upload staging area. Every machine reserves
// the top 512 KiB of PSRAM in this trial, including Beeb's disc and big allocator.
_Static_assert(2 * BANNER_ROWS * ROW_BYTES <= BOARD_STATUS_BYTES, "status reservation too small");
static const uint16_t **runtime_rows;
static const uint16_t *const *emulator_rows;
static bool status_visible;
#if MACHINE_C64 || MACHINE_SPECTRUM
static uint32_t *status_sram;
#endif
static unsigned status_active;
static uint32_t status_frame, status_updated_ms;

static bool status_allocate(void) {
    if (runtime_rows) return true;
    if (!board_display_get_rows() || !board_psram() || board_psram_size() < BOARD_STATUS_BYTES) return false;
    runtime_rows = malloc(2 * (BOARD_DISPLAY_HEIGHT + 1) * sizeof *runtime_rows);
    if (!runtime_rows) return false;
    uint32_t *base = (uint32_t *)(board_psram() + board_psram_size() - BOARD_STATUS_BYTES);
#if MACHINE_C64 || MACHINE_SPECTRUM
    return true; // The borrowed SRAM buffer is wired after its pixels are saved.
#else
    memset(base, 0, 2 * BANNER_ROWS * ROW_BYTES);
    for (unsigned b = 0; b < 2; b++) {
        uint32_t *fb = base + b * BANNER_ROWS * BOARD_DISPLAY_ROW_WORDS;
        const uint16_t **rows = runtime_rows + b * (BOARD_DISPLAY_HEIGHT + 1);
        for (unsigned y = 0; y < BANNER_ROWS; y++) {
#if !MACHINE_C64 && !MACHINE_SPECTRUM
            fb[(y + 1) * BOARD_DISPLAY_ROW_WORDS - 1] = BOARD_DISPLAY_ROW_END;
#endif
            rows[y * 2] = rows[y * 2 + 1] = (const uint16_t *)(fb + y * BOARD_DISPLAY_ROW_WORDS);
        }
        rows[BOARD_DISPLAY_HEIGHT] = NULL;
    }
#endif
    // Retained for the session: DMA may still reference a table after switching.
    return true;
}

static void status_draw(unsigned buffer) {
    uint32_t *fb = (uint32_t *)(board_psram() + board_psram_size() - BOARD_STATUS_BYTES);
#if MACHINE_C64 || MACHINE_SPECTRUM
    (void)buffer;
    fb = status_sram;
#else
    fb += buffer * BANNER_ROWS * BOARD_DISPLAY_ROW_WORDS;
#endif
    for (unsigned y = 0; y < BANNER_ROWS; y++)
        memset(fb + y * BOARD_DISPLAY_ROW_WORDS, 0, ROW_BYTES - 4);
    text(fb, 20, 15, "8-BIT RETRO STATUS");
    char line[64], value[48], joystick[64];
    snprintf(line, sizeof line, "MACHINE: %s", machine_name()); text(fb, 20, 36, line);
    text(fb, 20, 54, "IMAGE " RETRO_IMAGE_ID);
    unsigned c0, c1, speed; machine_perf_get(&c0, &c1, &speed);
    snprintf(line, sizeof line, "SPEED %u / CPU %u / %u", speed, c0, c1); text(fb, 20, 75, line);
    read_status(value, sizeof value, joystick, sizeof joystick);
    status(fb, value, joystick);
    text(fb, 20, 192, "WEB: 8BITRETRO.LOCAL");
    text(fb, 20, 211, "BOOT: RETURN TO EMULATOR");
    text(fb, 20, 227, "EMULATION CONTINUES");
}
#endif

void boot_status_frame(void) {
#if BOOT_STATUS_TRIAL
    static bool gpio_ready;
    static boot_button_t button;
    static bool pending_toggle;
    if (!gpio_ready) {
        // Pimoroni Presto schematic: USER_SW on GP46, active low. This reads
        // the dedicated input; it does not manipulate QSPI chip select.
        gpio_init(BOARD_BOOT_BUTTON_PIN);
        gpio_set_dir(BOARD_BOOT_BUTTON_PIN, GPIO_IN);
        gpio_pull_up(BOARD_BOOT_BUTTON_PIN);
        gpio_ready = true;
    }
    uint32_t now = to_ms_since_boot(get_absolute_time());
    if (boot_button_press(&button, !gpio_get(BOARD_BOOT_BUTTON_PIN), now))
        pending_toggle = !pending_toggle;
    uint32_t frame = board_display_frames();
    // Allow the previous scanout table/buffer to retire without blocking audio.
    if ((uint32_t)(frame - status_frame) < 2) return;
    if (pending_toggle) {
        pending_toggle = false;
        if (status_visible) {
#if MACHINE_C64 || MACHINE_SPECTRUM
            memcpy(status_sram, board_psram()+board_psram_size()-BOARD_STATUS_BYTES, BANNER_ROWS*ROW_BYTES);
#endif
            board_display_set_rows(emulator_rows);
#if MACHINE_C64 || MACHINE_SPECTRUM
            machine_status_framebuffer(false);
#endif
#if WITH_SOUND
            audio_status_mute(false);
#endif
            status_visible = false;
        } else if (status_allocate()) {
            emulator_rows = board_display_get_rows();
#if WITH_SOUND
            audio_status_mute(true);
#endif
#if MACHINE_C64 || MACHINE_SPECTRUM
            status_sram = machine_status_framebuffer(true);
            memcpy(board_psram()+board_psram_size()-BOARD_STATUS_BYTES, status_sram, BANNER_ROWS*ROW_BYTES);
            for(unsigned b=0;b<2;b++) {
                const uint16_t **t=runtime_rows+b*(BOARD_DISPLAY_HEIGHT+1);
                for(unsigned y=0;y<480;y++)t[y]=(const uint16_t *)(status_sram+(y/2)*BOARD_DISPLAY_ROW_WORDS);
                t[480]=NULL;
            }
#endif
            status_active ^= 1u;
            status_draw(status_active);
            __dmb();
            board_display_set_rows(runtime_rows + status_active * (BOARD_DISPLAY_HEIGHT + 1));
            status_visible = true;
        }
        status_frame = frame; status_updated_ms = now;
    } else if (status_visible && (uint32_t)(now - status_updated_ms) >= 1000) {
        status_active ^= 1u;
        status_draw(status_active);
        __dmb();
        board_display_set_rows(runtime_rows + status_active * (BOARD_DISPLAY_HEIGHT + 1));
        status_frame = frame; status_updated_ms = now;
    }
#endif
}

void boot_status_flash_begin(void) {
#if BOOT_STATUS_TRIAL && !MACHINE_C64 && !MACHINE_SPECTRUM
    if (status_visible) {
        // Existing emulator scanout lives in SRAM for all four machines.
        // Retire PSRAM DMA reads before flash_safe_execute changes QMI mode.
        board_display_set_rows(emulator_rows);
        retire_display_table();
    }
#endif
}
void boot_status_flash_end(void) {
#if BOOT_STATUS_TRIAL && !MACHINE_C64 && !MACHINE_SPECTRUM
    if (status_visible) {
        board_display_set_rows(runtime_rows + status_active * (BOARD_DISPLAY_HEIGHT + 1));
        status_frame = board_display_frames();
    }
#endif
}
