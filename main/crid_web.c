#include "index_html.h"
#include "crid_web.h"
#include "crid_rx_types.h"
#include "crid_tracker.h"
#include "crid_nvs.h"
#include "crid_config.h"
#include "crid_messages.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_wifi.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/netdb.h"
#include "cJSON.h"

static const char *TAG = "CRID_WEB";

// Simulator state (shared with app_main)
static sim_control_t g_sim;

sim_control_t *crid_web_get_sim(void) { return &g_sim; }
bool crid_web_is_sim_running(void) { return g_sim.running; }
void crid_web_set_sim_running(bool running) { g_sim.running = running; }

void crid_web_set_trajectory(const double *lats, const double *lons, int count, float speed) {
    if (count <= 0 || count > MAX_TRAJECTORY_POINTS) return;
    if (g_sim.trajectory.latitudes) free(g_sim.trajectory.latitudes);
    if (g_sim.trajectory.longitudes) free(g_sim.trajectory.longitudes);
    
    g_sim.trajectory.latitudes = malloc(count * sizeof(double));
    g_sim.trajectory.longitudes = malloc(count * sizeof(double));
    if (!g_sim.trajectory.latitudes || !g_sim.trajectory.longitudes) return;
    
    memcpy(g_sim.trajectory.latitudes, lats, count * sizeof(double));
    memcpy(g_sim.trajectory.longitudes, lons, count * sizeof(double));
    g_sim.trajectory.total_points = count;
    g_sim.trajectory.current_index = 0;
    g_sim.trajectory.speed = speed > 0 ? speed : 1.0f;
    g_sim.trajectory.active = true;
    g_sim.running = true;
    
    ESP_LOGI(TAG, "Trajectory set: %d points, speed=%.1f", count, speed);
}

/* ================================================================
 * Captive Portal DNS Server
 * ================================================================ */
static void dns_server_task(void *pvParameters) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "DNS socket create failed");
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(53);

    if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        ESP_LOGE(TAG, "DNS bind failed");
        close(sock);
        vTaskDelete(NULL);
        return;
    }

    // Get AP IP
    esp_netif_ip_info_t ip_info;
    esp_netif_t *ap_netif = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
    esp_netif_get_ip_info(ap_netif, &ip_info);

    ESP_LOGI(TAG, "DNS server started on port 53, AP IP: " IPSTR, IP2STR(&ip_info.ip));

    uint8_t rx_buf[512];
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    while (1) {
        int len = recvfrom(sock, rx_buf, sizeof(rx_buf), 0,
                          (struct sockaddr *)&client_addr, &client_addr_len);
        if (len < 12) continue; // DNS header minimum 12 bytes

        // Build DNS response: copy query header, set response flags
        uint8_t response[512];
        memcpy(response, rx_buf, len);
        
        // Set QR bit (response) and recursion available
        response[2] = 0x81;  // QR=1, Opcode=0, AA=0, TC=0, RD=1
        response[3] = 0x80;  // RA=1, Z=0, RCODE=0
        // Set answer count to 1
        response[6] = 0x00;
        response[7] = 0x01;

        // Add answer: pointer to name, type A, class IN, TTL 60, IP
        int ans_pos = len;
        response[ans_pos++] = 0xC0; // Name pointer
        response[ans_pos++] = 0x0C; // Offset to query name (byte 12)
        response[ans_pos++] = 0x00; // Type A (high)
        response[ans_pos++] = 0x01; // Type A (low)
        response[ans_pos++] = 0x00; // Class IN (high)
        response[ans_pos++] = 0x01; // Class IN (low)
        response[ans_pos++] = 0x00; // TTL (high)
        response[ans_pos++] = 0x00;
        response[ans_pos++] = 0x00;
        response[ans_pos++] = 0x3C; // TTL 60 (low)
        response[ans_pos++] = 0x00; // Data length (high)
        response[ans_pos++] = 0x04; // Data length (low)
        // IP address
        memcpy(&response[ans_pos], &ip_info.ip.addr, 4);
        ans_pos += 4;

        sendto(sock, response, ans_pos, 0,
               (struct sockaddr *)&client_addr, client_addr_len);
    }
}

/* ================================================================
 * Embedded Web Page
 * ================================================================ */

;

/* ================================================================
 * API Helpers
 * ================================================================ */

