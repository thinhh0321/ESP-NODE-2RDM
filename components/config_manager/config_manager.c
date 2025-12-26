#include "config_manager.h"
#include "storage_manager.h"
#include "esp_log.h"
#include "cJSON.h"
#include <string.h>

static const char *TAG = "config";
static const char *CONFIG_FILE = "config.json";

// Global configuration
static config_t g_config;

// Default configuration
static void config_set_defaults(void)
{
    memset(&g_config, 0, sizeof(config_t));
    
    // Network defaults
    g_config.network.use_ethernet = true;
    g_config.network.eth_use_static_ip = false;
    strcpy(g_config.network.eth_static_ip, "192.168.1.100");
    strcpy(g_config.network.eth_gateway, "192.168.1.1");
    strcpy(g_config.network.eth_netmask, "255.255.255.0");
    g_config.network.wifi_profile_count = 0;
    strcpy(g_config.network.ap_ssid, "ArtnetNode-0000");
    strcpy(g_config.network.ap_password, "12345678");
    g_config.network.ap_channel = 1;
    
    // Port 1 defaults
    g_config.port1.mode = DMX_MODE_OUTPUT;
    g_config.port1.universe_primary = 0;
    g_config.port1.universe_secondary = -1;
    g_config.port1.universe_offset = 0;
    g_config.port1.protocol_mode = PROTOCOL_MERGE_BOTH;
    g_config.port1.merge_mode = MERGE_MODE_HTP;
    g_config.port1.rdm_enabled = true;
    
    // Port 2 defaults
    g_config.port2.mode = DMX_MODE_OUTPUT;
    g_config.port2.universe_primary = 1;
    g_config.port2.universe_secondary = -1;
    g_config.port2.universe_offset = 0;
    g_config.port2.protocol_mode = PROTOCOL_MERGE_BOTH;
    g_config.port2.merge_mode = MERGE_MODE_HTP;
    g_config.port2.rdm_enabled = true;
    
    // Merge defaults
    g_config.merge.timeout_seconds = 3;
    
    // Node info defaults
    strcpy(g_config.node_info.short_name, "ArtNet-Node");
    strcpy(g_config.node_info.long_name, "Art-Net/sACN to DMX512/RDM Converter");
}

esp_err_t config_init(void)
{
    config_set_defaults();
    ESP_LOGI(TAG, "Configuration initialized with defaults");
    return ESP_OK;
}

esp_err_t config_load(void)
{
    if (!storage_file_exists(CONFIG_FILE)) {
        ESP_LOGW(TAG, "Config file not found, using defaults");
        return config_save(); // Create default config file
    }
    
    char buffer[4096];
    size_t size = sizeof(buffer);
    
    esp_err_t ret = storage_read_file(CONFIG_FILE, buffer, &size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read config file");
        return ret;
    }
    
    ret = config_from_json(buffer);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to parse config, using defaults");
        config_set_defaults();
        return ret;
    }
    
    ESP_LOGI(TAG, "Config loaded from storage");
    return ESP_OK;
}

esp_err_t config_save(void)
{
    char *json_str = NULL;
    esp_err_t ret = config_to_json(&json_str);
    if (ret != ESP_OK || json_str == NULL) {
        ESP_LOGE(TAG, "Failed to serialize config");
        return ESP_FAIL;
    }
    
    ret = storage_write_file(CONFIG_FILE, json_str, strlen(json_str));
    free(json_str);
    
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Config saved to storage");
    }
    
    return ret;
}

config_t* config_get(void)
{
    return &g_config;
}

esp_err_t config_reset_to_defaults(void)
{
    config_set_defaults();
    return config_save();
}

