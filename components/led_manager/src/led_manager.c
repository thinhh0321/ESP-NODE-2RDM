/**
 * @file led_manager.c
 * @brief LED Manager Implementation
 */

#include "led_manager.h"
#include "led_manager_internal.h"
#include "led_strip.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_timer.h"
#include <math.h>
#include <string.h>

static const char *TAG = "led_mgr";

static led_strip_handle_t led_strip = NULL;
static QueueHandle_t led_queue = NULL;
static TaskHandle_t led_task_handle = NULL;
static bool is_running = false;

// Current Active State
static led_event_t current_state = {0};
static led_event_t pending_restore_state = {0}; // State to restore after pulse/temp event
static bool has_restore_state = false;

// Rate limiting for pulse
static int64_t last_pulse_time = 0;

static void ws2812_set_color(rgb_color_t color) {
    if (led_strip) {
        led_strip_set_pixel(led_strip, 0, color.r, color.g, color.b);
        led_strip_refresh(led_strip);
    }
}

static void led_task(void *pvParameters) {
    led_event_t new_event;
    uint32_t blink_state = 0;
    int64_t last_update_time = 0;
    const int task_period_ms = 10; 

    ESP_LOGI(TAG, "LED Task Started");

    while (is_running) {
        // 1. Process Queue
        if (xQueueReceive(led_queue, &new_event, 0) == pdTRUE) {
            // Priority Check
            bool accept = false;
            
            // Logic:
            // - If new priority < current priority (numerically lower = higher importance), ACCEPT
            // - If new priority == current priority, OVERWRITE
            // - If new priority > current priority, IGNORE (unless current is temporary/expired)
            
            // Special handling for PULSE (Temporary)
            if (new_event.behavior == LED_BEHAVIOR_PULSE) {
                if (new_event.priority <= current_state.priority) {
                    if (!has_restore_state) {
                        // Save current state as restore state if we are interrupting a steady state
                        pending_restore_state = current_state;
                        has_restore_state = true;
                    }
                    current_state = new_event;
                    current_state.start_time = esp_timer_get_time() / 1000;
                    accept = true;
                }
            } else {
                // Persistent State Change
                if (new_event.priority <= current_state.priority || 
                    current_state.behavior == LED_BEHAVIOR_PULSE) { // Always override pulse
                    current_state = new_event;
                    has_restore_state = false; // New steady state clears restore
                    accept = true;
                }
            }

            if (accept) {
                // ESP_LOGD(TAG, "New State Accepted: %d (Prio %d)", current_state.state, current_state.priority);
            }
        }

        // 2. Check Expiration (for Pulse/Temp events)
        int64_t now_ms = esp_timer_get_time() / 1000;
        
        if (current_state.duration_ms > 0) {
            if (now_ms - current_state.start_time >= current_state.duration_ms) {
                // Expired
                if (has_restore_state) {
                    current_state = pending_restore_state;
                    has_restore_state = false;
                } else {
                    // Turn off if no restore state (shouldn't happen often)
                    memset(&current_state, 0, sizeof(led_event_t));
                    current_state.priority = LED_PRIORITY_LOWEST;
                }
            }
        }

        // 3. Render LED based on behavior
        switch (current_state.behavior) {
            case LED_BEHAVIOR_STATIC:
                ws2812_set_color(current_state.color);
                break;

            case LED_BEHAVIOR_SLOW_BLINK:
            case LED_BEHAVIOR_FAST_BLINK: {
                uint32_t period = (current_state.behavior == LED_BEHAVIOR_SLOW_BLINK) ? 
                                  LED_SLOW_BLINK_PERIOD_MS : LED_FAST_BLINK_PERIOD_MS;
                
                if (now_ms - last_update_time >= period / 2) {
                    blink_state = !blink_state;
                    last_update_time = now_ms;
                    
                    if (blink_state) {
                        ws2812_set_color(current_state.color);
                    } else {
                        ws2812_set_color((rgb_color_t){0, 0, 0});
                    }
                }
                break;
            }

            case LED_BEHAVIOR_PULSE:
                ws2812_set_color(current_state.color);
                break;

            case LED_BEHAVIOR_BREATH: {
                float time_ratio = (float)(now_ms % LED_BREATH_PERIOD_MS) / LED_BREATH_PERIOD_MS;
                float brightness = (sinf(2 * M_PI * time_ratio) + 1.0f) / 2.0f;
                rgb_color_t c;
                c.r = (uint8_t)(current_state.color.r * brightness);
                c.g = (uint8_t)(current_state.color.g * brightness);
                c.b = (uint8_t)(current_state.color.b * brightness);
                ws2812_set_color(c);
                break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(task_period_ms));
    }
    
    vTaskDelete(NULL);
}

esp_err_t led_manager_init(void) {
    ESP_LOGI(TAG, "Initializing LED Manager...");

    // 1. RMT Config
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_GPIO_PIN,
        .max_leds = LED_STRIP_NUM_PIXELS,
    };
    
    led_strip_rmt_config_t rmt_config = {
        .resolution_hz = LED_RMT_RESOLUTION_HZ,
        .flags.with_dma = false,
    };

    esp_err_t ret = led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to create LED strip: %s", esp_err_to_name(ret));
        return ret;
    }

    led_strip_clear(led_strip);

    // 2. Queue
    led_queue = xQueueCreate(10, sizeof(led_event_t));
    if (!led_queue) {
        return ESP_ERR_NO_MEM;
    }

    // ============================================================
    // Khởi tạo trạng thái mặc định với ưu tiên thấp nhất
    // ============================================================
    memset(&current_state, 0, sizeof(led_event_t));
    current_state.priority = LED_PRIORITY_LOWEST; // Giá trị là 255
    // ============================================================

    // 3. Task
    is_running = true;
    BaseType_t res = xTaskCreatePinnedToCore(led_task, "led_task", 2048, NULL, 3, &led_task_handle, 1);
    if (res != pdPASS) {
        ESP_LOGE(TAG, "Failed to create task");
        return ESP_FAIL;
    }

    // Default State: Boot
    led_manager_set_state(LED_STATE_BOOT);

    return ESP_OK;
}

