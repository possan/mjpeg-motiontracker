SYS := $(shell gcc -dumpmachine)

all: mm

mm: mm.cpp
ifneq (, $(findstring linux, $(SYS)))
	gcc -O9 -o mm mm.cpp -lrt -ljpeg -lcurl
else
	gcc -O9 -o mm mm.cpp -ljpeg -lcurl
endif

clean:
	rm -f mm

install: mm
	cp mm /usr/local/bin/mm
