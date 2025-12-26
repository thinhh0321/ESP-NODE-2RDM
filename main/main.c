#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "storage_manager.h"
#include "config_manager.h"
#include "led_manager.h"
#include "network_manager.h"

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
    
    // Start network (auto-fallback will be implemented later)
    ESP_LOGI(TAG, "Starting network...");
    ESP_ERROR_CHECK(network_start());
    
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
        
        vTaskDelay(pdMS_TO_TICKS(10000)); // Log every 10 seconds
    }
}