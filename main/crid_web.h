#ifndef CRID_WEB_H
#define CRID_WEB_H

#include "crid_config.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Initialize web server (DNS + HTTP)
 */
void crid_web_init(void);

/**
 * Start web server
 */
void crid_web_start(void);

/**
 * Get pointer to simulator config (for web API to modify)
 */
cn_crid_config_t *crid_web_get_sim_config(void);

/**
 * Check if simulator is running
 */
bool crid_web_is_sim_running(void);

/**
 * Set simulator running state
 */
void crid_web_set_sim_running(bool running);

#ifdef __cplusplus
}
#endif

#endif
