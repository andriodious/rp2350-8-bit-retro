#pragma once
#include <stddef.h>
#include <stdbool.h>
void presto_bluetooth_start(void);
void presto_bluetooth_json(char *out, size_t size);
int presto_bluetooth_command(const char *command);

void presto_bluetooth_frame(void);
void presto_bluetooth_boot_poll(void);
const char *presto_bluetooth_boot_status(void);

void presto_bluetooth_prepare_restart(void);
bool presto_bluetooth_restart_done(void);

void presto_preferences_restore(void);
void presto_preferences_changed(void);
void presto_preferences_save(void);
bool presto_preferences_flush(void);

const char *presto_keyboard_boot_status(void);

// Small read-only views for the paused touchscreen UI. Commands still use the
// web/USB command path, including radio arbitration and persistent storage.
typedef struct {
    char address[18], pin[7];
    bool remembered, scanning, capturing, connected, connecting, reconnect;
    unsigned devices, captured_usage;
} bluetooth_setup_state_t;
typedef struct { char address[18], name[40]; } bluetooth_setup_device_t;
void presto_bluetooth_setup_state(bool keyboard, bluetooth_setup_state_t *out);
bool presto_bluetooth_setup_device(bool keyboard, unsigned index, bluetooth_setup_device_t *out);
