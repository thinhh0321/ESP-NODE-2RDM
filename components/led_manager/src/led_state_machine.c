/**
 * @file led_state_machine.c
 * @brief State Machine Logic for LED Manager
 */

#include "led_manager_internal.h"
#include <string.h>

void led_state_get_preset(led_state_t state, led_event_t *event)
{
    memset(event, 0, sizeof(led_event_t));
    event->state = state;
    event->duration_ms = 0; // Default: Infinite

    switch (state) {
        case LED_STATE_BOOT:
            // Light Blue (Static)
            event->color = (rgb_color_t){0, 50, 100};
            event->behavior = LED_BEHAVIOR_STATIC;
            event->priority = LED_PRIORITY_BOOT;
            break;

        case LED_STATE_ETHERNET_OK:
            // Green (Static)
            event->color = (rgb_color_t){0, 255, 0};
            event->behavior = LED_BEHAVIOR_STATIC;
            event->priority = LED_PRIORITY_NETWORK;
            break;

        case LED_STATE_WIFI_STA_OK:
            // Green (Slow Blink)
            event->color = (rgb_color_t){0, 200, 0};
            event->behavior = LED_BEHAVIOR_SLOW_BLINK;
            event->priority = LED_PRIORITY_NETWORK;
            break;

        case LED_STATE_WIFI_AP:
            // Purple (Static)
            event->color = (rgb_color_t){128, 0, 128};
            event->behavior = LED_BEHAVIOR_STATIC;
            event->priority = LED_PRIORITY_NETWORK;
            break;

        case LED_STATE_RECEIVING:
            // White (Pulse)
            event->color = (rgb_color_t){255, 255, 255};
            event->behavior = LED_BEHAVIOR_PULSE;
            event->duration_ms = LED_PULSE_DURATION_MS;
            event->priority = LED_PRIORITY_PULSE;
            break;

        case LED_STATE_RDM_DISCOVERY:
            // Yellow (Slow Blink)
            event->color = (rgb_color_t){255, 200, 0};
            event->behavior = LED_BEHAVIOR_SLOW_BLINK;
            event->priority = LED_PRIORITY_RDM;
            break;

        case LED_STATE_ERROR:
            // Red (Fast Blink)
            event->color = (rgb_color_t){255, 0, 0};
            event->behavior = LED_BEHAVIOR_FAST_BLINK;
            event->priority = LED_PRIORITY_ERROR;
            break;

        default:
            // Off (safety)
            event->color = (rgb_color_t){0, 0, 0};
            event->behavior = LED_BEHAVIOR_STATIC;
            event->priority = LED_PRIORITY_LOWEST;
            break;
    }
}
