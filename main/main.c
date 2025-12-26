#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "storage_manager.h"
#include "config_manager.h"
#include "led_manager.h"

static const char *TAG = "main";

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
    
    // Test Sequence (Temporary for SPRINT 2 Verification)
    ESP_LOGI(TAG, "Starting LED Manager Test Sequence...");
    
    // 1. BOOT (Blue)
    led_manager_set_state(LED_STATE_BOOT);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // 2. NETWORK (Green)
    led_manager_set_state(LED_STATE_ETHERNET_OK);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // 3. WIFI STA (Green Blink)
    led_manager_set_state(LED_STATE_WIFI_STA_OK);
    vTaskDelay(pdMS_TO_TICKS(3000));

    // 4. ERROR (Red Fast Blink)
    led_manager_set_state(LED_STATE_ERROR);
    vTaskDelay(pdMS_TO_TICKS(2000));

    // 5. Pulse Test
    led_manager_set_state(LED_STATE_ETHERNET_OK); // Back to steady
    vTaskDelay(pdMS_TO_TICKS(1000));
    
    // clear test
    led_manager_clear();
    vTaskDelay(pdMS_TO_TICKS(10));
    
    for(int i=0; i<5; i++) {
        led_manager_pulse();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    
    ESP_LOGI(TAG, "System initialized successfully");
    
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}