/*

MotionTracker New

Based on HasciiCam code

./mmnew mmnew.conf /tmp/snapshot.jpg /tmp/snapshot.json§

*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/time.h>
#include <signal.h>

#include "capture.h"
#include "osc.h"
#include "bitmap.h"
#include "areas.h"

#include <jpeglib.h>
// #include <zmq.h>

volatile int userbreak;
char brichars[] = " .-:+*o8";

char config_snapshot_temp_filename[100] = "/var/tmp/snapshottemp.jpg";
char config_snapshot_filename[100] = "/var/tmp/snapshot.jpg";
char config_overlay_filename[100] = "/var/tmp/overlay.jpg";
char config_stats_filename[100] = "/var/tmp/stats.json";
char config_osc_hostname[100] = "127.0.0.1";
int config_osc_port = 8003;
int config_areas_average = 4;
// int config_capture_width = 640;
// int config_capture_height = 480;
ICapture *capture = NULL;
struct timeval start, ts;
int framenum = 0;

void quitproc (int sig) {
    fprintf (stderr, "interrupt caught, exiting.\n");
    if (capture != NULL) {
        delete(capture);
        capture = NULL;
    }
    userbreak = 1;
}

void save_jpeg(BITMAP *bmp, const char *filename) {
    struct jpeg_compress_struct cinfo = {0};
    struct jpeg_error_mgr jerr;
    JSAMPROW row_ptr[1];

    FILE *fp = fopen(filename, "wb");

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp);

    cinfo.image_width = bmp->width;
    cinfo.image_height = bmp->height;
    cinfo.input_components = 1;
    cinfo.in_color_space = JCS_GRAYSCALE; // JCS_RGB;
    jpeg_set_quality(&cinfo, 80, FALSE);

    jpeg_set_defaults(&cinfo);
    jpeg_start_compress(&cinfo, TRUE);

    while (cinfo.next_scanline < cinfo.image_height) {
        row_ptr[0] = bmp->buffer + cinfo.next_scanline * bmp->stride;
        jpeg_write_scanlines(&cinfo, row_ptr, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    fclose(fp);
}

void motion_callback(int area, char *prefix, int motion) {
    // printf("MOTION CALLBACK: %d %s %d\n", area, prefix, motion);
    osc_send(prefix, motion);
}


bool read_config(const char *filename) {
    printf("CONFIG: Loading \"%s\"...\n", filename);
    char buf[200];
    char buf2[200];
    FILE *f = fopen(filename, "rt");
    if (f == NULL) {
        printf("CONFIG: Unable to read config.\n");
        return false;
    }
    while(fgets(buf, 200, f) != NULL) {
        printf("CONFIG: Line: \"%s\"\n", buf);
        char *word = NULL;
        char *ptr = (char *)&buf;

        char *cmd = strsep(&ptr, " \n\r");
        if (cmd == NULL)
            continue;

        // printf("CONFIG: Command \"%s\"\n", cmd);
        if (strcmp(cmd, "osc") == 0) {
            char *str_host = strsep(&ptr, " \n\r");
            char *str_port = strsep(&ptr, " \n\r");
            if (str_host != NULL && str_port != NULL) {
                strcpy(config_osc_hostname, str_host);
                config_osc_port = atoi(str_port);
            }
        }
        else if (strcmp(cmd, "average") == 0) {
            char *str_average = strsep(&ptr, " \n\r");
            if (str_average != NULL) {
                config_areas_average = atoi(str_average);
            }
        }
        /*
        else if (strcmp(cmd, "capture") == 0) {
            char *str_width = strsep(&ptr, " \n\r");
            char *str_height = strsep(&ptr, " \n\r");
            if (str_width != NULL && str_height != NULL) {
                config_capture_width = atoi(str_width);
                config_capture_height = atoi(str_height);
            }
        }
        */
        else if (strcmp(cmd, "area") == 0) {
            char *str_x = strsep(&ptr, " \n\r");
            char *str_y = strsep(&ptr, " \n\r");
            char *str_w = strsep(&ptr, " \n\r");
            char *str_h = strsep(&ptr, " \n\r");
            char *str_min = strsep(&ptr, " \n\r");
            char *str_max = strsep(&ptr, " \n\r");
            char *str_message = strsep(&ptr, " \n\r");
            if (str_x != NULL && str_y != NULL && str_w != NULL && str_h != NULL && str_min != NULL && str_max != NULL && str_message != NULL) {
                areas_add(
                    atoi(str_x),
                    atoi(str_y),
                    atoi(str_w),
                    atoi(str_h),
                    atoi(str_min),
                    atoi(str_max),
                    strdup(str_message)
                );
            }
        } else if(strcmp(cmd, "input") == 0) {
            char *method = strsep(&ptr, " \n\r");
            if (strcmp(method, "video4linux") == 0) {
#ifndef __MACH__
                char *str_path = strsep(&ptr, " \n\r");
                char *str_width = strsep(&ptr, " \n\r");
                char *str_height = strsep(&ptr, " \n\r");
                if (str_path != NULL && str_width != NULL && str_height != NULL) {
                    int config_capture_width = atoi(str_width);
                    int config_capture_height = atoi(str_height);
                    capture = create_video4linux_capture(str_path, config_capture_width, config_capture_height);
                }
#endif
            } else if(strcmp(method, "mjpeg") == 0) {
                char *str_url = strsep(&ptr, " \n\r");
                char *str_auth = strsep(&ptr, " \n\r");
                if (str_url != NULL) {
                    capture = create_mjpeg_capture(str_url, str_auth);
                }

            } else if(strcmp(method, "dummy") == 0) {

                char *str_width = strsep(&ptr, " \n\r");
                char *str_height = strsep(&ptr, " \n\r");
                if (str_width != NULL && str_height != NULL) {
                    int config_capture_width = atoi(str_width);
                    int config_capture_height = atoi(str_height);
                    capture = create_dummy_capture(config_capture_width, config_capture_height);
                }
            }
        }
    }
    fclose(f);
    return true;
}

