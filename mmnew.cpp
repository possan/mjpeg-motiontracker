/* Based on HasciiCam code */

/*
UVC Ctrl
--------

sudo uvcdynctrl -s "White Balance Temperature, Auto" 0 .5
sudo uvcdynctrl -s "Gain" 100
sudo uvcdynctrl -s "Exposure (Absolute)" 200
sudo uvcdynctrl -s 'Focus, Auto' 0
sudo uvcdynctrl -s 'Focus (absolute)' 0

Makefile
--------

all: mini

mini: mini.cpp
    g++ -o mini -O9 -ljpeg -fpermissive mini.cpp

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
int config_capture_width = 640;
int config_capture_height = 480;

void quitproc (int sig) {
    fprintf (stderr, "interrupt caught, exiting.\n");
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

/*
void *context;
void *responder;

void server_start(int port) {
    //  Socket to talk to clients
    context = zmq_ctx_new ();
    responder = zmq_socket (context, ZMQ_REP);
    char url[20];
    sprintf(url, "tcp://*:%d", port);
    int rc = zmq_bind(responder, url);
}

void server_stop() {
    zmq_close(responder);
    zmq_ctx_destroy(context);
}

void server_poll() {
    char buffer[100] = { 0, };
    if (zmq_recv(responder, buffer, 100, ZMQ_DONTWAIT) == EAGAIN) {
        return;
    }
    if (strlen(buffer) > 0) {
        printf ("Received command: [%s]\n", buffer);

        if (strcmp(buffer, "restart") == 0) {
            zmq_send (responder, "ok", 2, 0);
            userbreak = 1;
        }

        if (strcmp(buffer, "snapshot") == 0) {
            // save snapshot
            save_jpeg(capture_bitmap(), config_snapshot_filename);
            zmq_send (responder, "ok", 2, 0);
        }

        if (strncmp(buffer, "trigger,", 7) == 0) {
            // save snapshot
            zmq_send (responder, "ok", 2, 0);
            int index = atoi(buffer + 8); // trigger,NNNN
            if (index >= 0 && index < areas_count()) {
                Area *area = areas_get(index);
                osc_send(area->message, 100);
            }
        }

        if (strcmp(buffer, "stat") == 0) {
            // save snapshot
            char ret[5000];
            strcpy(ret, "");
            strcat(ret, "status:[ ");

            for(int i=0; i<areas_count(); i++) {
                Area *a = areas_get(i);
                char buf[200];
                sprintf(buf, "{ \"id\":%d, \"motion\":%d, \"percent\":%d }", i, a->motion, a->percent);

                if (i > 0) {
                    strcat(ret, ", ");
                }
                strcat(ret, buf);
            }
            strcat(ret, " ]");
            zmq_send (responder, ret, strlen(ret), 0);
        }

    }
}
*/

void motion_callback(int area, char *prefix, int motion) {
    printf("MOTION CALLBACK: %d %s %d\n", area, prefix, motion);
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
        else if (strcmp(cmd, "capture") == 0) {
            char *str_width = strsep(&ptr, " \n\r");
            char *str_height = strsep(&ptr, " \n\r");
            if (str_width != NULL && str_height != NULL) {
                config_capture_width = atoi(str_width);
                config_capture_height = atoi(str_height);
            }
        }
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
        }
    }
    fclose(f);
    return true;
}

int main (int argc, char **argv) {
    int i, bri;
    int x, y;
    int o, framenum = 0;

    char temp[160];
    struct timeval start, ts;
    float fps;

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
    if (!capture_start(config_capture_width, config_capture_height)) {
        exit(-1);
    }

    gettimeofday(&start, NULL);
    // server_start(9999);

    while (userbreak < 1) {

        // server_poll();
        capture_wait();

        BITMAP *bmp = capture_bitmap();

        areas_check( bmp, &motion_callback );

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
        // printf(" ] [ ");
        // every zone...
        printf(" ] %1.1f fps\n", fps);

        if (framenum % 5 == 0) {
            // save_jpeg(bmp, "dummy.jpg");
            save_jpeg(areas_output_bitmap(), config_snapshot_temp_filename);
            remove(config_snapshot_filename);
            rename(config_snapshot_temp_filename, config_snapshot_filename);
        }

        /*
        if (framenum % 10 == 0) {
            // save_jpeg(bmp, "dummy.jpg");
            save_jpeg(capture_bitmap(), config_snapshot_temp_filename);
            remove(config_snapshot_filename);
            rename(config_snapshot_temp_filename, config_snapshot_filename);
        }
        */

        if (framenum % 5 == 0) {
            char ret[25000];
            char buf[200];
            char buf2[1200];
            char buf3[1200];
            strcpy(ret, "[ ");
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
            strcat(ret, " ]");
            FILE *f = fopen(config_stats_filename, "wt");
            fputs(ret, f);
            fclose(f);
        }
    }

    areas_free();
    capture_stop();
    // server_stop();
    exit (0);
}




