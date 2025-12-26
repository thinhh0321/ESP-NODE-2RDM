/**
 * @file led_manager_internal.h
 * @brief Internal definitions for LED Manager
 */

#ifndef LED_MANAGER_INTERNAL_H
#define LED_MANAGER_INTERNAL_H

#include "led_manager.h"

// Hardware Configuration
#define LED_GPIO_PIN            48
#define LED_RMT_RESOLUTION_HZ   10000000 // 10MHz
#define LED_STRIP_NUM_PIXELS    1

// Timing Constants
#define LED_SLOW_BLINK_PERIOD_MS    1000
#define LED_FAST_BLINK_PERIOD_MS    200
#define LED_PULSE_DURATION_MS       50
#define LED_BREATH_PERIOD_MS        2000
#define LED_PULSE_RATE_LIMIT_MS     100

// Priority Levels (Lower value = Higher priority)
#define LED_PRIORITY_ERROR          0
#define LED_PRIORITY_RDM            1
#define LED_PRIORITY_PULSE          2
#define LED_PRIORITY_NETWORK        3
#define LED_PRIORITY_BOOT           4
#define LED_PRIORITY_LOWEST         255

// Internal Event Structure
typedef struct {
    led_state_t state;
    rgb_color_t color;
    led_behavior_t behavior;
    uint32_t duration_ms;
    uint8_t priority;
    uint32_t start_time;
} led_event_t;

/**
 * @brief Get configuration for a preset state
 * 
 * @param state Input state
 * @param event Output event structure (populated with presets)
 */
void led_state_get_preset(led_state_t state, led_event_t *event);

#endif // LED_MANAGER_INTERNAL_H
