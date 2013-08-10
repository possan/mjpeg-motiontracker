all: mm

mm: mm.cpp
	gcc -O9 -o mm mm.cpp -ljpeg -lcurl

clean:
	rm -f mm