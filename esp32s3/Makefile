ELF:=main.elf

all : $(ELF)


PORT?=/dev/ttyUSB0

LINKER_SCRIPT:=esp32s3-ram.lds

IDF:=/home/cnlohr/esp/esp-idf

CFLAGS:=-mlongcalls -DHAVE_CONFIG_H -I$(IDF)/components/hal/include -I$(IDF)/components/hal/esp32s3/include -I$(IDF)/components/esp_wifi/include -I$(IDF)/components/esp_event/include -I$(IDF)/components/soc/include -I$(IDF)/components/soc/esp32s3/include -I$(IDF)/components/esp_common/include -I$(IDF)/components/driver/include -I$(IDF)/components/esp_hw_support/include -I$(IDF)/components/esp_hw_support/include/esp_private -I$(IDF)/components/esp_hal_regi2c/include -I$(IDF)/components/esp_rom/include -I$(IDF)/components/log/include -I$(IDF)/components/esp_system/include -I$(IDF)/components/driver/esp32s3/include -I$(IDF)/components/esp_ringbuf/include -I$(IDF)/components/heap/include -I$(IDF)/components/esp_rom/include/esp32s3 -I$(IDF)/components/newlib/platform_include -I$(IDF)/components/esp_timer/include -I$(IDF)/components/freertos/include -I$(IDF)/components/freertos/port/xtensa/include -I$(IDF)/components/freertos/include/esp_additions/freertos -I$(IDF)/components/xtensa/include -I$(IDF)/components/xtensa/esp32s3/include -I$(IDF)/components/driver/include -I$(IDF)/components/esp_lcd/include -I$(IDF)/components/esp_lcd/interface -I$(IDF)/components/esp_hw_support/include/soc -I$(IDF)/components/esp_hw_support/port -I$(IDF)/components/esp_hw_support/port/esp32s3/private_include  -I$(IDF)/components/esp_hw_support/port/esp32s3 -I$(IDF)/components/esp_hw_support/port/esp32s3/include -I$(IDF)/components/soc/esp32s3/register/ -I$(IDF)/components/esp_rom/esp32s3 -I$(IDF)/components/esp_hal_regi2c/esp32s3/include -DHAVE_CONFIG_H  -ffunction-sections -fdata-sections -Wall -Werror=all -Wno-error=unused-function -Wno-error=unused-variable -Wno-error=deprecated-declarations -Wextra -Wno-unused-parameter -Wno-sign-compare -ggdb -O2 -fmacro-prefix-map=$(IDF)=IDF -fstrict-volatile-bitfields -Wno-error=unused-but-set-variable -fno-jump-tables -fno-tree-switch-conversion -I. -DCONFIG_IDF_TARGET=

$(ELF) : main.c
	mkdir -p debug
	xtensa-esp32s3-elf-gcc  $^ $(CFLAGS)  main.S -T $(LINKER_SCRIPT) -o $@ -nodefaultlibs -nostartfiles
	xtensa-esp32s3-elf-objdump -s $@ > debug/debug_main_s.txt
	xtensa-esp32s3-elf-objdump -t $@ > debug/debug_main_t.txt
	xtensa-esp32s3-elf-objdump -S $@ > debug/debug_main_S.txt


main.bin : main.elf
	esputil -v mkbin $^ $@
    #  -chip esp32-s3 
	#xtensa-esp32s3-elf-objcopy -j .inst -O binary $^ debug/$@.inst
	#xtensa-esp32s3-elf-objcopy -j .data -O binary $^ debug/$@.data
	#cat debug/$@.inst debug/$@.data > debug/$@.concat
	#./checksums.c < debug/$@.concat > $@

#  --before no_reset --after no_reset 
run : main.bin
	esptool.py --no-stub --port $(PORT) load_ram $<
	stty -F /dev/ttyUSB0 115200 -echo raw
	cat /dev/ttyUSB0

flash : main.bin
	esputil -p $(PORT) -fp 0x010 flash 0x1000 $^
	#esptool.py --port $(PORT) write_flash 0x1000 $^

debug/main_flash.bin : main.bin

monitor : 
	stty -F $(PORT) 115200 -echo raw && cat $(PORT)

clean :
	rm -rf *.o *~ $(ELF) main.bin debug/debug_main_s.txt debug/debug_main_t.txt debug/debug_main_S.txt main_inst.bin main_data.bin debug



