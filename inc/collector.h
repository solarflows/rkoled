#ifndef RKOLED_COLLECTOR_H
#define RKOLED_COLLECTOR_H

#include "types.h"
#include "config.h"

int collector_init(const app_config_t *cfg);
void collector_update(system_telemetry_t *t, const app_config_t *cfg);
void collector_cleanup(void);

#endif // RKOLED_COLLECTOR_H
