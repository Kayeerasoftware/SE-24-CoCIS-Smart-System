idf_component_register(
    SRCS "main.c" "emergency_mgr.c" "access_entrance.c" "room_lighting.c" "storage_manager.c" "cli_console.c"
    INCLUDE_DIRS "../include"
    REQUIRES driver vfs fatfs esp_timer esp_adc
)
