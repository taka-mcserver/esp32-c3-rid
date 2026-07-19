/**
 * ESP32-C3 RID Combined Firmware
 * 
 * WiFi AP: SSID "rid", Password "12345678"
 * Captive Portal auto-redirects to management UI
 * 
 * Features:
 *   - C-RID WiFi sniffer + decoder (GB 42590, ASTM F3411)
 *   - C-RID simulated transmitter
 *   - NVS caching of received drone data
 *   - Serial debug output on UART1 (GPIO4 TX)
 *   - Web management interface
 */

#include <stdio.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_system.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"

#include "opendroneid.h"
#include "crid_rx_types.h"
#include "crid_sniffer.h"
#include "crid_parser.h"
#include "crid_tracker.h"
#include "crid_json.h"
#include "crid_web.h"
#include "crid_nvs.h"
#include "crid_serial.h"

// Simulator
#include "crid_config.h"
#include "crid_messages.h"
#include "crid_patrol.h"
#include "crid_wifi.h"

static const char *TAG = "APP";

/* ================================================================
 * WiFi AP Config
 * ================================================================ */
#define AP_SSID     "rid"
#define AP_PASSWORD "12345678"
#define AP_CHANNEL  6

/* ================================================================
 * Simulator State
 * ================================================================ */
static cn_crid_config_t g_sim_config;
static uint8_t g_beacon_frame[1024];
static uint16_t g_beacon_frame_len = 0;

static void wifi_init_ap(void) {
    esp_netif_create_default_wifi_ap();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Set AP+NULL mode so both AP and promiscuous sniffing work
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .password = AP_PASSWORD,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA2_PSK,
            .channel = AP_CHANNEL,
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_set_channel(AP_CHANNEL, WIFI_SECOND_CHAN_NONE));
    
    ESP_LOGI(TAG, "WiFi AP started: SSID=%s CH=%d", AP_SSID, AP_CHANNEL);
}

/* ================================================================
 * Tasks
 * ================================================================ */

static void sniffer_task(void *pv) {
    crid_sniffer_init();
    QueueHandle_t q = crid_sniffer_get_queue();
    SemaphoreHandle_t mtx = crid_tracker_get_mutex();
    sniffer_msg_t msg;
    uint32_t last_cleanup = 0;

    while (1) {
        if (xQueueReceive(q, &msg, pdMS_TO_TICKS(1000)) != pdTRUE) continue;
        if (msg.msg_type != MSG_TYPE_RID) continue;

        if (xSemaphoreTake(mtx, pdMS_TO_TICKS(100)) != pdTRUE) continue;

        uav_track_t *uav = crid_tracker_find_or_create(msg.src_mac);
        if (!uav) {
            uint32_t now = esp_log_timestamp();
            if (now - last_cleanup >= 10000) {
                crid_tracker_cleanup(UAV_TIMEOUT_MS);
                last_cleanup = now;
                uav = crid_tracker_find_or_create(msg.src_mac);
            }
            if (!uav) { xSemaphoreGive(mtx); continue; }
        }

        bool was_new = (uav->msg_count == 0);
        uav->last_rssi = msg.rssi;
        uav->last_channel = msg.channel;
        memcpy(uav->oui, msg.oui, 3);
        uav->oui_type = msg.oui_type;
        uav->transport = (uint8_t)GET_RID_TRANSPORT(msg.oui[0], msg.oui[1], msg.oui[2]);

        rid_protocol_t proto = crid_parser_decode(uav, msg.data, msg.data_len);
        if (proto != RID_PROTOCOL_UNKNOWN) uav->protocol = (uint8_t)proto;

        crid_parser_extract_layered(uav);
        xSemaphoreGive(mtx);

        if (was_new && uav->basic_id.valid) {
            json_uav_discovery(uav);
            crid_nvs_save_uav(uav);
        }
        if (uav->basic_id.valid) {
            json_uav_update(uav);
        }
    }
}

static void simulator_task(void *pv) {
    crid_wifi_init(AP_CHANNEL);

    TickType_t lastBeacon = xTaskGetTickCount();
    TickType_t lastPatrol = xTaskGetTickCount();
    const TickType_t bInt = pdMS_TO_TICKS(1000);
    const TickType_t pInt = pdMS_TO_TICKS(10000);

    while (1) {
        vTaskDelayUntil(&lastBeacon, bInt);
        if (!crid_web_is_sim_running()) continue;

        TickType_t now = xTaskGetTickCount();
        if ((now - lastPatrol) >= pInt) {
            crid_patrol_step(crid_web_get_sim_config());
            lastPatrol = now;
        }

        cn_crid_config_t *cfg = crid_web_get_sim_config();
        if (crid_build_beacon_frame(cfg, g_beacon_frame, sizeof(g_beacon_frame), &g_beacon_frame_len)) {
            crid_wifi_send_raw_frame(g_beacon_frame, g_beacon_frame_len);
        }

        // Serial debug
        char dbg[256];
        snprintf(dbg, sizeof(dbg), "{\"evt\":\"sim\",\"id\":\"%s\",\"lat\":%.6f,\"lon\":%.6f,\"alt\":%.1f}\n",
            cfg->uas_id, cfg->latitude, cfg->longitude, cfg->altitude_msl);
        crid_serial_write(dbg, strlen(dbg));
    }
}

static void monitor_task(void *pv) {
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        int online = crid_tracker_get_active_count();
        int total = crid_nvs_get_total_count();
        char buf[256];
        snprintf(buf, sizeof(buf),
            "{\"evt\":\"status\",\"ts\":%lu,\"online\":%d,\"total\":%d,\"sim\":%s,\"heap\":%lu}\n",
            esp_log_timestamp(), online, total,
            crid_web_is_sim_running() ? "true" : "false",
            esp_get_free_heap_size());
        crid_serial_write(buf, strlen(buf));
        ESP_LOGI(TAG, "Status: online=%d total=%d sim=%s heap=%lu",
            online, total, crid_web_is_sim_running() ? "ON" : "OFF", esp_get_free_heap_size());
    }
}

/* ================================================================
 * Main
 * ================================================================ */
void app_main(void) {
    ESP_LOGI(TAG, "=== ESP32-C3 RID Scanner + Simulator ===");
    ESP_LOGI(TAG, "WiFi AP: %s / %s", AP_SSID, AP_PASSWORD);
    ESP_LOGI(TAG, "Target: ESP32-C3 SuperMini");

    // Init NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    // Init TCP/IP
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Init WiFi AP (shared by sniffer + web + transmitter)
    wifi_init_ap();

    // Init serial debug
    crid_serial_init();

    // Init tracker + NVS
    crid_tracker_init();
    crid_nvs_init();

    // Init web server
    crid_web_init();

    // Init simulator config
    crid_config_init_default(crid_web_get_sim_config());
    crid_nvs_load_sim_config(crid_web_get_sim_config(), sizeof(cn_crid_config_t));

    // Create tasks
    xTaskCreate(sniffer_task, "sniffer", 4096, NULL, 5, NULL);
    xTaskCreate(simulator_task, "sim", 4096, NULL, 4, NULL);
    xTaskCreate(monitor_task, "monitor", 2048, NULL, 2, NULL);

    // Start web server on calling task (blocking)
    crid_web_start();
}
