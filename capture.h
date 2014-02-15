#ifndef _CAPTURE_H_
#define _CAPTURE_H_

extern bool capture_start(int width, int height);

extern void capture_wait(); // blocking

extern int capture_width();
extern int capture_stride();
extern int capture_height();

extern unsigned char *capture_pointer();

extern void capture_stop();

#endif