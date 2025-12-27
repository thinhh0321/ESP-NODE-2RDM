idf_component_register(
    SRCS "web_server.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_server json config_manager network_manager dmx_handler artnet_receiver sacn_receiver merge_engine
)
