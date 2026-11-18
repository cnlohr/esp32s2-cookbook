idf_component_register(SRCS "main.c" "advanced_usb_control.c" "programmer.c" "rgb.c"
                    INCLUDE_DIRS "."
                    REQUIRES driver esp_tinyusb tinyusb spi_flash ulp efuse led_strip nvs_flash)

set_source_files_properties(programmer.c PROPERTIES COMPILE_FLAGS -Wno-error=unused-label)

                    

