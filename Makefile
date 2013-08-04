all: mm test

mm: mm.cpp
	gcc -o mm mm.cpp -ljpeg -lcurl

test: mm
	./mm config1.txt
