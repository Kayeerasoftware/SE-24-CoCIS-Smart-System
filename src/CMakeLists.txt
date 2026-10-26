# CMakeLists.txt for src component in PlatformIO / ESP-IDF
# SE-24 CoCIS Smart System Multi-Target Build

if(APP_NODE STREQUAL "ROOM_NODE_1")
   set(app_dir "ROOM_NODE_1")
elseif(APP_NODE STREQUAL "ROOM_NODE_2")
   set(app_dir "ROOM_NODE_2")
else()
   set(app_dir "ENTERANCE_MASTER")
endif()

file(GLOB app_sources "${app_dir}/*.c")

idf_component_register(
   SRCS ${app_sources}
   INCLUDE_DIRS "${app_dir}" "."
   REQUIRES common_protocol esp_wifi nvs_flash driver fatfs sdmmc vfs
)
