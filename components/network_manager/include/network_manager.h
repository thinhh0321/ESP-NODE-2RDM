/**
 * @file network_manager.h
 * @brief Network Manager Component for ESP-NODE-2RDM
 *
 * Manages network connectivity including:
 * - Ethernet (W5500 SPI)
 * - WiFi Station mode (with multiple profiles)
 * - WiFi Access Point mode
 * - Auto-fallback mechanism
 */

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_wifi_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief IP Configuration Structure
 */
typedef struct {
    bool use_dhcp;
    char ip[16];
    char netmask[16];
    char gateway[16];
    char dns1[16];
    char dns2[16];
} ip_config_t;

/**
 * @brief WiFi Profile Structure
 */
typedef struct {
    char ssid[32];
    char password[64];
    uint8_t priority;       // 0-255, lower value = higher priority
    bool use_static_ip;
    ip_config_t static_ip;
} wifi_profile_t;

/**
 * @brief Network State
 */
typedef enum {
    NETWORK_DISCONNECTED = 0,
    NETWORK_ETHERNET_CONNECTED = 1,
    NETWORK_WIFI_STA_CONNECTED = 2,
    NETWORK_WIFI_AP_ACTIVE = 3,
    NETWORK_CONNECTING = 4,
    NETWORK_ERROR = 5
} network_state_t;

/**
 * @brief Network Status Structure
 */
typedef struct {
    network_state_t state;
    char ip_address[16];
    char netmask[16];
    char gateway[16];
    uint8_t mac[6];
    int rssi;               // WiFi only
    uint32_t packets_received;
    uint32_t packets_sent;
    uint64_t bytes_received;
    uint64_t bytes_sent;
} network_status_t;

/**
 * @brief Network State Change Callback
 */
typedef void (*network_state_callback_t)(network_state_t state, void *user_data);

// ============================================================================
// Initialization
// ============================================================================

/**
 * @brief Initialize network manager
 * 
 * Sets up network interface and event handlers.
 * Must be called before any other network functions.
 * 
 * @return ESP_OK on success
 */
esp_err_t network_init(void);

/**
 * @brief Start network connection
 * 
 * Attempts connection according to configuration:
 * 1. Try Ethernet (if enabled)
 * 2. Try WiFi STA (each profile by priority)
 * 3. Fallback to WiFi AP
 * 
 * @return ESP_OK if connection initiated successfully
 */
esp_err_t network_start(void);

/**
 * @brief Stop all network interfaces
 * 
 * @return ESP_OK on success
 */
esp_err_t network_stop(void);

// ============================================================================
// Ethernet Control
// ============================================================================

/**
 * @brief Connect to Ethernet (W5500)
 * 
 * @param config IP configuration (NULL for DHCP)
 * @return ESP_OK on success
 */
esp_err_t network_ethernet_connect(const ip_config_t *config);

/**
 * @brief Disconnect Ethernet
 * 
 * @return ESP_OK on success
 */
esp_err_t network_ethernet_disconnect(void);

/**
 * @brief Check Ethernet link status
 * 
 * @return true if link is up
 */
bool network_ethernet_is_link_up(void);

// ============================================================================
// WiFi Station Control
// ============================================================================

/**
 * @brief Connect to WiFi AP using profile
 * 
 * @param profile WiFi profile configuration
 * @return ESP_OK on success
 */
esp_err_t network_wifi_sta_connect(const wifi_profile_t *profile);

/**
 * @brief Disconnect from WiFi AP
 * 
 * @return ESP_OK on success
 */
esp_err_t network_wifi_sta_disconnect(void);

/**
 * @brief Scan for WiFi networks
 * 
 * @param scan_result Buffer to store scan results
 * @param max_aps Maximum number of APs to return
 * @return Number of APs found
 */
uint16_t network_wifi_scan(wifi_ap_record_t *scan_result, uint16_t max_aps);

// ============================================================================
// WiFi Access Point Control
// ============================================================================

/**
 * @brief Start WiFi Access Point
 * 
 * @param ssid AP SSID
 * @param password AP password (min 8 characters)
 * @param channel WiFi channel (1-13)
 * @param config IP configuration (NULL for default: 192.168.4.1)
 * @return ESP_OK on success
 */
esp_err_t network_wifi_ap_start(const char *ssid,
                                const char *password,
                                uint8_t channel,
                                const ip_config_t *config);

/**
 * @brief Stop WiFi Access Point
 * 
 * @return ESP_OK on success
 */
esp_err_t network_wifi_ap_stop(void);

/**
 * @brief Get list of connected stations (AP mode)
 * 
 * @param stations Buffer to store station info
 * @param max_stations Maximum number of stations
 * @return Number of connected stations
 */
uint16_t network_wifi_ap_get_stations(wifi_sta_info_t *stations,
                                      uint16_t max_stations);

// ============================================================================
// Status & Information
// ============================================================================

/**
 * @brief Get current network status
 * 
 * @param status Buffer to store status
 * @return ESP_OK on success
 */
esp_err_t network_get_status(network_status_t *status);

/**
 * @brief Get current IP address string
 * 
 * @return IP address string or NULL if not connected
 */
const char* network_get_ip_address(void);

/**
 * @brief Get MAC address
 * 
 * @param mac Buffer for 6-byte MAC address
 * @return ESP_OK on success
 */
esp_err_t network_get_mac_address(uint8_t mac[6]);

/**
 * @brief Check if network is connected
 * 
 * @return true if any interface is connected
 */
bool network_is_connected(void);

// ============================================================================
// Event Callbacks
// ============================================================================

/**
 * @brief Register callback for network state changes
 * 
 * @param callback Callback function
 * @param user_data User data to pass to callback
 */
void network_register_state_callback(network_state_callback_t callback,
                                    void *user_data);

#ifdef __cplusplus
}
#endif

#endif // NETWORK_MANAGER_H
