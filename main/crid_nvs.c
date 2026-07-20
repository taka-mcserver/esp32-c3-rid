#include "crid_nvs.h"
#include <string.h>
#include <stdio.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <inttypes.h>

static const char *TAG = "CRID_NVS";
static const char *NVS_NS = "crid_cache";
static nvs_handle_t g_nvs;
static int32_t g_count = 0;

void crid_nvs_init(void) {
    esp_err_t err = nvs_open(NVS_NS, NVS_READWRITE, &g_nvs);
    if (err != ESP_OK) { ESP_LOGE(TAG, "NVS open fail: %d", err); return; }
    nvs_get_i32(g_nvs, "total_count", &g_count);
    if (g_count < 0) g_count = 0;
    ESP_LOGI(TAG, "NVS init, %" PRId32 " records", g_count);
}

static void mac_key(const uint8_t *mac, char *k, size_t sz) {
    snprintf(k, sz, "m_%02X%02X%02X%02X%02X%02X", mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
}
static void idx_key(int idx, char *k, size_t sz) {
    snprintf(k, sz, "i_%04X", idx);
}

bool crid_nvs_save_uav(const uav_track_t *uav) {
    if (!g_nvs) return false;
    char mk[32]; mac_key(uav->mac, mk, sizeof(mk));
    size_t sz = 0;
    esp_err_t err = nvs_get_blob(g_nvs, mk, NULL, &sz);
    err = nvs_set_blob(g_nvs, mk, uav, sizeof(uav_track_t));
    if (err != ESP_OK) return false;
    if (sz == 0) {
        char ik[32]; idx_key(g_count, ik, sizeof(ik));
        nvs_set_blob(g_nvs, ik, uav->mac, 6);
        g_count++;
        nvs_set_i32(g_nvs, "total_count", g_count);
    }
    nvs_commit(g_nvs);
    return true;
}

int crid_nvs_get_total_count(void) { return g_count; }

const uav_track_t *crid_nvs_get_by_index(int index) {
    if (!g_nvs || index < 0 || index >= g_count) return NULL;
    static uav_track_t r;
    char ik[32]; idx_key(index, ik, sizeof(ik));
    uint8_t mac[6]; size_t ms = 6;
    if (nvs_get_blob(g_nvs, ik, mac, &ms) != ESP_OK) return NULL;
    char mk[32]; mac_key(mac, mk, sizeof(mk));
    size_t us = sizeof(uav_track_t);
    if (nvs_get_blob(g_nvs, mk, &r, &us) == ESP_OK) return &r;
    return NULL;
}

const uav_track_t *crid_nvs_get_by_mac(const uint8_t *mac) {
    if (!g_nvs) return NULL;
    static uav_track_t r;
    char mk[32]; mac_key(mac, mk, sizeof(mk));
    size_t us = sizeof(uav_track_t);
    if (nvs_get_blob(g_nvs, mk, &r, &us) == ESP_OK) return &r;
    return NULL;
}

void crid_nvs_clear_all(void) {
    if (!g_nvs) return;
    nvs_erase_all(g_nvs);
    nvs_commit(g_nvs);
    g_count = 0;
}

bool crid_nvs_save_sim_config(const void *cfg, size_t len) {
    if (!g_nvs) return false;
    return nvs_set_blob(g_nvs, "sim_cfg", cfg, len) == ESP_OK && nvs_commit(g_nvs) == ESP_OK;
}

size_t crid_nvs_load_sim_config(void *cfg, size_t max) {
    if (!g_nvs) return 0;
    size_t len = max;
    if (nvs_get_blob(g_nvs, "sim_cfg", cfg, &len) == ESP_OK) return len;
    return 0;
}
