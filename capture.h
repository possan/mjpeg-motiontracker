#ifndef _CAPTURE_H_
#define _CAPTURE_H_

#include "bitmap.h"

typedef void (*CaptureFrameCallback)(BITMAP *bitmap);

class ICapture {
	public:
		virtual ~ICapture() {};
		virtual bool block(CaptureFrameCallback callback) = 0;
};

// virtual bool start(int width, int height) = 0;
// extern void
// extern bool capture_start(int width, int height);
// extern void capture_wait(); // blocking
// extern BITMAP *capture_bitmap();
// extern void capture_stop();

extern ICapture *create_dummy_capture(int width, int height);
extern ICapture *create_mjpeg_capture(const char *url, const char *auth);
#ifndef __MACH__
extern ICapture *create_video4linux_capture(const char *device, int width, int height);
#endif

#endif