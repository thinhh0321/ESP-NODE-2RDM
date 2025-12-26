#ifndef CONFIG_MANAGER_H
#define CONFIG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

// DMX port modes
typedef enum {
    DMX_MODE_DISABLED = 0,
    DMX_MODE_OUTPUT,
    DMX_MODE_INPUT,
    DMX_MODE_RDM_MASTER,
    DMX_MODE_RDM_RESPONDER
} dmx_mode_t;

// Merge modes
typedef enum {
    MERGE_MODE_HTP = 0,
    MERGE_MODE_LTP,
    MERGE_MODE_LAST,
    MERGE_MODE_BACKUP,
    MERGE_MODE_DISABLE
} merge_mode_t;

// Protocol modes
typedef enum {
    PROTOCOL_ARTNET_ONLY = 0,
    PROTOCOL_SACN_ONLY,
    PROTOCOL_ARTNET_PRIORITY,
    PROTOCOL_SACN_PRIORITY,
    PROTOCOL_MERGE_BOTH
} protocol_mode_t;

// WiFi profile
typedef struct {
    char ssid[32];
    char password[64];
    uint8_t priority;
    bool use_static_ip;
    char static_ip[16];
    char gateway[16];
    char netmask[16];
} wifi_profile_t;

// Port configuration
typedef struct {
    dmx_mode_t mode;
    uint16_t universe_primary;
    int16_t universe_secondary;
    int16_t universe_offset;
    protocol_mode_t protocol_mode;
    merge_mode_t merge_mode;
    bool rdm_enabled;
} port_config_t;

// Main configuration
typedef struct {
    struct {
        bool use_ethernet;
        bool eth_use_static_ip;
        char eth_static_ip[16];
        char eth_gateway[16];
        char eth_netmask[16];
        wifi_profile_t wifi_profiles[5];
        uint8_t wifi_profile_count;
        char ap_ssid[32];
        char ap_password[64];
        uint8_t ap_channel;
    } network;
    
    port_config_t port1;
    port_config_t port2;
    
    struct {
        uint8_t timeout_seconds;
    } merge;
    
    struct {
        char short_name[18];
        char long_name[64];
    } node_info;
} config_t;

/**
 * @brief Initialize configuration manager
 * @return ESP_OK on success
 */
esp_err_t config_init(void);

/**
 * @brief Load configuration from storage
 * @return ESP_OK on success
 */
esp_err_t config_load(void);

/**
 * @brief Save configuration to storage
 * @return ESP_OK on success
 */
esp_err_t config_save(void);

/**
 * @brief Get current configuration
 * @return Pointer to config structure
 */
config_t* config_get(void);

/**
 * @brief Reset to default configuration
 * @return ESP_OK on success
 */
esp_err_t config_reset_to_defaults(void);

/**
 * @brief Export config as JSON string
 * @param json_str Output JSON string (caller must free)
 * @return ESP_OK on success
 */
esp_err_t config_to_json(char **json_str);

/**
 * @brief Import config from JSON string
 * @param json_str Input JSON string
 * @return ESP_OK on success
 */
esp_err_t config_from_json(const char *json_str);

#endif