esp_err_t led_manager_deinit(void) {
    if (is_running) {
        is_running = false;
        // Wait for task to exit is tricky without event groups, but mostly we just let it die
    }
    if (led_strip) {
        led_strip_del(led_strip);
        led_strip = NULL;
    }
    return ESP_OK;
}

esp_err_t led_manager_set_state(led_state_t state) {
    if (!led_queue) return ESP_ERR_INVALID_STATE;

    led_event_t event;
    led_state_get_preset(state, &event);
    
    // Non-blocking send from ISR context is safer if we ever call from ISR
    xQueueSend(led_queue, &event, 0);
    return ESP_OK;
}

esp_err_t led_manager_set_custom(rgb_color_t color, led_behavior_t behavior, uint32_t duration_ms, uint8_t priority) {
    if (!led_queue) return ESP_ERR_INVALID_STATE;

    led_event_t event = {
        .state = LED_STATE_MAX, // Custom
        .color = color,
        .behavior = behavior,
        .duration_ms = duration_ms,
        .priority = priority
    };

    xQueueSend(led_queue, &event, 0);
    return ESP_OK;
}

esp_err_t led_manager_pulse(void) {
    if (!led_queue) return ESP_ERR_INVALID_STATE;

    // Rate Limit
    int64_t now = esp_timer_get_time() / 1000;
    if (now - last_pulse_time < LED_PULSE_RATE_LIMIT_MS) {
        return ESP_OK; // Skip
    }
    last_pulse_time = now;

    led_event_t event;
    led_state_get_preset(LED_STATE_RECEIVING, &event);
    xQueueSend(led_queue, &event, 0);

    return ESP_OK;
}


esp_err_t led_manager_clear(void) {
    if (!led_queue) return ESP_ERR_INVALID_STATE;
    
    led_event_t event = {
        .state = LED_STATE_MAX,
        .priority = 0,           // Ưu tiên cao nhất để chiếm quyền
        .duration_ms = 1,        // Chỉ tồn tại 1ms rồi biến mất
        .behavior = LED_BEHAVIOR_STATIC
    };
    
    return xQueueSend(led_queue, &event, 0);
}