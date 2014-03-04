#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bitmap.h"

BITMAP *bitmap_init(int width, int height, int channels) {
	printf("BITMAP: Init %d x %d (%d chan)\n", width, height, channels);
	BITMAP *dest = (BITMAP *)malloc(sizeof(BITMAP));
	dest->width = width;
	dest->stride = width * channels;
	dest->channels = channels;
	dest->height = height;
	dest->buffer = (unsigned char *)malloc(dest->stride * (dest->height + 100));
	bitmap_clear(dest);
	return dest;
}

void bitmap_free(BITMAP *bitmap) {
	if (bitmap->buffer != NULL) {
		free(bitmap->buffer);
		bitmap->buffer = NULL;
	}
	free(bitmap);
}

void bitmap_resize(BITMAP *dest, int width, int height) {
	if (width != dest->width || height != dest->height) {
		printf("BITMAP: Resize to %d x %d (%d chan)\n", width, height, dest->channels);
		if (dest->buffer != NULL) {
			free(dest->buffer);
			dest->buffer = NULL;
		}
		dest->width = width;
		dest->stride = width * dest->channels;
		dest->height = height;
		dest->buffer = (unsigned char *)malloc(dest->stride * (dest->height + 100));
	}
}

void bitmap_copy(BITMAP *dest, BITMAP *src) {
	bitmap_resize(dest, src->width, src->height);
    memcpy(dest->buffer, src->buffer, src->stride * src->height);
}

void bitmap_clear(BITMAP *dest) {
    memset(dest->buffer, 0, dest->stride * dest->height);
}
