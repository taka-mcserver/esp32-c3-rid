#ifndef CRID_CONFIG_H
#define CRID_CONFIG_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// --- 鎶ユ枃绫诲瀷 (绗﹀悎 ASTM F3411 / ASD-STAN 4709-002) ---
#define MSG_TYPE_BASIC_ID    0x0  // 鍩烘湰 ID 鎶ユ枃
#define MSG_TYPE_LOCATION    0x1  // 浣嶇疆鍚戦噺鎶ユ枃
#define MSG_TYPE_AUTH        0x2  // 璁よ瘉鎶ユ枃
#define MSG_TYPE_SELF_DESC   0x3  // 杩愯鎻忚堪鎶ユ枃
#define MSG_TYPE_SYSTEM      0x4  // 绯荤粺鎶ユ枃
#define MSG_TYPE_OPERATOR_ID 0x5  // 鎿嶄綔鍛?ID 鎶ユ枃
#define MSG_TYPE_PACKED      0xF  // 鎶ユ枃鎵撳寘

// --- ID 绫诲瀷 ---
#define ID_TYPE_NONE             0
#define ID_TYPE_SERIAL_NUMBER    1
#define ID_TYPE_CAA_REGISTRATION 2
#define ID_TYPE_UTM_ASSIGNED     3
#define ID_TYPE_SPECIFIC_SESSION 4

// --- UA 绫诲瀷 ---
#define UA_TYPE_NONE             0
#define UA_TYPE_AEROPLANE        1
#define UA_TYPE_HELICOPTER       2
#define UA_TYPE_GYROPLANE        3
#define UA_TYPE_HYBRID_LIFT      4
#define UA_TYPE_ORNITHOPTER      5
#define UA_TYPE_GLIDER           6
#define UA_TYPE_KITE             7
#define UA_TYPE_FREE_BALLOON     8
#define UA_TYPE_CAPTIVE_BALLOON  9
#define UA_TYPE_AIRSHIP          10
#define UA_TYPE_FALL_AWAY        11
#define UA_TYPE_ROCKET           12
#define UA_TYPE_TETHERED         13
#define UA_TYPE_GROUND_OBSTACLE  14
#define UA_TYPE_OTHER            15

// --- 杩愯鐘舵€?(ASTM F3411) ---
#define STATUS_UNDECLARED         0
#define STATUS_GROUND             1
#define STATUS_AIRBORNE           2
#define STATUS_EMERGENCY          3
#define STATUS_REMOTE_ID_FAIL     4

// --- 楂樺害鍙傝€冪被鍨?---
#define HEIGHT_REF_OVER_TAKEOFF   0
#define HEIGHT_REF_OVER_GROUND    1

// --- 鎿嶄綔鍛樹綅缃被鍨?---
#define OP_LOC_TYPE_TAKEOFF       0
#define OP_LOC_TYPE_LIVE_GNSS     1
#define OP_LOC_TYPE_FIXED         2

// --- 鍒嗙被绫诲瀷 ---
#define CLASSIFICATION_UNDECLARED 0
#define CLASSIFICATION_EU         1

// --- 鎻忚堪绫诲瀷 ---
#define DESC_TYPE_TEXT            0
#define DESC_TYPE_EMERGENCY       1
#define DESC_TYPE_EXTENDED_STATUS 2

// --- 鎶ユ枃甯搁噺 ---
#define CRID_MESSAGE_SIZE      25     // 姣忔潯鎶ユ枃 25 瀛楄妭
#define CRID_UAS_ID_MAX_LEN    20     // UAS ID 鏈€澶ч暱搴?
#define CRID_SSID_MAX_LEN      31     // SSID 鏈€澶ч暱搴?

// --- 涓浗 C-RID 鏍囧噯 OUI 鍜岀被鍨?---
#define CRID_OUI_0 0xFA
#define CRID_OUI_1 0x0B
#define CRID_OUI_2 0xBC
#define CRID_VENDOR_TYPE 0x0D

// --- 榛樿閰嶇疆 ---
#define DEFAULT_WIFI_CHANNEL       6
#define DEFAULT_BEACON_INTERVAL_MS 1000
#define DEFAULT_MAX_FRAME_SIZE     512

