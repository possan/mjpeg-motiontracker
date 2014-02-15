#ifndef _BITMAP_H_
#define _BITMAP_H_

typedef struct {
	unsigned char *buffer;
	int width;
	int height;
	int stride;
	int channels;
} BITMAP;

extern BITMAP *bitmap_init(int width, int height, int channels);
extern void bitmap_copy(BITMAP *dest, BITMAP *src);
extern void bitmap_clear(BITMAP *dest);
extern void bitmap_resize(BITMAP *dest, int width, int height);
extern void bitmap_free(BITMAP *bitmap);

#endif
