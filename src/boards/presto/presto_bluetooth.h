#pragma once
#include <stddef.h>
void presto_bluetooth_start(void);
void presto_bluetooth_json(char *out, size_t size);
int presto_bluetooth_command(const char *command);

void presto_bluetooth_frame(void);