// --- 閰嶇疆缁撴瀯浣?---
typedef struct {
    char uas_id[CRID_UAS_ID_MAX_LEN + 1];  // UAS ID / 鏃犱汉鏈哄敮涓€鏍囪瘑 (搴忓垪鍙凤紝濉叆 Basic ID 鎶ユ枃)
    uint8_t id_type;                         // ID 绫诲瀷 (0-4)
    uint8_t ua_type;                         // 鏃犱汉鏈虹被鍨?(0-15)
    char drone_name[CRID_UAS_ID_MAX_LEN + 1]; // 鏃犱汉鏈哄瀷鍙?(Self-ID 鎻忚堪鎶ユ枃)
    float latitude;                          // 绾害
    float longitude;                         // 缁忓害
    float altitude_msl;                      // 娴锋嫈楂樺害 (m)
    float altitude_agl;                      // 鐩稿鍦伴潰楂樺害 (m)
    float speed_horizontal;                  // 姘村钩閫熷害 (m/s)
    float speed_vertical;                    // 鍨傜洿閫熷害 (m/s)
    float heading;                           // 鑸悜 (搴?
    uint8_t status;                          // 杩愯鐘舵€?(0-4)
    float operator_lat;                      // 鎿嶄綔鍛樼含搴?
    float operator_lon;                      // 鎿嶄綔鍛樼粡搴?
    float operator_alt;                      // 鎿嶄綔鍛橀珮搴?(m)
    char operator_id[CRID_UAS_ID_MAX_LEN + 1]; // 鎿嶄綔鍛?ID
    uint8_t operator_location_type;          // 鎿嶄綔鍛樹綅缃被鍨?(0=Takeoff, 1=Live GNSS, 2=Fixed)
    uint8_t classification_type;             // 鍒嗙被绫诲瀷 (0=Undeclared, 1=EU)
    uint8_t category_eu;                     // EU 绫诲埆 (0=Undeclared, 1=Open, 2=Specific, 3=Certified)
    uint8_t class_eu;                        // EU 绛夌骇 (0-7)
    uint8_t height_type;                     // 楂樺害鍙傝€冪被鍨?(0=Takeoff, 1=Ground)
    uint8_t mac_address[6];                  // MAC 鍦板潃
    char ssid[CRID_SSID_MAX_LEN + 1];        // SSID
    uint8_t channel;                         // 閫氶亾
    uint8_t message_counter;                 // 娑堟伅璁℃暟鍣?(0-255, 寰幆)

    // 宸℃父鍙傛暟
    float base_latitude;                     // 鍩哄噯绾害
    float base_longitude;                    // 鍩哄噯缁忓害
    float base_altitude_msl;                 // 鍩哄噯娴锋嫈楂樺害 (m)
    float patrol_radius_lat;                 // 绾害鏂瑰悜宸℃父鍗婂緞
    float patrol_radius_lon;                 // 缁忓害鏂瑰悜宸℃父鍗婂緞
    float patrol_speed;                      // 宸℃父閫熷害鍙傛暟
    float time_counter;                      // 鏃堕棿璁℃暟鍣?
} cn_crid_config_t;

/**
 * @brief 鍒濆鍖栦腑鍥?C-RID 閰嶇疆涓洪粯璁ゅ€?
 * @param config 鎸囧悜閰嶇疆缁撴瀯浣撶殑鎸囬拡
 */
void crid_config_init_default(cn_crid_config_t *config);

/**
 * @brief 鏇存柊浣嶇疆鏁版嵁
 * @param config 鎸囧悜閰嶇疆缁撴瀯浣撶殑鎸囬拡
 */
void crid_config_update_position(cn_crid_config_t *config,
                                 float lat, float lon,
                                 float alt_msl, float alt_agl,
                                 float speed_h, float speed_v,
                                 float heading);

#ifdef __cplusplus
}
#endif


// --- Multi-drone simulation ---
#define MAX_SIM_DRONES 20
#define MAX_TRAJECTORY_POINTS 500

// KMZ trajectory data
typedef struct {
    bool active;
    double *latitudes;
    double *longitudes;
    int total_points;
    int current_index;
    float speed;      // m/s between points
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
