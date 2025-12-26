#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "storage_manager.h"
#include "config_manager.h"
#include "led_manager.h"
#include "network_manager.h"
#include "dmx_handler.h"

static const char *TAG = "main";

// Network state callback
static void network_state_changed(network_state_t state, void *user_data)
{
    ESP_LOGI(TAG, "Network state changed: %d", state);
    
    // Update LED based on network state
    switch (state) {
        case NETWORK_ETHERNET_CONNECTED:
            led_manager_set_state(LED_STATE_ETHERNET_OK);
            break;
        case NETWORK_WIFI_STA_CONNECTED:
            led_manager_set_state(LED_STATE_WIFI_STA_OK);
            break;
        case NETWORK_WIFI_AP_ACTIVE:
            led_manager_set_state(LED_STATE_WIFI_AP);
            break;
        case NETWORK_ERROR:
            led_manager_set_state(LED_STATE_ERROR);
            break;
        case NETWORK_DISCONNECTED:
        case NETWORK_CONNECTING:
        default:
            led_manager_set_state(LED_STATE_BOOT);
            break;
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "ESP-NODE-2RDM Firmware v0.1.0");
    ESP_LOGI(TAG, "ESP-IDF Version: %s", esp_get_idf_version());
    
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    
    // Initialize storage
    ESP_ERROR_CHECK(storage_init());
    
    // Initialize and load config
    ESP_ERROR_CHECK(config_init());
    ESP_ERROR_CHECK(config_load());
    
    config_t *config = config_get();
    ESP_LOGI(TAG, "Node: %s", config->node_info.short_name);

    // Initialize LED Manager
    ESP_ERROR_CHECK(led_manager_init());
    led_manager_set_state(LED_STATE_BOOT);
    
    // Initialize Network Manager
    ESP_LOGI(TAG, "Initializing network manager...");
    ESP_ERROR_CHECK(network_init());
    
    // Register network state callback
    network_register_state_callback(network_state_changed, NULL);
    
    // Start network with auto-fallback (asynchronous)
    ESP_LOGI(TAG, "Starting network with auto-fallback...");
    ESP_ERROR_CHECK(network_start_with_fallback());
    
    // Initialize DMX Handler
    ESP_LOGI(TAG, "Initializing DMX handler...");
    ESP_ERROR_CHECK(dmx_handler_init());
    
    // Configure DMX ports based on config
    ESP_LOGI(TAG, "Configuring DMX ports...");
    ESP_ERROR_CHECK(dmx_handler_configure_port(DMX_PORT_1, &config->port1));
    ESP_ERROR_CHECK(dmx_handler_configure_port(DMX_PORT_2, &config->port2));
    
    // Start DMX ports if not disabled
    if (config->port1.mode != DMX_MODE_DISABLED) {
        ESP_LOGI(TAG, "Starting DMX port 1 in mode %d", config->port1.mode);
        ESP_ERROR_CHECK(dmx_handler_start_port(DMX_PORT_1));
    }
    
    if (config->port2.mode != DMX_MODE_DISABLED) {
        ESP_LOGI(TAG, "Starting DMX port 2 in mode %d", config->port2.mode);
        ESP_ERROR_CHECK(dmx_handler_start_port(DMX_PORT_2));
    }
    
    ESP_LOGI(TAG, "System initialized successfully");
    
    // Main loop
    while (1) {
        // Get network status periodically
        network_status_t status;
        if (network_get_status(&status) == ESP_OK) {
            if (network_is_connected()) {
                const char *ip = network_get_ip_address();
                if (ip) {
                    ESP_LOGI(TAG, "Network connected - IP: %s", ip);
                }
            }
        }
        
        // Get DMX port status
        dmx_port_status_t dmx_status;
        if (dmx_handler_get_port_status(DMX_PORT_1, &dmx_status) == ESP_OK && dmx_status.is_active) {
            ESP_LOGI(TAG, "DMX Port 1 - Mode: %d, Frames sent: %lu, Frames received: %lu",
                     dmx_status.mode, dmx_status.stats.frames_sent, dmx_status.stats.frames_received);
        }
        
        if (dmx_handler_get_port_status(DMX_PORT_2, &dmx_status) == ESP_OK && dmx_status.is_active) {
            ESP_LOGI(TAG, "DMX Port 2 - Mode: %d, Frames sent: %lu, Frames received: %lu",
                     dmx_status.mode, dmx_status.stats.frames_sent, dmx_status.stats.frames_received);
        }
        
        vTaskDelay(pdMS_TO_TICKS(10000)); // Log every 10 seconds
    }
}