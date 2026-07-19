#include "crid_nvs.h"
#include <string.h>
#include <stdio.h>
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"

static const char *TAG = "CRID_NVS";
static const char *NVS_NAMESPACE = "crid_cache";
static nvs_handle_t g_nvs_handle;
static int g_total_count = 0;

void crid_nvs_init(void) {
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &g_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS namespace: %d", err);
        return;
    }
    
    // Read total count
    nvs_get_i32(g_nvs_handle, "total_count", &g_total_count);
    if (g_total_count < 0) g_total_count = 0;
    
    ESP_LOGI(TAG, "NVS cache initialized, %d records", g_total_count);
}

// Convert MAC to a key string: "mac_XXXXXXXXXXXX"
static void mac_to_key(const uint8_t *mac, char *key, size_t key_size) {
    snprintf(key, key_size, "mac_%02X%02X%02X%02X%02X%02X",
             mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}

// Convert index to key string: "idx_NNNN"
static void index_to_key(int index, char *key, size_t key_size) {
    snprintf(key, key_size, "idx_%04X", index);
}

bool crid_nvs_save_uav(const uav_track_t *uav) {
    if (g_nvs_handle == 0) return false;
    
    char mac_key[32];
    mac_to_key(uav->mac, mac_key, sizeof(mac_key));
    
    // Check if already exists
    size_t existing_size = sizeof(uav_track_t);
    esp_err_t err = nvs_get_blob(g_nvs_handle, mac_key, NULL, &existing_size);
    
    // Save UAV data blob
    err = nvs_set_blob(g_nvs_handle, mac_key, uav, sizeof(uav_track_t));
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "Failed to save UAV: %d", err);
        return false;
    }
    
    // If new, save index mapping and increment count
    if (existing_size == 0 || err == ESP_ERR_NVS_NOT_FOUND) {
        char idx_key[32];
        index_to_key(g_total_count, idx_key, sizeof(idx_key));
        nvs_set_blob(g_nvs_handle, idx_key, uav->mac, 6);
        g_total_count++;
        nvs_set_i32(g_nvs_handle, "total_count", g_total_count);
    }
    
    nvs_commit(g_nvs_handle);
    return true;
}

int crid_nvs_load_all(uav_track_t *buffer, int max_count) {
    if (g_nvs_handle == 0) return 0;
    
    int loaded = 0;
    for (int i = 0; i < g_total_count && loaded < max_count; i++) {
        char idx_key[32];
        index_to_key(i, idx_key, sizeof(idx_key));
        
        uint8_t mac[6];
        size_t mac_size = 6;
        esp_err_t err = nvs_get_blob(g_nvs_handle, idx_key, mac, &mac_size);
        if (err != ESP_OK) continue;
        
        char mac_key[32];
        mac_to_key(mac, mac_key, sizeof(mac_key));
        
        size_t uav_size = sizeof(uav_track_t);
        err = nvs_get_blob(g_nvs_handle, mac_key, buffer + loaded, &uav_size);
        if (err == ESP_OK) {
            loaded++;
        }
    }
    return loaded;
}

int crid_nvs_get_total_count(void) {
    return g_total_count;
}

const uav_track_t *crid_nvs_get_by_index(int index) {
    if (g_nvs_handle == 0 || index < 0 || index >= g_total_count) return NULL;
    
    static uav_track_t _result;
    
    char idx_key[32];
    index_to_key(index, idx_key, sizeof(idx_key));
    
    uint8_t mac[6];
    size_t mac_size = 6;
    if (nvs_get_blob(g_nvs_handle, idx_key, mac, &mac_size) != ESP_OK) return NULL;
    
    char mac_key[32];
    mac_to_key(mac, mac_key, sizeof(mac_key));
    
    size_t uav_size = sizeof(uav_track_t);
    if (nvs_get_blob(g_nvs_handle, mac_key, &_result, &uav_size) == ESP_OK) {
        return &_result;
    }
    return NULL;
}

const uav_track_t *crid_nvs_get_by_mac(const uint8_t *mac) {
    if (g_nvs_handle == 0) return NULL;
    
    static uav_track_t _result;
    char mac_key[32];
    mac_to_key(mac, mac_key, sizeof(mac_key));
    
    size_t uav_size = sizeof(uav_track_t);
    if (nvs_get_blob(g_nvs_handle, mac_key, &_result, &uav_size) == ESP_OK) {
        return &_result;
    }
    return NULL;
}

void crid_nvs_clear_all(void) {
    if (g_nvs_handle == 0) return;
    nvs_erase_all(g_nvs_handle);
    nvs_commit(g_nvs_handle);
    g_total_count = 0;
    ESP_LOGI(TAG, "All cached data cleared");
}

bool crid_nvs_save_sim_config(const void *config, size_t len) {
    if (g_nvs_handle == 0) return false;
    esp_err_t err = nvs_set_blob(g_nvs_handle, "sim_config", config, len);
    if (err != ESP_OK) return false;
    nvs_commit(g_nvs_handle);
    return true;
}

size_t crid_nvs_load_sim_config(void *config, size_t max_len) {
    if (g_nvs_handle == 0) return 0;
    size_t len = max_len;
    esp_err_t err = nvs_get_blob(g_nvs_handle, "sim_config", config, &len);
    if (err != ESP_OK) return 0;
    return len;
}
