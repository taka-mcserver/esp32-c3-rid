#ifndef CRID_WEB_H
#define CRID_WEB_H

#include <stdbool.h>
#include "crid_config.h"

#ifdef __cplusplus
extern "C" {
#endif

void crid_web_init(void);
void crid_web_start(void);
sim_control_t *crid_web_get_sim(void);
bool crid_web_is_sim_running(void);
void crid_web_set_sim_running(bool running);

// Trajectory: set points from web API
void crid_web_set_trajectory(const double *lats, const double *lons, int count, float speed);

#ifdef __cplusplus
}
#endif
#endif
