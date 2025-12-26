idf_component_register(
    SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES storage_manager config_manager led_manager network_manager dmx_handler artnet_receiver sacn_receiver merge_engine lwip
)