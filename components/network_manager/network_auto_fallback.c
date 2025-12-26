/**
 * @file network_auto_fallback.c
 * @brief Network Auto-Fallback Implementation Example
 * 
 * This file demonstrates how to implement the auto-fallback sequence:
 * Ethernet → WiFi STA (by priority) → WiFi AP
 * 
 * This can be integrated into main.c or run as a separate task.
 */

#include "network_manager.h"
#include "config_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "network_fallback";

/**
 * @brief Network auto-fallback task
 * 
 * This task implements the auto-fallback logic:
 * 1. Try Ethernet (if enabled)
 * 2. Try WiFi STA profiles by priority
 * 3. Fallback to WiFi AP
 * 
 * @param pvParameters Task parameters (unused)
 */
void network_auto_fallback_task(void *pvParameters)
{
    config_t *config = config_get();
    bool connected = false;
    
    ESP_LOGI(TAG, "Starting network auto-fallback sequence...");
    
    // Step 1: Try Ethernet
    if (config->network.use_ethernet) {
        ESP_LOGI(TAG, "Attempting Ethernet connection...");
        
        ip_config_t ip_config = {0};
        ip_config.use_dhcp = !config->network.eth_use_static_ip;
        
        if (config->network.eth_use_static_ip) {
            strncpy(ip_config.ip, config->network.eth_static_ip, 
                   sizeof(ip_config.ip) - 1);
            strncpy(ip_config.netmask, config->network.eth_netmask, 
                   sizeof(ip_config.netmask) - 1);
            strncpy(ip_config.gateway, config->network.eth_gateway, 
                   sizeof(ip_config.gateway) - 1);
        }
        
        for (int retry = 0; retry < 3; retry++) {
            ESP_LOGI(TAG, "Ethernet attempt %d/3", retry + 1);
            
            if (network_ethernet_connect(&ip_config) == ESP_OK) {
                // Wait for connection
                vTaskDelay(pdMS_TO_TICKS(10000)); // 10 second timeout
                
                if (network_ethernet_is_link_up()) {
                    ESP_LOGI(TAG, "Ethernet connected successfully");
                    connected = true;
                    break;
                }
            }
            
            if (retry < 2) {
                ESP_LOGW(TAG, "Ethernet connection failed, retrying...");
                vTaskDelay(pdMS_TO_TICKS(2000));
            }
        }
        
        if (!connected) {
            ESP_LOGW(TAG, "Ethernet connection failed after 3 attempts");
        }
    }
    
    // Step 2: Try WiFi Station profiles (Priority 2: Only if Ethernet failed)
    // WiFi is only started when Ethernet is unavailable to free RF and RAM
    if (!connected && config->network.wifi_profile_count > 0) {
        ESP_LOGI(TAG, "Attempting WiFi Station connection...");
        ESP_LOGI(TAG, "Starting WiFi (Ethernet unavailable)...");
        
        // Sort profiles by priority (bubble sort for simplicity)
        wifi_profile_t profiles[5];
        memcpy(profiles, config->network.wifi_profiles, 
               sizeof(wifi_profile_t) * config->network.wifi_profile_count);
        
        for (int i = 0; i < config->network.wifi_profile_count - 1; i++) {
            for (int j = 0; j < config->network.wifi_profile_count - i - 1; j++) {
                if (profiles[j].priority > profiles[j + 1].priority) {
                    wifi_profile_t temp = profiles[j];
                    profiles[j] = profiles[j + 1];
                    profiles[j + 1] = temp;
                }
            }
        }
        
        // Try each profile
        for (int i = 0; i < config->network.wifi_profile_count; i++) {
            ESP_LOGI(TAG, "Trying WiFi profile: %s (priority %d)",
                    profiles[i].ssid, profiles[i].priority);
            
            if (network_wifi_sta_connect(&profiles[i]) == ESP_OK) {
                // Wait for connection
                vTaskDelay(pdMS_TO_TICKS(15000)); // 15 second timeout
                
                if (network_is_connected()) {
                    ESP_LOGI(TAG, "WiFi Station connected to: %s", profiles[i].ssid);
                    connected = true;
                    break;
                }
            }
            
            ESP_LOGW(TAG, "Failed to connect to: %s", profiles[i].ssid);
            network_wifi_sta_disconnect();
            vTaskDelay(pdMS_TO_TICKS(1000)); // Brief delay before next attempt
        }
        
        if (!connected) {
            ESP_LOGW(TAG, "WiFi Station connection failed for all profiles");
        }
    }
    
    // Step 3: Fallback to WiFi AP (Priority 3: Last resort)
    if (!connected) {
        ESP_LOGI(TAG, "Falling back to WiFi AP mode (Priority 3)...");
        ESP_LOGI(TAG, "Starting WiFi AP as fallback...");
        
        ip_config_t ap_config = {
            .use_dhcp = false,
            .ip = "192.168.4.1",
            .netmask = "255.255.255.0",
            .gateway = "192.168.4.1"
        };
        
        strncpy((char *)ap_config.ip, "192.168.4.1", sizeof(ap_config.ip) - 1);
        strncpy((char *)ap_config.netmask, "255.255.255.0", 
               sizeof(ap_config.netmask) - 1);
        strncpy((char *)ap_config.gateway, "192.168.4.1", 
               sizeof(ap_config.gateway) - 1);
        
        esp_err_t ret = network_wifi_ap_start(
            config->network.ap_ssid,
            config->network.ap_password,
            config->network.ap_channel,
            &ap_config
        );
        
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "WiFi AP started successfully");
            ESP_LOGI(TAG, "SSID: %s", config->network.ap_ssid);
            ESP_LOGI(TAG, "IP: 192.168.4.1");
            connected = true;
        } else {
            ESP_LOGE(TAG, "Failed to start WiFi AP!");
        }
    }
    
    // Task complete
    if (connected) {
        const char *ip = network_get_ip_address();
        ESP_LOGI(TAG, "Network connected - IP: %s", ip ? ip : "unknown");
    } else {
        ESP_LOGE(TAG, "All network connection attempts failed!");
    }
    
    // Delete task
    vTaskDelete(NULL);
}

/**
 * @brief Start network with auto-fallback
 * 
 * Creates a task to handle the auto-fallback sequence.
 * This function returns immediately; connection happens asynchronously.
 * 
 * @return ESP_OK if task created successfully
 */
esp_err_t network_start_with_fallback(void)
{
    BaseType_t ret = xTaskCreate(
        network_auto_fallback_task,
        "net_fallback",
        4096,
        NULL,
        5,  // Priority
        NULL
    );
    
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create auto-fallback task");
        return ESP_FAIL;
    }
    
    return ESP_OK;
}
