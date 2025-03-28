set(public_include_dirs "./include")

idf_component_register(INCLUDE_DIRS "${public_include_dirs}")

add_prebuilt_library(esp_new_jpeg "${CMAKE_CURRENT_SOURCE_DIR}/lib/${CONFIG_IDF_TARGET}/libesp_new_jpeg.a")
target_link_libraries(${COMPONENT_LIB} INTERFACE "-Wl,--start-group" esp_new_jpeg "-Wl,--end-group")
