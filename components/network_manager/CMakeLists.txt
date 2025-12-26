idf_component_register(
    SRCS "network_manager.c" "network_auto_fallback.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_netif esp_eth esp_wifi esp_event driver config_manager
)
