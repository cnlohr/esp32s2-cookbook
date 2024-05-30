all : updioprg

updioprog : updioprog.c
	gcc -g -O2 -o $@ $^ -ludev

avr_test.bin : avr_test.elf
	avr-size $^
	avr-objcopy $^ -O binary $@
	avr-objdump -t $^ > $^.map
	avr-objdump -S $^ > $^.lst

avr_test.elf : avr_test.c attiny416.S
	avr-gcc -O1 -mmcu=attiny416 -DF_CPU=8000000 -nostdlib -o $@ $^ 

flash : avr_test.bin updioprog
	./updioprog ERASE WRITE 0x8000 avr_test.bin READ 0x8000 128  + BOOT

clean :
	rm -rf updioprog avr_test.elf avr_test.bin avr_test.map avr_test.lst avr_test.elf.map avr_test.elf.lst

