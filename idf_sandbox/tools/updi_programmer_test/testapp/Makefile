all : testapp

testapp : testapp.c
	gcc -o $@ $^ -ludev

avr_test.bin : avr_test.elf
	avr-size $^
	avr-objcopy $^ -O binary $@
	avr-objdump -t $^ > $^.map
	avr-objdump -S $^ > $^.lst

avr_test.elf : avr_test.c attiny416.S
	avr-gcc -O1 -mmcu=attiny416 -DF_CPU=8000000 -nostdlib -o $@ $^ 


clean :
	rm -rf testapp avr_test.elf avr_test.bin avr_test.map avr_test.lst

