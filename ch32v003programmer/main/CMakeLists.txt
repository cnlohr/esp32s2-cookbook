idf_component_register(SRCS "main.c" "advanced_usb_control.c" "programmer.c"
                    INCLUDE_DIRS ""
                    REQUIRES driver esp_tinyusb tinyusb spi_flash ulp efuse)
                    

