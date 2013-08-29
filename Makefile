SYS := $(shell gcc -dumpmachine)

all: mm mm-msg

mm: mm.cpp
ifneq (, $(findstring linux, $(SYS)))
	gcc -O9 -o mm mm.cpp -lrt -ljpeg -lcurl
else
	gcc -O9 -o mm mm.cpp -ljpeg -lcurl
endif

mm-msg: mm-msg.cpp
ifneq (, $(findstring linux, $(SYS)))
	gcc -O9 -o mm-msg mm-msg.cpp -lrt -ljpeg -lcurl
else
	gcc -O9 -o mm-msg mm-msg.cpp -ljpeg -lcurl
endif

clean:
	rm -f mm
	rm -f mm-msg

install: mm mm-msg
	cp mm /usr/local/bin/mm
	cp mm-msg /usr/local/bin/mm-msg
