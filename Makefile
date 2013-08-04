all: mm test

mm: mm.cpp
	gcc -O9 -o mm mm.cpp -ljpeg -lcurl

test: mm
	./mm config1.txt

clean:
	rm -f mm