all : testapp badappleplay

badappleplay : badappleplay.c
	gcc -o $@ $^ -lavcodec -lavutil -lswscale -lavformat -ludev -Og -g

testapp : testapp.c
	gcc -o $@ $^ -ludev

clean :
	rm -rf testapp badappleplay
