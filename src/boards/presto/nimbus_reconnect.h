#pragma once
#include <stdbool.h>
#include <stdint.h>
#define RECONNECT_LISTEN_MS 3000u
static inline uint32_t nimbus_resume_deadline(bool enabled,uint32_t now,uint32_t deadline) {
    return enabled?deadline:now+RECONNECT_LISTEN_MS;
}
static inline bool nimbus_retry_due(bool ready,bool enabled,bool busy,uint32_t now,uint32_t deadline) {
    return ready&&enabled&&!busy&&(int32_t)(now-deadline)>=0;
}
