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

#include "capture.h"
#include "osc.h"
#include "config.h"

#include <jpeglib.h>
#include <zmq.h>

volatile sig_atomic_t userbreak;

void quitproc (int sig) {
    fprintf (stderr, "interrupt caught, exiting.\n");
    userbreak = 1;
}



void save_jpeg(const char *filename) {
    // bool ipl2jpeg(IplImage *frame, unsigned char **outbuffer, long unsigned int *outlen) {
    unsigned char *gray;
    int gw, gh, gs;

    gray = capture_pointer();
    gh = capture_height();
    gs = capture_stride();
    gw = capture_width();

    // unsigned char *outdata = (uchar *) frame->imageData;
    struct jpeg_compress_struct cinfo = {0};
    struct jpeg_error_mgr jerr;
    JSAMPROW row_ptr[1];

    FILE *fp = fopen(filename, "wb");

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp);

    cinfo.image_width = gw;
    cinfo.image_height = gh;
    cinfo.input_components = 1;
    cinfo.in_color_space = JCS_GRAYSCALE; // JCS_RGB;
    jpeg_set_quality(&cinfo, 80, FALSE);

    jpeg_set_defaults(&cinfo);
    jpeg_start_compress(&cinfo, TRUE);

    while (cinfo.next_scanline < cinfo.image_height) {
        row_ptr[0] = gray + cinfo.next_scanline * gs;
        jpeg_write_scanlines(&cinfo, row_ptr, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    fclose(fp);
}

char brichars[] = " .-:+*o8";
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
            userbreak = 1;
        }

        if (strcmp(buffer, "snapshot") == 0) {
            // save snapshot
            save_jpeg("snapshot.jpg");
        }

        if (strcmp(buffer, "trigger") == 0) {
            // save snapshot
        }

        zmq_send (responder, "ok", 2, 0);
    }
}

int main (int argc, char **argv) {
    int i, bri;
    int x, y;
    int o, framenum = 0;
    unsigned char *gray;
    int gw, gh, gs;

    char temp[160];
    struct timeval start, ts;
    float fps;

    if (signal (SIGINT, quitproc) == SIG_ERR) {
        perror ("Couldn't install SIGINT handler");
        exit (1);
    }

    config_init("mmnew.conf");

    char osc_host[100];
    config_get_string("osc_host", (char *)&osc_host, (char *)"127.0.0.1");
    int osc_port = config_get_int("osc_port", 8003);

    osc_init(osc_host, osc_port);

    if (!capture_start(900, 400)) {
        exit(-1);
    }

    gettimeofday(&start, NULL);

    config_save();

    server_start(9999);

    while (userbreak < 1) {

        server_poll();

        capture_wait();

        printf("%08d: [ ", framenum);

        gray = capture_pointer();
        gh = capture_height();
        gs = capture_stride();
        gw = capture_width();

        y = (framenum * 15) % gh;// / 2;
        for(i=0; i<120; i++) {
            x = (i * gw) / 120;
            o = (y * gw) + x;
            bri = (gray[o] * 8 / 256);
            // printf("%d", bri);
            printf("%c", brichars[bri]);
            // printf("%03d %03d %03d\n", grey[0], grey[1], grey[2]);
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

        printf(" ] [ ");

        // every zone...

        printf(" ] %1.1f fps\n", fps);

        if (framenum % 10 == 0) {
            save_jpeg("dummy.jpg");
        }

        if (framenum % 15 == 0) {
            // save_jpeg("dummy.jpg");
            osc_send("/motion/1", 100);
        }

        if (framenum % 99 == 0) {
            // save_jpeg("dummy.jpg");
            config_save();
        }

    }

    capture_stop();
    server_stop();
    exit (0);
}




