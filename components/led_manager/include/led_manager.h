/**
 * @file led_manager.h
 * @brief LED Manager Component for ESP-NODE-2RDM
 *
 * Handles WS2812 status LED with state machine and priority system.
 */

#ifndef LED_MANAGER_H
#define LED_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief LED States
 * Priority increases from BOOT (lowest) to ERROR (highest)
 */
typedef enum {
    LED_STATE_BOOT = 0,             // Light Blue (Static)
    LED_STATE_ETHERNET_OK,          // Green (Static)
    LED_STATE_WIFI_STA_OK,          // Green (Slow Blink)
    LED_STATE_WIFI_AP,              // Purple (Static)
    LED_STATE_RECEIVING,            // White (Pulse)
    LED_STATE_RDM_DISCOVERY,        // Yellow (Slow Blink)
    LED_STATE_ERROR,                // Red (Fast Blink)
    LED_STATE_MAX
} led_state_t;

/**
 * @brief LED Behavior Types
 */
typedef enum {
    LED_BEHAVIOR_STATIC = 0,        // Constant color
    LED_BEHAVIOR_SLOW_BLINK,        // 1Hz (500ms ON, 500ms OFF)
    LED_BEHAVIOR_FAST_BLINK,        // 5Hz (100ms ON, 100ms OFF)
    LED_BEHAVIOR_PULSE,             // Single shot pulse then return to previous
    LED_BEHAVIOR_BREATH             // Sinusoidal breathing effect
} led_behavior_t;

/**
 * @brief RGB Color Structure
 */
typedef struct {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} rgb_color_t;

/**
 * @brief Initialize LED Manager
 * 
 * Configures RMT channel and starts the LED task.
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_manager_init(void);

/**
 * @brief Deinitialize LED Manager
 * 
 * Stops task and frees resources.
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_manager_deinit(void);

/**
 * @brief Set LED to a predefined state
 * 
 * @param state Target state from led_state_t
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_manager_set_state(led_state_t state);

/**
 * @brief Set custom LED pattern
 * 
 * @param color RGB color
 * @param behavior Blink/Static/Pulse behavior
 * @param duration_ms Duration in ms (0 for infinite)
 * @param priority Priority level (0=Highest, 255=Lowest)
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_manager_set_custom(rgb_color_t color,
                                 led_behavior_t behavior,
                                 uint32_t duration_ms,
                                 uint8_t priority);

/**
 * @brief Trigger a data pulse (White flash)
 * 
 * Used for indicating packet reception. Rate limited to 10Hz.
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_manager_pulse(void);

/**
 * @brief Clear LED (turn off)
 * 
 * Sends a high-priority event to turn off the LED immediately.
 * 
 * @return esp_err_t ESP_OK on success
 */
esp_err_t led_manager_clear(void);
#ifdef __cplusplus
}
#endif

#endif // LED_MANAGER_H
