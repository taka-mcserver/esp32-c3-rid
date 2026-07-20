#include "crid_patrol.h"
#include "crid_config.h"
#include <math.h>
#include "esp_random.h"
#include "esp_log.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *TAG = "PATROL";

void crid_patrol_step(cn_crid_config_t *config) {
    if (!config) return;
    config->time_counter += 1.0f;
    float angle = config->time_counter * config->patrol_speed;
    float new_lat = config->base_latitude + config->patrol_radius_lat * cosf(angle);
    float new_lon = config->base_longitude + config->patrol_radius_lon * sinf(angle);
    float alt_offset = 5.0f * sinf(config->time_counter * 0.1f);
    float new_alt_msl = config->base_altitude_msl + alt_offset;
    float new_speed_h = 1.5f + 0.5f * sinf(config->time_counter * 0.1f);
    float new_speed_v = 0.5f * cosf(config->time_counter * 0.1f);
    float dlat = -config->patrol_radius_lat * sinf(angle) * config->patrol_speed;
    float dlon = config->patrol_radius_lon * cosf(angle) * config->patrol_speed;
    float new_heading = atan2f(dlon, dlat) * 180.0f / M_PI;
    if (new_heading < 0.0f) new_heading += 360.0f;
    crid_config_update_position(config, new_lat, new_lon, new_alt_msl, new_alt_msl - 5.0f, new_speed_h, new_speed_v, new_heading);
}

// Random walk within 1km of base position
void crid_patrol_random_step(cn_crid_config_t *cfg) {
    if (!cfg) return;
    cfg->time_counter += 1.0f;
    
    // Random direction change every step
    float heading_change = ((float)(esp_random() % 600) - 300.0f) / 100.0f;
    float new_heading = cfg->heading + heading_change;
    if (new_heading < 0) new_heading += 360;
    if (new_heading >= 360) new_heading -= 360;
    
    // Move in current direction at random speed variation
    float speed = cfg->speed_horizontal + ((float)(esp_random() % 40) - 20.0f) / 100.0f;
    if (speed < 0.5f) speed = 0.5f;
    if (speed > 15.0f) speed = 15.0f;
    
    float heading_rad = new_heading * M_PI / 180.0f;
    double dlat = speed * cos(heading_rad) / 111000.0;
    double dlon = speed * sin(heading_rad) / (111000.0 * cos(cfg->latitude * M_PI / 180.0));
    
    double new_lat = cfg->latitude + dlat;
    double new_lon = cfg->longitude + dlon;
    
    // Keep within 1km of base
    double dist_lat = (new_lat - cfg->base_latitude) * 111000.0;
    double dist_lon = (new_lon - cfg->base_longitude) * 111000.0 * cos(cfg->base_latitude * M_PI / 180.0);
    double dist = sqrt(dist_lat * dist_lat + dist_lon * dist_lon);
    
    if (dist > 1000.0) {
        // Bounce back towards center
        new_lat = cfg->base_latitude + (new_lat - cfg->base_latitude) * 0.5;
        new_lon = cfg->base_longitude + (new_lon - cfg->base_longitude) * 0.5;
        new_heading = atan2(cfg->base_longitude - new_lon, cfg->base_latitude - new_lat) * 180.0 / M_PI;
        if (new_heading < 0) new_heading += 360;
    }
    
    float new_alt = cfg->altitude_msl + ((float)(esp_random() % 20) - 10.0f) / 10.0f;
    if (new_alt < 10) new_alt = 10;
    if (new_alt > 500) new_alt = 500;
    
    crid_config_update_position(cfg, new_lat, new_lon, new_alt, new_alt - 5.0f, speed, 0, new_heading);
}