static void add_cors_headers(httpd_req_t *req) {
    httpd_resp_set_hdr(req, "Access-Control-Allow-Origin", "*");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Methods", "GET, POST, OPTIONS");
    httpd_resp_set_hdr(req, "Access-Control-Allow-Headers", "Content-Type");
}

// Convert MAC to string "XX:XX:XX:XX:XX:XX"
static void mac_to_str(const uint8_t *mac, char *buf, size_t sz) {
    snprintf(buf, sz, "%02X:%02X:%02X:%02X:%02X:%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Get protocol name string
static const char *protocol_name(uint8_t proto) {
    switch (proto) {
        case 1: return "GB 42590";
        case 2: return "GB 46750";
        case 3: return "ASTM F3411";
        default: return "Unknown";
    }
}

// Get status text
static const char *status_text(uint8_t status) {
    switch (status) {
        case 0: return "Undeclared";
        case 1: return "Ground";
        case 2: return "Airborne";
        case 3: return "Emergency";
        case 4: return "Failure";
        default: return "Unknown";
    }
}

static const char *id_type_text(uint8_t id_type) {
    switch (id_type) {
        case 0: return "None";
        case 1: return "Serial Number";
        case 2: return "CAA Registration";
        case 3: return "UTM ID";
        case 4: return "Specific Session";
        default: return "Unknown";
    }
}

/* ================================================================
 * API: GET /api/status
 * ================================================================ */
static esp_err_t api_status_get(httpd_req_t *req) {
    add_cors_headers(req);
    httpd_resp_set_type(req, "application/json");

    int online = crid_tracker_get_active_count();
    int total = crid_nvs_get_total_count();
    uint32_t heap = esp_get_free_heap_size();

    char buf[256];
    snprintf(buf, sizeof(buf),
        "{\"online\":%d,\"total\":%d,\"heap_kb\":%lu,\"sim_running\":%s}",
        online, total, heap / 1024, g_sim.running ? "true" : "false");
    
    httpd_resp_send(req, buf, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ================================================================
 * API: GET /api/drones - list all drones
 * ================================================================ */
static esp_err_t api_drones_get(httpd_req_t *req) {
    add_cors_headers(req);
    httpd_resp_set_type(req, "application/json");

    cJSON *root = cJSON_CreateObject();
    cJSON *list = cJSON_CreateArray();

    // First add active (online) drones
    SemaphoreHandle_t mutex = crid_tracker_get_mutex();
    if (xSemaphoreTake(mutex, pdMS_TO_TICKS(200)) == pdTRUE) {
        uav_track_t *table = crid_tracker_get_table();
        for (int i = 0; i < MAX_TRACKED_UAVS; i++) {
            if (!table[i].active) continue;
            cJSON *item = cJSON_CreateObject();
            char mac_str[20];
            mac_to_str(table[i].mac, mac_str, sizeof(mac_str));
            cJSON_AddStringToObject(item, "mac", mac_str);
            cJSON_AddBoolToObject(item, "online", true);
            cJSON_AddNumberToObject(item, "rssi", table[i].last_rssi);
            
            if (table[i].basic_id.valid && table[i].basic_id.ua_type != ODID_UATYPE_NONE) {
                cJSON_AddStringToObject(item, "id", table[i].basic_id.uas_id);
                cJSON_AddStringToObject(item, "id_type", id_type_text(table[i].basic_id.id_type));
                cJSON_AddStringToObject(item, "model", table[i].basic_id.uas_id);
            }
            if (table[i].location.valid) {
                cJSON_AddNumberToObject(item, "latitude", table[i].location.latitude);
                cJSON_AddNumberToObject(item, "longitude", table[i].location.longitude);
                cJSON_AddNumberToObject(item, "altitude_msl", table[i].location.altitude_geo);
                cJSON_AddNumberToObject(item, "altitude_agl", table[i].location.height);
                cJSON_AddNumberToObject(item, "speed_h", table[i].location.speed_horizontal);
                cJSON_AddNumberToObject(item, "speed_v", table[i].location.speed_vertical);
                cJSON_AddNumberToObject(item, "heading", table[i].location.direction);
                cJSON_AddStringToObject(item, "status_text", status_text(table[i].location.status));
            }
            if (table[i].operator_id.valid) {
                cJSON_AddStringToObject(item, "operator_id", table[i].operator_id.id);
                cJSON_AddNumberToObject(item, "operator_lat", table[i].system.operator_latitude);
                cJSON_AddNumberToObject(item, "operator_lon", table[i].system.operator_longitude);
            }
            cJSON_AddStringToObject(item, "protocol_name", protocol_name(table[i].protocol));
            cJSON_AddNumberToObject(item, "first_seen", table[i].first_seen_ms);
            cJSON_AddNumberToObject(item, "last_seen", table[i].last_seen_ms);
            cJSON_AddItemToArray(list, item);
        }
        xSemaphoreGive(mutex);
    }

    // Add cached drones
    int cached_count = crid_nvs_get_total_count();
    for (int i = 0; i < cached_count && i < 50; i++) {
        const uav_track_t *uav = crid_nvs_get_by_index(i);
        if (!uav) continue;
        
        // Skip if already in active list
        bool already = false;
        char mac1[20];
        mac_to_str(uav->mac, mac1, sizeof(mac1));
        // Simple check - just add all cached, duplicates will be visible
        if (already) continue;
        
        cJSON *item = cJSON_CreateObject();
        char mac_str[20];
        mac_to_str(uav->mac, mac_str, sizeof(mac_str));
        cJSON_AddStringToObject(item, "mac", mac_str);
        cJSON_AddBoolToObject(item, "online", false);
        cJSON_AddNumberToObject(item, "rssi", uav->last_rssi);
        
        if (uav->basic_id.valid && uav->basic_id.ua_type != ODID_UATYPE_NONE) {
            cJSON_AddStringToObject(item, "id", uav->basic_id.uas_id);
            cJSON_AddStringToObject(item, "model", uav->basic_id.uas_id);
        }
        if (uav->location.valid) {
            cJSON_AddNumberToObject(item, "latitude", uav->location.latitude);
            cJSON_AddNumberToObject(item, "longitude", uav->location.longitude);
            cJSON_AddNumberToObject(item, "altitude_msl", uav->location.altitude_geo);
            cJSON_AddNumberToObject(item, "altitude_agl", uav->location.height);
            cJSON_AddNumberToObject(item, "speed_h", uav->location.speed_horizontal);
            cJSON_AddNumberToObject(item, "speed_v", uav->location.speed_vertical);
            cJSON_AddNumberToObject(item, "heading", uav->location.direction);
            cJSON_AddStringToObject(item, "status_text", status_text(uav->location.status));
        }
        if (uav->operator_id.valid) {
            cJSON_AddStringToObject(item, "operator_id", uav->operator_id.id);
            cJSON_AddNumberToObject(item, "operator_lat", uav->system.operator_latitude);
            cJSON_AddNumberToObject(item, "operator_lon", uav->system.operator_longitude);
        }
        cJSON_AddStringToObject(item, "protocol_name", protocol_name(uav->protocol));
        cJSON_AddNumberToObject(item, "first_seen", uav->first_seen_ms);
        cJSON_AddNumberToObject(item, "last_seen", uav->last_seen_ms);
        cJSON_AddItemToArray(list, item);
    }

    cJSON_AddItemToObject(root, "list", list);
    
    char *json_str = cJSON_PrintUnformatted(root);
    httpd_resp_send(req, json_str, HTTPD_RESP_USE_STRLEN);
    free(json_str);
    cJSON_Delete(root);
    
    return ESP_OK;
}

/* ================================================================
 * API: POST /api/sim_toggle
 * ================================================================ */
static esp_err_t api_sim_toggle_post(httpd_req_t *req) {
    add_cors_headers(req);
    httpd_resp_set_type(req, "application/json");

    char buf[128];
    int received = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (received > 0) {
        buf[received] = 0;
        cJSON *json = cJSON_Parse(buf);
        if (json) {
            cJSON *running = cJSON_GetObjectItem(json, "running");
            if (running && cJSON_IsBool(running)) {
                g_sim.running = cJSON_IsTrue(running);
                ESP_LOGI(TAG, "Simulator %s", g_sim.running ? "ON" : "OFF");
            }
            cJSON_Delete(json);
        }
    }

    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ================================================================
 * API: GET /api/sim_config
 * ================================================================ */
static esp_err_t api_sim_config_get(httpd_req_t *req) {
    add_cors_headers(req);
    httpd_resp_set_type(req, "application/json");
    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "count", g_sim.count);
    cJSON_AddNumberToObject(root, "center_lat", g_sim.center_lat);
    cJSON_AddNumberToObject(root, "center_lon", g_sim.center_lon);
    cJSON_AddBoolToObject(root, "running", g_sim.running);
    cJSON_AddBoolToObject(root, "trajectory_active", g_sim.trajectory.active);
    cJSON_AddNumberToObject(root, "trajectory_points", g_sim.trajectory.total_points);
    cJSON_AddNumberToObject(root, "trajectory_index", g_sim.trajectory.current_index);
    
    // First drone details
    cJSON_AddStringToObject(root, "uas_id", g_sim.drones[0].uas_id);
    cJSON_AddStringToObject(root, "drone_name", g_sim.drones[0].drone_name);
    cJSON_AddNumberToObject(root, "ua_type", g_sim.drones[0].ua_type);
    cJSON_AddNumberToObject(root, "latitude", g_sim.drones[0].latitude);
    cJSON_AddNumberToObject(root, "longitude", g_sim.drones[0].longitude);
    cJSON_AddNumberToObject(root, "altitude_msl", g_sim.drones[0].altitude_msl);
    cJSON_AddNumberToObject(root, "altitude_agl", g_sim.drones[0].altitude_agl);
    cJSON_AddNumberToObject(root, "speed_horizontal", g_sim.drones[0].speed_horizontal);
    cJSON_AddNumberToObject(root, "speed_vertical", g_sim.drones[0].speed_vertical);
    cJSON_AddNumberToObject(root, "heading", g_sim.drones[0].heading);
    cJSON_AddNumberToObject(root, "status", g_sim.drones[0].status);
    cJSON_AddStringToObject(root, "operator_id", g_sim.drones[0].operator_id);
    
    char *s = cJSON_PrintUnformatted(root);
    httpd_resp_send(req, s, HTTPD_RESP_USE_STRLEN);
    free(s);
    cJSON_Delete(root);
    return ESP_OK;
}

/* ================================================================
 * API: POST /api/sim_config
 * ================================================================ */
static esp_err_t api_sim_config_post(httpd_req_t *req) {
    add_cors_headers(req);
    httpd_resp_set_type(req, "application/json");
    char buf[4096];
    int received = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (received > 0) {
        buf[received] = 0;
        cJSON *json = cJSON_Parse(buf);
        if (json) {
            cJSON *item;
            item = cJSON_GetObjectItem(json, "count");
            if (item && cJSON_IsNumber(item)) {
                int cnt = item->valueint;
                if (cnt < 1) cnt = 1;
                if (cnt > MAX_SIM_DRONES) cnt = MAX_SIM_DRONES;
                if (cnt != g_sim.count) {
                    g_sim.count = cnt;
                    for (int i = 0; i < cnt; i++) {
                        crid_config_init_random(&g_sim.drones[i], i, g_sim.center_lat, g_sim.center_lon);
                    }
                }
            }
            item = cJSON_GetObjectItem(json, "center_lat");
            if (item && cJSON_IsNumber(item)) {
                g_sim.center_lat = item->valuedouble;
                // Re-init all drones with new center
                for (int i = 0; i < g_sim.count; i++) {
                    crid_config_init_random(&g_sim.drones[i], i, g_sim.center_lat, g_sim.center_lon);
                }
            }
            item = cJSON_GetObjectItem(json, "center_lon");
            if (item && cJSON_IsNumber(item)) g_sim.center_lon = item->valuedouble;
            
            crid_nvs_save_sim_config(&g_sim, sizeof(g_sim));
            cJSON_Delete(json);
        }
    }
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ================================================================
 * API: POST /api/clear_cache
 * ================================================================ */
static esp_err_t api_clear_cache_post(httpd_req_t *req) {
    add_cors_headers(req);
    httpd_resp_set_type(req, "application/json");
    crid_nvs_clear_all();
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ================================================================
 * Captive Portal 404 Handler (Redirects unknown URIs to /)
 * ================================================================ */
static esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err) {
    httpd_resp_set_status(req, "302 Found");
    httpd_resp_set_hdr(req, "Location", "http://192.168.4.1/");
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

/* ================================================================
 * Redirect all non-API requests to index.html (captive portal)
 * ================================================================ */
static esp_err_t root_handler(httpd_req_t *req) {
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}


/* ================================================================
 * API: POST /api/trajectory - upload KMZ trajectory points
 * ================================================================ */
static esp_err_t api_trajectory_post(httpd_req_t *req) {
    add_cors_headers(req);
    httpd_resp_set_type(req, "application/json");
    
    char buf[8192];
    int received = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (received > 0) {
        buf[received] = 0;
        cJSON *json = cJSON_Parse(buf);
        if (json) {
            cJSON *pts = cJSON_GetObjectItem(json, "points");
            if (pts && cJSON_IsArray(pts)) {
                int count = cJSON_GetArraySize(pts);
                if (count > 0 && count <= MAX_TRAJECTORY_POINTS) {
                    double *lats = malloc(count * sizeof(double));
                    double *lons = malloc(count * sizeof(double));
                    if (lats && lons) {
                        for (int i = 0; i < count; i++) {
                            cJSON *pt = cJSON_GetArrayItem(pts, i);
                            if (pt) {
                                cJSON *lat = cJSON_GetObjectItem(pt, "lat");
                                cJSON *lon = cJSON_GetObjectItem(pt, "lon");
                                lats[i] = lat ? lat->valuedouble : 0;
                                lons[i] = lon ? lon->valuedouble : 0;
                            }
                        }
                        float spd = 1.0f;
                        cJSON *s = cJSON_GetObjectItem(json, "speed");
                        if (s && cJSON_IsNumber(s)) spd = (float)s->valuedouble;
                        
                        crid_web_set_trajectory(lats, lons, count, spd);
                        free(lats);
                        free(lons);
                    }
                }
            }
            cJSON_Delete(json);
        }
    }
    httpd_resp_send(req, "{\"ok\":true}", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

/* ================================================================
 * URI Handlers
 * ================================================================ */
static const httpd_uri_t uri_handlers[] = {
    {.uri = "/api/trajectory", .method = HTTP_POST, .handler = api_trajectory_post},
    {.uri = "/",              .method = HTTP_GET,  .handler = root_handler},
    {.uri = "/index.html",    .method = HTTP_GET,  .handler = root_handler},
    {.uri = "/api/status",    .method = HTTP_GET,  .handler = api_status_get},
    {.uri = "/api/drones",    .method = HTTP_GET,  .handler = api_drones_get},
    {.uri = "/api/sim_config",.method = HTTP_GET,  .handler = api_sim_config_get},
    {.uri = "/api/sim_config",.method = HTTP_POST, .handler = api_sim_config_post},
    {.uri = "/api/sim_toggle",.method = HTTP_POST, .handler = api_sim_toggle_post},
    {.uri = "/api/clear_cache",.method = HTTP_POST,.handler = api_clear_cache_post},
};

static httpd_handle_t g_server = NULL;

void crid_web_init(void) {
    memset(&g_sim, 0, sizeof(g_sim));
    g_sim.count = 1;
    g_sim.center_lat = 39.9042;
    g_sim.center_lon = 116.4074;
    crid_config_init_random(&g_sim.drones[0], 0, g_sim.center_lat, g_sim.center_lon);
    size_t loaded = crid_nvs_load_sim_config(&g_sim, sizeof(g_sim));
    if (loaded > 0 && g_sim.count > 0) {
        ESP_LOGI(TAG, "Loaded saved sim config: %d drones", g_sim.count);
    }
}

void crid_web_start(void) {
    // Start DNS server for captive portal
    xTaskCreate(dns_server_task, "dns_server", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "DNS captive portal task created");

    // Start HTTP server
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_uri_handlers = 16;
    config.stack_size = 8192;
    config.lru_purge_enable = true;
    config.recv_wait_timeout = 5;
    config.send_wait_timeout = 5;
    
    if (httpd_start(&g_server, &config) == ESP_OK) {
        httpd_register_err_handler(g_server, HTTPD_404_NOT_FOUND, http_404_error_handler);
        for (int i = 0; i < sizeof(uri_handlers) / sizeof(uri_handlers[0]); i++) {
            httpd_register_uri_handler(g_server, &uri_handlers[i]);
        }
        ESP_LOGI(TAG, "HTTP server started on port 80");
    } else {
        ESP_LOGE(TAG, "Failed to start HTTP server!");
    }
}
