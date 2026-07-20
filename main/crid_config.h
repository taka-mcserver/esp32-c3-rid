#ifndef CRID_CONFIG_H
#define CRID_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Message types (ASTM F3411 / ASD-STAN 4709-002)
#define MSG_TYPE_BASIC_ID    0x0
#define MSG_TYPE_LOCATION    0x1
#define MSG_TYPE_AUTH        0x2
#define MSG_TYPE_SELF_DESC   0x3
#define MSG_TYPE_SYSTEM      0x4

// OUI: FA:0B:BC (Intel example, replace with your own for production)
#define CRID_OUI_B0 0xFA
#define CRID_OUI_B1 0x0B
#define CRID_OUI_B2 0xBC

// GB42590-2023 vendor info type
#define CRID_VENDOR_TYPE 0x0D

// Max lengths
#define CRID_UAS_ID_MAX_LEN 20
#define CRID_SSID_MAX_LEN   32
#define CRID_SELF_ID_MAX_LEN 23

// Default Wi-Fi channel for RID broadcast (ASTM: channel 6)
#define DEFAULT_RID_CHANNEL 6

// Beacon interval in ms
#define DEFAULT_BEACON_INTERVAL_MS 250

// China C-RID configuration structure
typedef struct {
    // Identification
    char uas_id[CRID_UAS_ID_MAX_LEN + 1];
    char drone_name[CRID_SELF_ID_MAX_LEN + 1];
    uint8_t id_type;
    uint8_t ua_type;

    // Position
    float latitude;
    float longitude;
    float altitude_msl;
    float altitude_agl;
    float speed_horizontal;
    float speed_vertical;
    float heading;
    uint8_t status;

    // Operator info
    float operator_lat;
    float operator_lon;
    float operator_alt;
    char operator_id[CRID_UAS_ID_MAX_LEN + 1];
    uint8_t operator_location_type;
    uint8_t classification_type;
    uint8_t category_eu;
    uint8_t class_eu;
    uint8_t height_type;

    // Radio config
    uint8_t mac_address[6];
    char ssid[CRID_SSID_MAX_LEN + 1];
    uint8_t channel;
    uint8_t message_counter;

    // Patrol parameters
    float base_latitude;
    float base_longitude;
    float base_altitude_msl;
    float patrol_radius_lat;
    float patrol_radius_lon;
    float patrol_speed;
    float time_counter;
} cn_crid_config_t;

void crid_config_init_default(cn_crid_config_t *config);
void crid_config_update_position(cn_crid_config_t *config,
                                 float lat, float lon,
                                 float alt_msl, float alt_agl,
                                 float speed_h, float speed_v,
                                 float heading);

#ifdef __cplusplus
}
#endif

// Multi-drone simulation
#define MAX_SIM_DRONES 20
#define MAX_TRAJECTORY_POINTS 500

// KMZ trajectory data
typedef struct {
    bool active;
    double *latitudes;
    double *longitudes;
    int total_points;
    int current_index;
    float speed;
    double base_lat;
    double base_lon;
} sim_trajectory_t;

// Multi-drone control
typedef struct {
    cn_crid_config_t drones[MAX_SIM_DRONES];
    uint8_t count;
    double center_lat;
    double center_lon;
    bool running;
    sim_trajectory_t trajectory;
} sim_control_t;

// Multi-drone helpers
void crid_config_init_random(cn_crid_config_t *cfg, int index, double center_lat, double center_lon);
void crid_patrol_random_step(cn_crid_config_t *cfg);

#endif // CRID_CONFIG_H