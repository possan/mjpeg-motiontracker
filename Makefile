SYS := $(shell gcc -dumpmachine)

all: mm mm-msg mmnew

mm: mm.cpp
ifneq (, $(findstring linux, $(SYS)))
	gcc -O9 -o mm mm.cpp -lrt -ljpeg -lcurl
else
	gcc -O2 -o mm mm.cpp -ljpeg -lcurl \
		-I/usr/local/Cellar/jpeg/8d/include/ \
		-L/usr/local/Cellar/jpeg/8d/lib \
		-I/usr/local/Cellar/curl/7.33.0/include/curl/ \
		-L/usr/local/Cellar/curl/7.33.0/lib
endif

mm-msg: mm-msg.cpp
ifneq (, $(findstring linux, $(SYS)))
	gcc -O9 -o mm-msg mm-msg.cpp -lrt -ljpeg -lcurl
else
	gcc -O2 -o mm-msg mm-msg.cpp -ljpeg -lcurl -I/usr/local/Cellar/jpeg/8d/include/ -L/usr/local/Cellar/jpeg/8d/lib -I/usr/local/Cellar/curl/7.33.0/include/curl/ -L/usr/local/Cellar/curl/7.33.0/lib
endif

mmnew: mmnew.cpp osc.cpp osc.h capture.cpp capture.h bitmap.cpp bitmap.h areas.cpp areas.h
ifneq (, $(findstring linux, $(SYS)))
	# gcc -O9 -o mm-msg mm-msg.cpp -lrt -ljpeg -lcurl
	gcc -O9 -o mmnew -ljpeg -fpermissive -I. osc.cpp capture.cpp areas.cpp bitmap.cpp mmnew.cpp
else
	# gcc -O9 -o mm-msg mm-msg.cpp -ljpeg -lcurl
	gcc -O2 -o mmnew -ljpeg -I. \
		-I/usr/local/Cellar/jpeg/8d/include/ \
		-L/usr/local/Cellar/jpeg/8d/lib \
		osc.cpp capture.cpp areas.cpp bitmap.cpp mmnew.cpp
endif

clean:
	rm -f mm
	rm -f mm-msg
	rm -f mmnew

install: mm mm-msg
	cp mm /usr/local/bin/mm
	cp mm-msg /usr/local/bin/mm-msg
	cp mmnew /usr/local/bin/mmnew
