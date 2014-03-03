#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "bitmap.h"
#include "areas.h"

AREA_MOTION_CALLBACK area_callback = NULL;

int history_averageframes = 0;
int areas_frame_index = 0;

BITMAP *history4;
BITMAP *history3;
BITMAP *history2;
BITMAP *history1;
BITMAP *average;
BITMAP *diff;

int num_areas;
Area *areas;

void check_area_motion(BITMAP *input, Area *area) {
    // printf("FRAME: check motion in area id #%d...\n", area->id);
    area->trig = 0;

    // for directional support compare diff against x/y-offset average images and pick the one with least diff.

    int diff_sum = 0;
    for(int j=0; j<area->height; j++) {
        int y2 = area->y + j;
        if (y2 >= 0 && y2 < input->height) {
          int o = y2 * diff->stride + (area->x * diff->channels);
          for(int i=0; i<area->width; i++) {
              for(int c=0; i<diff->channels; i++) {
                  diff_sum += diff->buffer[o];
                  o ++;
              }
          }
        }
    }
    diff_sum /= diff->channels;

    area->motion = diff_sum;

    int pct = (area->motion - area->min_motion) * 100 / (area->max_motion - area->min_motion);

    area->percent = pct;
    area->trig = (area->motion > area->min_motion);

    for(int i=99; i>=1; i--) {
        area->percent_history[i] = area->percent_history[i-1];
        area->trig_history[i] = area->trig_history[i-1];
    }

    area->percent_history[0] = area->percent;
    area->trig_history[0] = area->trig;

    if (area->trig) {
        printf("MOTION: area %d triggered - current=%d [%d - %d] - percent=%d\n",
            area->id, area->motion, area->min_motion, area->max_motion, area->percent);
        // osc_send(area->message, area->percent_motion - area->treshold_percent);
        if (area_callback != NULL) {
        	area_callback(area->id, area->message, area->percent);
        }
    }
}

void highlight_areas(unsigned char *temp) {
    /*
    for(int i=0; i<num_areas; i++) {
        Area *area = (Area *)&areas[i];
        for(int j=0; j<area->height; j++) {
            int o = ((area->y + j) * frame_width + area->x) * 3;
            int line2 = (area->height-1) - (area->height - 1) * area->treshold_percent / 100.0;
            for(int i=0; i<area->width; i++) {
                int c = (i * 1);
                if (c > 99)
                    c = 99;
                int line = (area->height-1) - (area->height - 1) * area->percent_history[c] / 100.0;
                int trig = (area->trig_history[c]);
                if (j == line) {
                        temp[o + 1] = 255;
                    if (trig) {
                        temp[o + 0] = 255;
                        temp[o + 2] = 255;
                    }
                } else if (j > line2) {
                    temp[o + 1] = 120;
                    if (trig) {
                        temp[o + 1] = 200;
                        temp[o + 0] = 120;
                    }
                } else {
                    temp[o + 1] = 60;
                    if (trig) {
                        temp[o + 1] = 120;
                        temp[o + 0] = 60;
                    }
                }
                o += 3;
            }
        }
    }
    */
}