esp_err_t config_to_json(char **json_str)
{
    cJSON *root = cJSON_CreateObject();
    if (root == NULL) {
        return ESP_FAIL;
    }
    
    // Network
    cJSON *network = cJSON_CreateObject();
    cJSON_AddBoolToObject(network, "use_ethernet", g_config.network.use_ethernet);
    cJSON_AddBoolToObject(network, "eth_use_static_ip", g_config.network.eth_use_static_ip);
    cJSON_AddStringToObject(network, "eth_static_ip", g_config.network.eth_static_ip);
    cJSON_AddStringToObject(network, "eth_gateway", g_config.network.eth_gateway);
    cJSON_AddStringToObject(network, "eth_netmask", g_config.network.eth_netmask);
    cJSON_AddStringToObject(network, "ap_ssid", g_config.network.ap_ssid);
    cJSON_AddStringToObject(network, "ap_password", g_config.network.ap_password);
    cJSON_AddNumberToObject(network, "ap_channel", g_config.network.ap_channel);
    
    // WiFi profiles
    cJSON *profiles = cJSON_CreateArray();
    for (int i = 0; i < g_config.network.wifi_profile_count; i++) {
        cJSON *profile = cJSON_CreateObject();
        cJSON_AddStringToObject(profile, "ssid", g_config.network.wifi_profiles[i].ssid);
        cJSON_AddStringToObject(profile, "password", g_config.network.wifi_profiles[i].password);
        cJSON_AddNumberToObject(profile, "priority", g_config.network.wifi_profiles[i].priority);
        cJSON_AddBoolToObject(profile, "use_static_ip", g_config.network.wifi_profiles[i].use_static_ip);
        cJSON_AddStringToObject(profile, "static_ip", g_config.network.wifi_profiles[i].static_ip);
        cJSON_AddStringToObject(profile, "gateway", g_config.network.wifi_profiles[i].gateway);
        cJSON_AddStringToObject(profile, "netmask", g_config.network.wifi_profiles[i].netmask);
        cJSON_AddItemToArray(profiles, profile);
    }
    cJSON_AddItemToObject(network, "wifi_profiles", profiles);
    cJSON_AddItemToObject(root, "network", network);
    
    // Port 1
    cJSON *port1 = cJSON_CreateObject();
    cJSON_AddNumberToObject(port1, "mode", g_config.port1.mode);
    cJSON_AddNumberToObject(port1, "universe_primary", g_config.port1.universe_primary);
    cJSON_AddNumberToObject(port1, "universe_secondary", g_config.port1.universe_secondary);
    cJSON_AddNumberToObject(port1, "universe_offset", g_config.port1.universe_offset);
    cJSON_AddNumberToObject(port1, "protocol_mode", g_config.port1.protocol_mode);
    cJSON_AddNumberToObject(port1, "merge_mode", g_config.port1.merge_mode);
    cJSON_AddBoolToObject(port1, "rdm_enabled", g_config.port1.rdm_enabled);
    cJSON_AddItemToObject(root, "port1", port1);
    
    // Port 2
    cJSON *port2 = cJSON_CreateObject();
    cJSON_AddNumberToObject(port2, "mode", g_config.port2.mode);
    cJSON_AddNumberToObject(port2, "universe_primary", g_config.port2.universe_primary);
    cJSON_AddNumberToObject(port2, "universe_secondary", g_config.port2.universe_secondary);
    cJSON_AddNumberToObject(port2, "universe_offset", g_config.port2.universe_offset);
    cJSON_AddNumberToObject(port2, "protocol_mode", g_config.port2.protocol_mode);
    cJSON_AddNumberToObject(port2, "merge_mode", g_config.port2.merge_mode);
    cJSON_AddBoolToObject(port2, "rdm_enabled", g_config.port2.rdm_enabled);
    cJSON_AddItemToObject(root, "port2", port2);
    
    // Merge
    cJSON *merge = cJSON_CreateObject();
    cJSON_AddNumberToObject(merge, "timeout_seconds", g_config.merge.timeout_seconds);
    cJSON_AddItemToObject(root, "merge", merge);
    
    // Node info
    cJSON *node_info = cJSON_CreateObject();
    cJSON_AddStringToObject(node_info, "short_name", g_config.node_info.short_name);
    cJSON_AddStringToObject(node_info, "long_name", g_config.node_info.long_name);
    cJSON_AddItemToObject(root, "node_info", node_info);
    
    *json_str = cJSON_Print(root);
    cJSON_Delete(root);
    
    return (*json_str != NULL) ? ESP_OK : ESP_FAIL;
}

