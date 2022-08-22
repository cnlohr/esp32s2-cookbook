ELF:=main.elf

all : $(ELF)


PORT?=/dev/ttyUSB0

LINKER_SCRIPT:=esp32s2-ram.lds


CFLAGS:=-mlongcalls -DHAVE_CONFIG_H -I/home/cnlohr/esp/esp-idf/components/hal/include -I/home/cnlohr/esp/esp-idf/components/hal/esp32s2/include -I/home/cnlohr/esp/esp-idf/components/esp_wifi/include -I/home/cnlohr/esp/esp-idf/components/esp_event/include -I/home/cnlohr/esp/esp-idf/components/soc/include -I/home/cnlohr/esp/esp-idf/components/soc/esp32s2/include -I/home/cnlohr/esp/esp-idf/components/esp_common/include -I/home/cnlohr/esp/esp-idf/components/driver/include -I/home/cnlohr/esp/esp-idf/components/esp_hw_support/include -I/home/cnlohr/esp/esp-idf/components/esp_rom/include -I/home/cnlohr/esp/esp-idf/components/log/include -I/home/cnlohr/esp/esp-idf/components/esp_system/include -I/home/cnlohr/esp/esp-idf/components/driver/esp32s2/include -I/home/cnlohr/esp/esp-idf/components/esp_ringbuf/include -I/home/cnlohr/esp/esp-idf/components/heap/include -I/home/cnlohr/esp/esp-idf/components/esp_rom/include/esp32s2 -I/home/cnlohr/esp/esp-idf/components/newlib/platform_include -I/home/cnlohr/esp/esp-idf/components/esp_timer/include -I/home/cnlohr/esp/esp-idf/components/freertos/include -I/home/cnlohr/esp/esp-idf/components/freertos/port/xtensa/include -I/home/cnlohr/esp/esp-idf/components/freertos/include/esp_additions/freertos -I/home/cnlohr/esp/esp-idf/components/xtensa/include -I/home/cnlohr/esp/esp-idf/components/xtensa/esp32s2/include -I/home/cnlohr/esp/esp-idf/components/driver/include -I/home/cnlohr/esp/esp-idf/components/esp_lcd/include -I/home/cnlohr/esp/esp-idf/components/esp_lcd/interface -I/home/cnlohr/esp/esp-idf/components/esp_hw_support/include/soc -I/home/cnlohr/esp/esp-idf/components/esp_hw_support/port -I/home/cnlohr/esp/esp-idf/components/esp_hw_support/port/esp32s2/private_include  -I/home/cnlohr/esp/esp-idf/components/esp_hw_support/port/esp32s2 -DHAVE_CONFIG_H  -ffunction-sections -fdata-sections -Wall -Werror=all -Wno-error=unused-function -Wno-error=unused-variable -Wno-error=deprecated-declarations -Wextra -Wno-unused-parameter -Wno-sign-compare -ggdb -O2 -fmacro-prefix-map=/home/cnlohr/esp/esp-idf=IDF -fstrict-volatile-bitfields -Wno-error=unused-but-set-variable -fno-jump-tables -fno-tree-switch-conversion -I.

$(ELF) : main.c
	mkdir -p debug
	xtensa-esp32s2-elf-gcc  $^ $(CFLAGS)  main.S -T $(LINKER_SCRIPT) -o $@ -nodefaultlibs -nostartfiles
	xtensa-esp32s2-elf-objdump -s $@ > debug/debug_main_s.txt
	xtensa-esp32s2-elf-objdump -t $@ > debug/debug_main_t.txt
	xtensa-esp32s2-elf-objdump -S $@ > debug/debug_main_S.txt


main.bin : main.elf
	esputil -v mkbin $^ $@
    #  -chip esp32-s2 
	#xtensa-esp32s2-elf-objcopy -j .inst -O binary $^ debug/$@.inst
	#xtensa-esp32s2-elf-objcopy -j .data -O binary $^ debug/$@.data
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