void update_average_and_diff(BITMAP *input) {
  int o = 0, c;
  int len = average->stride * average->height;

  if (history_averageframes < 2) {
    unsigned char *avgptr = average->buffer;
    unsigned char *h1ptr = history1->buffer;
    c = len;
    while (--c > 0) {
      // average->buffer[o] = history1->buffer[o];
      // o++;
      *avgptr++ = *h1ptr++;
      // *avgptr = *h1ptr;
      // avgptr ++;
      // h1ptr ++;
    }
  } else if (history_averageframes == 2) {
    unsigned char *avgptr = average->buffer;
    unsigned char *h1ptr = history1->buffer;
    unsigned char *h2ptr = history2->buffer;
    c = len;
    while (--c > 0) {
      *avgptr++ = (*h1ptr++ + *h2ptr++) >> 1;
      // *avgptr = (*h1ptr + *h2ptr) / 2;
      // avgptr ++;
      // h1ptr ++;
      // h2ptr ++;
      // average->buffer[o] = (history1->buffer[o] + history2->buffer[o]) >> 1;
      // o++;
    }
  } else if (history_averageframes == 3) {
    unsigned char *avgptr = average->buffer;
    unsigned char *h1ptr = history1->buffer;
    unsigned char *h2ptr = history2->buffer;
    unsigned char *h3ptr = history3->buffer;
    c = len;
    while (--c > 0) {
      *avgptr++ = (*h1ptr++ + *h2ptr++ + *h3ptr++) / 3;
      // *avgptr = (*h1ptr + *h2ptr + *h3ptr) / 3;
      // avgptr ++;
      // h1ptr ++;
      // h2ptr ++;
      // h3ptr ++;
      // average->buffer[o] = (history1->buffer[o] + history2->buffer[o] + history3->buffer[o]) / 3;
      // o++;
    }
  } else { // 4 +
    unsigned char *avgptr = average->buffer;
    unsigned char *h1ptr = history1->buffer;
    unsigned char *h2ptr = history2->buffer;
    unsigned char *h3ptr = history3->buffer;
    unsigned char *h4ptr = history4->buffer;
    c = len;
    while (--c > 0) {
      *avgptr++ = (*h1ptr++ + *h2ptr++ + *h3ptr++ + *h4ptr++) >> 2;
      // *avgptr = (*h1ptr + *h2ptr + *h3ptr + *h4ptr) >> 2;
      // avgptr ++;
      // h1ptr ++;
      // h2ptr ++;
      // h3ptr ++;
      // h4ptr ++;
      // average->buffer[o] = (history1->buffer[o] + history2->buffer[o] + history3->buffer[o] + history4->buffer[o]) >> 2;
      // o++;
    }
  }

  {
    unsigned char *diffptr = diff->buffer;
    unsigned char *avgptr = average->buffer;
    unsigned char *inptr = input->buffer;
    c = len;
    while (--c > 0) {
      *diffptr++ = abs(*inptr++ - *avgptr++);
      //  diffptr ++;
      //  inptr ++;
      //  avgptr ++;
      // diff->buffer[o] = abs(input->buffer[o] - average->buffer[o]);
      // o++;
    }
  }
}

void areas_init() {
    areas = (Area *)malloc(sizeof(Area) * 100);
    num_areas = 0;
    history_averageframes = 4;
	history4 = bitmap_init(0, 0, 1);
	history3 = bitmap_init(0, 0, 1);
	history2 = bitmap_init(0, 0, 1);
	history1 = bitmap_init(0, 0, 1);
	average = bitmap_init(1, 1, 1);
	diff = bitmap_init(0, 0, 1);
	areas_frame_index = 0;
}

void areas_set_average_frames(int averageframes) {
    history_averageframes = averageframes;
}

void areas_free() {
	bitmap_free(history4);
	bitmap_free(history3);
	bitmap_free(history2);
	bitmap_free(history1);
	bitmap_free(average);
	bitmap_free(diff);
    free(areas);
}

int areas_count() {
	return num_areas;
}

Area *areas_get(int area) {
    return (Area *)&areas[area];
}

void areas_get_history(int area, int *history, int historysize) {
	for(int i=0; i<historysize; i++) {
		history[i] = 0;
	}
}

void areas_add(int x, int y, int width, int height, int minchange, int maxchange, char *prefix) {
    Area *area = (Area *)&areas[num_areas];
    area->id = 0;
    area->x = x;
    area->y = y;
    area->width = width;
    area->height = height;
    area->trig = 0;
    area->motion = 0;
    area->min_motion = minchange;
    area->max_motion = maxchange;
    area->message = strdup(prefix);
    num_areas ++;
    printf("AREAS: Creating area #%d at %d, %d size %d x %d motion range %d-%d, message \"%s\".\n",
        num_areas,
        area->x,
        area->y,
        area->width,
        area->height,
        area->min_motion,
        area->max_motion,
        area->message);
}

void areas_check(BITMAP *input, AREA_MOTION_CALLBACK callback) {
	area_callback = callback;

    bitmap_resize(history4, input->width, input->height);
    bitmap_resize(history3, input->width, input->height);
    bitmap_resize(history2, input->width, input->height);
    bitmap_resize(history1, input->width, input->height);
    bitmap_resize(average, input->width, input->height);
    bitmap_resize(diff, input->width, input->height);

    update_average_and_diff(input);

    bitmap_copy(history4, history3);
    bitmap_copy(history3, history2);
    bitmap_copy(history2, history1);
    bitmap_copy(history1, input);


    if (areas_frame_index > 10) {
	    for(int i=0; i<num_areas; i++) {
	        Area *area = (Area *)&areas[i];
	        check_area_motion(input, area);
	    }
    } else {
        printf("MOTION: Skipping initial frame #%d\n", areas_frame_index);
    }

	areas_frame_index ++;
}

BITMAP *areas_output_bitmap() {
	return average;
}