esp_err_t config_from_json(const char *json_str)
{
    cJSON *root = cJSON_Parse(json_str);
    if (root == NULL) {
        ESP_LOGE(TAG, "JSON parse error");
        return ESP_FAIL;
    }
    
    // Parse network
    cJSON *network = cJSON_GetObjectItem(root, "network");
    if (network) {
        cJSON *item;
        if ((item = cJSON_GetObjectItem(network, "use_ethernet"))) {
            g_config.network.use_ethernet = cJSON_IsTrue(item);
        }
        if ((item = cJSON_GetObjectItem(network, "eth_use_static_ip"))) {
            g_config.network.eth_use_static_ip = cJSON_IsTrue(item);
        }
        if ((item = cJSON_GetObjectItem(network, "eth_static_ip"))) {
            strncpy(g_config.network.eth_static_ip, item->valuestring, sizeof(g_config.network.eth_static_ip) - 1);
        }
        if ((item = cJSON_GetObjectItem(network, "eth_gateway"))) {
            strncpy(g_config.network.eth_gateway, item->valuestring, sizeof(g_config.network.eth_gateway) - 1);
        }
        if ((item = cJSON_GetObjectItem(network, "eth_netmask"))) {
            strncpy(g_config.network.eth_netmask, item->valuestring, sizeof(g_config.network.eth_netmask) - 1);
        }
        if ((item = cJSON_GetObjectItem(network, "ap_ssid"))) {
            strncpy(g_config.network.ap_ssid, item->valuestring, sizeof(g_config.network.ap_ssid) - 1);
        }
        if ((item = cJSON_GetObjectItem(network, "ap_password"))) {
            strncpy(g_config.network.ap_password, item->valuestring, sizeof(g_config.network.ap_password) - 1);
        }
        if ((item = cJSON_GetObjectItem(network, "ap_channel"))) {
            g_config.network.ap_channel = item->valueint;
        }
        
        // Parse WiFi profiles
        cJSON *profiles = cJSON_GetObjectItem(network, "wifi_profiles");
        if (profiles && cJSON_IsArray(profiles)) {
            int count = cJSON_GetArraySize(profiles);
            g_config.network.wifi_profile_count = (count > 5) ? 5 : count;
            
            for (int i = 0; i < g_config.network.wifi_profile_count; i++) {
                cJSON *profile = cJSON_GetArrayItem(profiles, i);
                if ((item = cJSON_GetObjectItem(profile, "ssid"))) {
                    strncpy(g_config.network.wifi_profiles[i].ssid, item->valuestring, sizeof(g_config.network.wifi_profiles[i].ssid) - 1);
                }
                if ((item = cJSON_GetObjectItem(profile, "password"))) {
                    strncpy(g_config.network.wifi_profiles[i].password, item->valuestring, sizeof(g_config.network.wifi_profiles[i].password) - 1);
                }
                if ((item = cJSON_GetObjectItem(profile, "priority"))) {
                    g_config.network.wifi_profiles[i].priority = item->valueint;
                }
                if ((item = cJSON_GetObjectItem(profile, "use_static_ip"))) {
                    g_config.network.wifi_profiles[i].use_static_ip = cJSON_IsTrue(item);
                }
                if ((item = cJSON_GetObjectItem(profile, "static_ip"))) {
                    strncpy(g_config.network.wifi_profiles[i].static_ip, item->valuestring, sizeof(g_config.network.wifi_profiles[i].static_ip) - 1);
                }
                if ((item = cJSON_GetObjectItem(profile, "gateway"))) {
                    strncpy(g_config.network.wifi_profiles[i].gateway, item->valuestring, sizeof(g_config.network.wifi_profiles[i].gateway) - 1);
                }
                if ((item = cJSON_GetObjectItem(profile, "netmask"))) {
                    strncpy(g_config.network.wifi_profiles[i].netmask, item->valuestring, sizeof(g_config.network.wifi_profiles[i].netmask) - 1);
                }
            }
        }
    }
    
    // Parse port1
    cJSON *port1 = cJSON_GetObjectItem(root, "port1");
    if (port1) {
        cJSON *item;
        if ((item = cJSON_GetObjectItem(port1, "mode"))) {
            g_config.port1.mode = item->valueint;
        }
        if ((item = cJSON_GetObjectItem(port1, "universe_primary"))) {
            g_config.port1.universe_primary = item->valueint;
        }
        if ((item = cJSON_GetObjectItem(port1, "universe_secondary"))) {
            g_config.port1.universe_secondary = item->valueint;
        }
        if ((item = cJSON_GetObjectItem(port1, "universe_offset"))) {
            g_config.port1.universe_offset = item->valueint;
        }
        if ((item = cJSON_GetObjectItem(port1, "protocol_mode"))) {
            g_config.port1.protocol_mode = item->valueint;
        }
        if ((item = cJSON_GetObjectItem(port1, "merge_mode"))) {
            g_config.port1.merge_mode = item->valueint;
        }
        if ((item = cJSON_GetObjectItem(port1, "rdm_enabled"))) {
            g_config.port1.rdm_enabled = cJSON_IsTrue(item);
        }
    }
    
    // Parse port2
    cJSON *port2 = cJSON_GetObjectItem(root, "port2");
    if (port2) {
        cJSON *item;
        if ((item = cJSON_GetObjectItem(port2, "mode"))) {
            g_config.port2.mode = item->valueint;
        }
        if ((item = cJSON_GetObjectItem(port2, "universe_primary"))) {
            g_config.port2.universe_primary = item->valueint;
        }
        if ((item = cJSON_GetObjectItem(port2, "universe_secondary"))) {
            g_config.port2.universe_secondary = item->valueint;
        }
        if ((item = cJSON_GetObjectItem(port2, "universe_offset"))) {
            g_config.port2.universe_offset = item->valueint;
        }
        if ((item = cJSON_GetObjectItem(port2, "protocol_mode"))) {
            g_config.port2.protocol_mode = item->valueint;
        }
        if ((item = cJSON_GetObjectItem(port2, "merge_mode"))) {
            g_config.port2.merge_mode = item->valueint;
        }
        if ((item = cJSON_GetObjectItem(port2, "rdm_enabled"))) {
            g_config.port2.rdm_enabled = cJSON_IsTrue(item);
        }
    }
    
    // Parse merge
    cJSON *merge = cJSON_GetObjectItem(root, "merge");
    if (merge) {
        cJSON *item;
        if ((item = cJSON_GetObjectItem(merge, "timeout_seconds"))) {
            g_config.merge.timeout_seconds = item->valueint;
        }
    }
    
    // Parse node_info
    cJSON *node_info = cJSON_GetObjectItem(root, "node_info");
    if (node_info) {
        cJSON *item;
        if ((item = cJSON_GetObjectItem(node_info, "short_name"))) {
            strncpy(g_config.node_info.short_name, item->valuestring, sizeof(g_config.node_info.short_name) - 1);
        }
        if ((item = cJSON_GetObjectItem(node_info, "long_name"))) {
            strncpy(g_config.node_info.long_name, item->valuestring, sizeof(g_config.node_info.long_name) - 1);
        }
    }
    
    cJSON_Delete(root);
    return ESP_OK;
}
