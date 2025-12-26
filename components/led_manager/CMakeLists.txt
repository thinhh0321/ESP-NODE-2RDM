idf_component_register(
    SRCS "src/led_manager.c" "src/led_state_machine.c"
    INCLUDE_DIRS "include"
    REQUIRES led_strip driver freertos esp_timer
)
