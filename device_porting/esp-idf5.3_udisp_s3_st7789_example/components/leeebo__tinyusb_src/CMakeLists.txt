if(CONFIG_USING_SYNOPSYS_DWC2_DRIVER)
set(DCD_DRIVER_SRC synopsys/dwc2/dcd_dwc2.c)
else() # CONFIG_USING_ESP32_DCD_DRIVER
if(IDF_TARGET STREQUAL "esp32p4")
# we only support DWC2 for esp32p4
message(WARNING "Only DWC2 driver is supported for esp32p4")
set(DCD_DRIVER_SRC synopsys/dwc2/dcd_dwc2.c)
else() # esp32s2, esp32s3
set(DCD_DRIVER_SRC espressif/esp32sx/dcd_esp32sx.c)
endif() # esp32p4
endif() # CONFIG_USING_SYNOPSYS_DWC2_DRIVER
set(DCD_CORE_SRCS
  "${CMAKE_CURRENT_SOURCE_DIR}/src/tusb.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/common/tusb_fifo.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/device/usbd.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/device/usbd_control.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/src/portable/${DCD_DRIVER_SRC}"
)
set(DCD_CORE_INCLUDES
  "${CMAKE_CURRENT_SOURCE_DIR}"
  "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

set(TUSB_COMPILE_OPTIONS 
  "-Wno-format"
  "-DCFG_TUSB_OS=OPT_OS_FREERTOS"
)

if(IDF_TARGET STREQUAL "esp32s2")
  list(APPEND TUSB_COMPILE_OPTIONS "-DCFG_TUSB_MCU=OPT_MCU_ESP32S2")
elseif(IDF_TARGET STREQUAL "esp32s3")
  list(APPEND TUSB_COMPILE_OPTIONS "-DCFG_TUSB_MCU=OPT_MCU_ESP32S3")
elseif(IDF_TARGET STREQUAL "esp32p4")
  list(APPEND TUSB_COMPILE_OPTIONS "-DCFG_TUSB_MCU=OPT_MCU_ESP32P4")
else()
  message(FATAL_ERROR "${IDF_TARGET} not supported by tinyusb")
endif()

if(CONFIG_TINYUSB_SOURCE_CODE_ONLY)
set(SRCS "")
set(INCLUDES "")
else()
set(SRCS ${DCD_CORE_SRCS})
set(INCLUDES ${DCD_CORE_INCLUDES})
endif()

idf_component_register(SRCS ${SRCS}
                    INCLUDE_DIRS ${INCLUDES}
                    REQUIRES freertos soc)

idf_component_set_property(${COMPONENT_NAME} DCD_CORE_INCLUDES "${DCD_CORE_INCLUDES}")
idf_component_set_property(${COMPONENT_NAME} DCD_CORE_SRCS "${DCD_CORE_SRCS}")
idf_component_set_property(${COMPONENT_NAME} TUSB_COMPILE_OPTIONS "${TUSB_COMPILE_OPTIONS}")

if(NOT CONFIG_TINYUSB_SOURCE_CODE_ONLY)
# build all class drivers
target_sources(${COMPONENT_LIB} PRIVATE
  "src/class/audio/audio_device.c"
  "src/class/bth/bth_device.c"
  "src/class/cdc/cdc_device.c"
  "src/class/dfu/dfu_device.c"
  "src/class/dfu/dfu_rt_device.c"
  "src/class/hid/hid_device.c"
  "src/class/midi/midi_device.c"
  "src/class/msc/msc_device.c"
  "src/class/net/ncm_device.c"
  "src/class/net/ecm_rndis_device.c"
  "src/class/usbtmc/usbtmc_device.c"
  "src/class/vendor/vendor_device.c"
  "src/class/video/video_device.c"
)

target_compile_options(${COMPONENT_LIB} PUBLIC
  ${TUSB_COMPILE_OPTIONS}
)

endif() # CONFIG_TINYUSB_SOURCE_CODE_ONLY 