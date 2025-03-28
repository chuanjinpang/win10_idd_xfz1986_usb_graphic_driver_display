idf_component_register(
     INCLUDE_DIRS "include"
)

# target_link_libraries(${COMPONENT_LIB} INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/libs/${CONFIG_IDF_TARGET}/libesp_codec.a")
# target_link_libraries(${COMPONENT_LIB} INTERFACE "-u audio_free -u audio_calloc_inner -u audio_calloc")

add_prebuilt_library(esp_codec "${CMAKE_CURRENT_SOURCE_DIR}/libs/${CONFIG_IDF_TARGET}/libesp_new_jpeg.a" REQUIRES esp_jpeg)
target_link_libraries(${COMPONENT_LIB}  "-Wl,--start-group" esp_new_jpeg"-Wl,--end-group")
