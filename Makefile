all: mm

mm: mm.cpp
	gcc -O9 -o mm mm.cpp -ljpeg -lcurl

clean:
	rm -f mm

install:
	cp mm /usr/local/bin/mm
