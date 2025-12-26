idf_component_register(
    SRCS "config_manager.c"
    INCLUDE_DIRS "include"
    REQUIRES json nvs_flash storage_manager
)
