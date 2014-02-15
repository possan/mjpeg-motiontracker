#ifndef _AREAS_H_
#define _AREAS_H_

#include "bitmap.h"

struct Area {
    int id;
    int x;
    int y;
    int width;
    int height;
    int trig;
    int trig_history[100];
    int min_motion;
    int max_motion;
    int motion;
    int percent;
    int percent_history[100];
    char *message;
};

typedef void (* AREA_MOTION_CALLBACK)(int area, char *prefix, int value);

extern void areas_init();

extern void areas_set_average_frames(int averageframes);

extern int areas_count();

extern Area *areas_get(int area);

extern void areas_get_history(int area, int *history, int historysize);

extern void areas_add(int x, int y, int width, int height, int minchange, int maxchange, char *prefix);

extern void areas_check(BITMAP *input, AREA_MOTION_CALLBACK callback);

extern BITMAP *areas_output_bitmap();

extern void areas_free();

#endif
