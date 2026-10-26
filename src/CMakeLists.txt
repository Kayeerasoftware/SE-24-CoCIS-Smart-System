# CMakeLists.txt for src component in PlatformIO / ESP-IDF
# SE-24 CoCIS Smart System Multi-Target Build

set(app_sources "")

if(DEFINED ENV{PIOENV})
    if("$ENV{PIOENV}" STREQUAL "ROOM_NODE_1")
        file(GLOB app_sources "ROOM_NODE_1/*.c")
        set(include_dirs "ROOM_NODE_1")
    elseif("$ENV{PIOENV}" STREQUAL "ROOM_NODE_2")
        file(GLOB app_sources "ROOM_NODE_2/*.c")
        set(include_dirs "ROOM_NODE_2")
    else()
        file(GLOB app_sources "ENTERANCE_MASTER/*.c")
        set(include_dirs "ENTERANCE_MASTER")
    endif()
else()
    file(GLOB app_sources "ENTERANCE_MASTER/*.c")
    set(include_dirs "ENTERANCE_MASTER")
endif()

idf_component_register(
    SRCS ${app_sources}
    INCLUDE_DIRS ${include_dirs} "."
    REQUIRES common_protocol esp_wifi nvs_flash driver fatfs sdmmc vfs
)
