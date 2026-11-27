// Standalone USB serial diagnostic for the retro8bit_FPGA R2 board.
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hardware/i2c.h"
#include "pico/bootrom.h"
#include "pico/stdlib.h"

enum { ADDRESS = 0x32, SDA_PIN = PICO_DEFAULT_I2C_SDA_PIN, SCL_PIN = PICO_DEFAULT_I2C_SCL_PIN, TIMEOUT_US = 10000 };
enum { HALT = 0x00, ARM_M = 0x10, ARM_T = 0x11 };

static bool get_status(uint8_t status[8]) {
    int count = i2c_read_timeout_us(i2c0, ADDRESS, status, 8, false, TIMEOUT_US);
    if (count != 8) {
        printf("Qwiic read failed (%d)\n", count);
        return false;
    }
    if (status[0] != 0xb8) {
        printf("Wrong bridge signature: %02x\n", status[0]);
        return false;
    }
    return true;
}

static void show_status(const uint8_t s[8]) {
    unsigned flags = s[2];
    printf("FPGA B8  strap=%u m-power=%u t-power=%u m-on=%u t-on=%u "
           "ready=%u busy=%u m-IRQ=%u m-NMI=%u m-nOE-active=%u "
           "seq=%u result=%u data=%02x\n",
           (flags >> 3) & 1, (flags >> 1) & 1, (flags >> 2) & 1,
           (flags >> 4) & 1, (flags >> 5) & 1, (flags >> 6) & 1, flags >> 7,
           s[1] & 1, (s[1] >> 1) & 1, (s[1] >> 2) & 1, s[5], s[3], s[4]);
}

static void issue(uint8_t op, uint8_t addr, uint8_t data) {
    uint8_t before[8], after[8];
    if (!get_status(before)) return;
    if ((before[2] & 0x80) && op != HALT) {
        puts("Bridge busy");
        return;
    }
    uint8_t packet[] = {op, addr, data};
    int sent = i2c_write_timeout_us(i2c0, ADDRESS, packet, sizeof packet, false, TIMEOUT_US);
    if (sent != (int)sizeof packet) {
        printf("Qwiic write failed (%d)\n", sent);
        return;
    }
    absolute_time_t deadline = make_timeout_time_ms(50);
    do {
        if (!get_status(after)) return;
        bool resetting = (op == ARM_M || op == ARM_T) && after[3] == 0 &&
                         !(after[2] & 0x40);
        if (after[5] != before[5] && !resetting) {
            show_status(after);
            return;
        }
        sleep_ms(1);
    } while (!time_reached(deadline));
    puts("Command timed out. Check status before retrying.");
}

static bool parse_byte(const char *token, unsigned max, uint8_t *value) {
    if (!token || !*token || *token == '-') return false;
    char *end;
    unsigned long n = strtoul(token, &end, 16);
    if (*end || n > max) return false;
    *value = (uint8_t)n;
    return true;
}

static void command_line(char *line) {
    static const struct { const char *name; uint8_t op; bool writing; bool tube; } bus[] = {
        {"fcr",0x20,false,false}, {"fcw",0x21,true,false},
        {"fdr",0x22,false,false}, {"fdw",0x23,true,false},
        {"tr",0x30,false,true}, {"tw",0x31,true,true},
    };
    char *op = strtok(line, " \t");
    if (!op) return;
    char *arg = strtok(NULL, " \t");
    char *data = strtok(NULL, " \t");
    if (!strtok(NULL, " \t")) {
        if (!arg && !strcmp(op,"status")) {
            uint8_t status[8];
            if (get_status(status)) show_status(status);
            return;
        }
        if (!arg && !strcmp(op,"halt")) { issue(HALT,0,0); return; }
        if (!arg && !strcmp(op,"bootsel")) {
            issue(HALT,0,0);
            reset_usb_boot(0,0);
            return;
        }
        if (arg && !data && !strcmp(op,"arm")) {
            if (!strcmp(arg,"mhz")) { issue(ARM_M,0,0); return; }
            if (!strcmp(arg,"tube")) { issue(ARM_T,0,0); return; }
        }
        for (unsigned i = 0; i < sizeof bus / sizeof bus[0]; ++i) {
            if (strcmp(op,bus[i].name)) continue;
            uint8_t address, value = 0;
            if (parse_byte(arg,bus[i].tube ? 7 : 255,&address) &&
                (bus[i].writing ? parse_byte(data,255,&value) : !data)) {
                issue(bus[i].op,address,value);
                return;
            }
            break;
        }
    }
    puts("status | halt | arm mhz | arm tube | fcr AA | fcw AA DD | "
         "fdr AA | fdw AA DD | tr R | tw R DD | bootsel (numbers hex)");
}

int main(void) {
    stdio_init_all();
    i2c_init(i2c0, 100000);
    gpio_set_function(SDA_PIN, GPIO_FUNC_I2C);
    gpio_set_function(SCL_PIN, GPIO_FUNC_I2C);
    gpio_pull_up(SDA_PIN);
    gpio_pull_up(SCL_PIN);
    char line[64];
    size_t used = 0;
    bool overflow = false;
    while (true) {
        int ch = getchar_timeout_us(10000);
        if (ch == PICO_ERROR_TIMEOUT) continue;
        if (ch == '\r' || ch == '\n') {
            if (overflow) puts("Command too long");
            else if (used) {
                line[used] = '\0';
                command_line(line);
            }
            used = 0;
            overflow = false;
        } else if (ch == 8 || ch == 127) {
            if (used) --used;
        } else if (ch >= 32 && ch < 127) {
            if (used < sizeof line - 1) line[used++] = (char)ch;
            else overflow = true;
        }
    }
}
