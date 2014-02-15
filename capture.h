#ifndef _CAPTURE_H_
#define _CAPTURE_H_

#include "bitmap.h"

extern bool capture_start(int width, int height);

extern void capture_wait(); // blocking

extern BITMAP *capture_bitmap();

extern void capture_stop();

#endif