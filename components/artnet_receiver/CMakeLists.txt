idf_component_register(
    SRCS "artnet_receiver.c"
    INCLUDE_DIRS "include"
    REQUIRES lwip config_manager esp_timer esp_netif
)