void capture_callback(BITMAP *bmp) {
    struct timeval ts;
    int i, bri;
    int x, y;
    int o;
    float fps;

    areas_check(bmp, &motion_callback);

    printf("%08d: [ ", framenum);
    y = (framenum * 20) % bmp->height;// / 2;
    for(i=0; i<80; i++) {
        x = (i * bmp->width) / 80;
        o = (y * bmp->stride) + x;
        bri = (bmp->buffer[o] * 8 / 256);
        printf("%c", brichars[bri]);
    }
    framenum ++;
    gettimeofday(&ts, NULL);
    ts.tv_sec -= start.tv_sec;
    ts.tv_usec -= start.tv_usec;
    if (ts.tv_usec < 0) {
        ts.tv_sec--;
        ts.tv_usec += 1000000;
    }
    fps = (framenum-1)/(ts.tv_usec+1000000.0*ts.tv_sec)*1000000.0;
    printf(" ][ ");

    // every zone...
    for(i=0; i<areas_count(); i++) {
        Area *a = areas_get(i);
        printf("%3d%% ", a->percent);
    }

    printf("] %1.1f fps\n", fps);

    if (framenum % 9 == 0) {
        BITMAP *bmp2 = areas_output_bitmap();
        if (bmp2 != NULL) {
            // printf("save jpeg\n");
            save_jpeg(bmp2, config_snapshot_temp_filename);
            remove(config_snapshot_filename);
            rename(config_snapshot_temp_filename, config_snapshot_filename);
        }
    }

    if (framenum % 2 == 0) {
        // printf("save stats\n");
        float fps;
        char ret[25000];
        char buf[200];
        char buf2[1200];
        char buf3[1200];
        sprintf(ret, "{ \"fps\": %1.1f, \"areas\": [ ", fps);
        for(int i=0; i<areas_count(); i++) {
            Area *a = areas_get(i);
            strcpy(buf2, "");
            strcpy(buf3, "");
            for(int j=0; j<20; j++) {
                if (j > 0) {
                    strcat(buf2, ",");
                    strcat(buf3, ",");
                }

                sprintf(buf, "%d", a->percent_history[j]);
                strcat(buf2, buf);

                sprintf(buf, "%d", a->trig_history[j]);
                strcat(buf3, buf);
            }
            sprintf(buf, "{ \"id\":%d, \"motion\":%d, \"percent\":%d, \"percent_history\":[%s], \"trig_history\":[%s] }", i, a->motion, a->percent, buf2, buf3);
            if (i > 0) {
                strcat(ret, ", ");
            }
            strcat(ret, buf);
        }
        strcat(ret, " ] }");
        FILE *f = fopen(config_stats_filename, "wt");
        fputs(ret, f);
        fclose(f);
    }

    usleep(1000);
}

int main (int argc, char **argv) {
    char temp[160];

    if (signal (SIGINT, quitproc) == SIG_ERR) {
        perror ("Couldn't install SIGINT handler");
        exit (1);
    }

    if (argc != 4) {
        printf("Syntax: mmnew [config file] [snapshot jpeg file] [stats json file]\n");
        exit (1);
    }

    areas_init();

    strcpy(config_snapshot_filename, argv[2]);

    sprintf(config_snapshot_temp_filename, "%s.tmp", config_snapshot_filename);

    strcpy(config_stats_filename, argv[3]);
    read_config(argv[1]);

    areas_set_average_frames(config_areas_average);
    osc_init(config_osc_hostname, config_osc_port);

    if (capture == NULL) {
        printf("Error: No capture device selected in config file:\n");
        printf("\nSyntax:\n");
        printf("\tinput video4linux /dev/video0 640 480\n");
        printf("\tinput mjpeg http://user:pass@camera/image.mjpeg\n");
        printf("\tinput dummy 640 480\n");
        exit(-1);
    }



    gettimeofday(&start, NULL);
    // server_start(9999);

    while (userbreak < 1) {
        if (capture != NULL) {
            capture->block(&capture_callback);
        }
    }

    areas_free();

    if (capture != NULL) {
        delete(capture);
        capture = NULL;
    }

    // server_stop();
    exit (0);
}
