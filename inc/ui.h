#ifndef RKOLED_UI_H
#define RKOLED_UI_H

#include "types.h"
#include "config.h"

void ui_render_astra(const system_telemetry_t *t, const app_config_t *cfg);
void ui_render_classic(const system_telemetry_t *t, const app_config_t *cfg);
void ui_render_boot(const app_config_t *cfg);

#endif // RKOLED_UI_H
