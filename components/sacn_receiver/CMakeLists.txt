idf_component_register(
    SRCS "sacn_receiver.c"
    INCLUDE_DIRS "include"
    REQUIRES lwip esp_timer esp_netif
)
