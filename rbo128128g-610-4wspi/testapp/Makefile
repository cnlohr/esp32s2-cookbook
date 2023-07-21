all : testapp badappleplay

badappleplay : badappleplay.c
	gcc -o $@ $^ -lavcodec -lavutil -lswscale -lavformat -ludev -lm -O2

testapp : testapp.c
	gcc -o $@ $^ -ludev -lm -O2

clean :
	rm -rf testapp badappleplay
