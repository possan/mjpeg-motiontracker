#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include "bitmap.h"
#include "capture.h"

// Darwin dummy impl.

class DummyCapture : public ICapture {
private:
	BITMAP *bmp;
public:
	DummyCapture(int width, int height);
	~DummyCapture();
	void wait();
	bool block(CaptureFrameCallback callback);
};

DummyCapture::DummyCapture(int width, int height) {
	printf("DUMMY CAPTURE: Start capturing %d x %d\n", width, height);
    this->bmp = bitmap_init(width, height, 1);
}


DummyCapture::~DummyCapture() {
	bitmap_free(this->bmp);
}

void DummyCapture::wait() {
	int i;
	for(i = 0; i < this->bmp->stride * this->bmp->height; i ++) {
		this->bmp->buffer[i] = rand() % 255;
	}
	// no delay.
    usleep(100000); // ~10fps
}

bool DummyCapture::block(CaptureFrameCallback callback) {
	this->wait();
	callback(this->bmp);
	return true;
}

ICapture *create_dummy_capture(int width, int height) {
	return new DummyCapture(width, height);
}
