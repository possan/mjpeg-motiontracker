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
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <signal.h>
#include <linux/types.h>
#include <linux/videodev2.h>

#include <jpeglib.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

struct geometry {
  int w, h, size;
  int bright, contrast, gamma; };
// struct geometry aa_geo;
struct geometry vid_geo;
/* if width&height have been manually changed */
int whchanged = 0;

char device[256];

/* buffers */
unsigned char *image = NULL; /* mmapped */

/* declare the sighandler */
void quitproc (int Sig);
volatile sig_atomic_t userbreak;

int buftype = V4L2_BUF_TYPE_VIDEO_CAPTURE;
struct v4l2_capability capability;
struct v4l2_input input;
struct v4l2_standard standard;
struct v4l2_format format;
struct v4l2_requestbuffers reqbuf;
struct v4l2_buffer buffer;
struct {
    void *start;
    size_t length;
} *buffers;

int fd = -1;
unsigned char *grey;
int xbytestep = 2;
int ybytestep = 0;
int renderhop=1, framenum=0; // renderhop is how many frames to guzzle before rendering
int gw, gh; // number of cols/rows in grey intermediate representation
int vw, vh; // video w and h
size_t greysize;
// int vbytesperline;



char osc_ip[100] = "127.0.0.1";
int osc_port = 8003;
int osc_socket;
struct sockaddr_in si_me;

void osc_init() {
    printf("OSC: Using host \"%s\", port %d\n", osc_ip, osc_port);
    osc_socket = socket(AF_INET, SOCK_DGRAM, 0);
    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(osc_port);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (inet_aton(osc_ip, &si_me.sin_addr)==0) {
        printf("OSC: Can't resolve ip.\n");
    }
}


void osc_send(char *msg, float amt) {
    // printf("OSC: Sending message \"%s\" with value %1.1f\n", msg, amt);

    unsigned char msgbuf[100];
    memset(msgbuf, 0, 100);

    // strcpy((char *)&msgbuf, "hej\n");

    int o = 0;

    memcpy(msgbuf+o, msg, strlen(msg));
    o += strlen(msg);

    o += 4 - (o % 4);

    char *typestring = (char *)",f";

    memcpy(msgbuf+o, typestring, strlen(typestring));
    o += strlen(typestring);

    o += 4 - (o % 4);

    char minibuf[4];
    memcpy((char *)&minibuf, (unsigned char *)&amt, 4);

    msgbuf[o] = minibuf[3];
    msgbuf[o+1] = minibuf[2];
    msgbuf[o+2] = minibuf[1];
    msgbuf[o+3] = minibuf[0];
    // memcpy(msgbuf+o, (unsigned char *)&amt, 4);
    o += 4;

    // o += 4 - (o % 4);

    /*
    for(int i=0; i<o; i++) {
        char c = msgbuf[i];
        printf("%02X ", c);
    }
    printf("\n");

    for(int i=0; i<o; i++) {
        char c = msgbuf[i];
        if (c > 32 && c < 128)
            printf("%C  ", c);
        else
            printf("?  ");
    }
    printf("\n");
    */

    if (sendto(osc_socket, (const char *)msgbuf, o, 0, (sockaddr*)&si_me, sizeof(si_me)) == -1) {
        printf("OSC: Failed to send.\n");
    }
}

void YUV422_to_grey(unsigned char *src, unsigned char *dst, int w, int h) {
    unsigned char *writehead, *readhead;
    int x,y;
    writehead = dst;
    readhead  = src;
    for(y=0; y<gh; ++y){
        for(x=0; x<gw; ++x){
            *(writehead++) = *readhead;
            readhead += xbytestep;
        }
        readhead += ybytestep;
    }
}

static int video_set_format(int dev, unsigned int w, unsigned int h, unsigned int format)
{
    struct v4l2_format fmt;
    int ret;

    memset(&fmt, 0, sizeof fmt);
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    fmt.fmt.pix.width = w;
    fmt.fmt.pix.height = h;
    fmt.fmt.pix.pixelformat = format;
    fmt.fmt.pix.field = V4L2_FIELD_ANY;

    ret = ioctl(dev, VIDIOC_S_FMT, &fmt);
    if (ret < 0) {
        printf("Unable to set format: %d.\n", errno);
        return ret;
    }

    printf("Video format set: width: %u height: %u buffer size: %u\n",
        fmt.fmt.pix.width, fmt.fmt.pix.height, fmt.fmt.pix.sizeimage);
    return 0;
}

int vid_detect(char *devfile) {
    int errno;
    unsigned int i;

    if (-1 == (fd = open(devfile,O_RDWR|O_NONBLOCK))) {
        perror("!! error in opening video capture device: ");
        return -1;
    } else {
        close(fd);
        fd = open(devfile,O_RDWR);
    }

    // Check that streaming is supported
    memset(&capability, 0, sizeof(capability));
    if(-1 == ioctl(fd, VIDIOC_QUERYCAP, &capability)) {
        perror("VIDIOC_QUERYCAP");
        exit(EXIT_FAILURE);
    }
    if((capability.capabilities & V4L2_CAP_VIDEO_CAPTURE) == 0){
        printf("Fatal: Device %s does not support video capture\n", capability.card);
        exit(EXIT_FAILURE);
    }
    if((capability.capabilities & V4L2_CAP_STREAMING) == 0){
        printf("Fatal: Device %s does not support streaming data capture\n", capability.card);
        exit(EXIT_FAILURE);
    }

    fprintf(stderr,"Device detected is %s\n",devfile);
    fprintf(stderr,"Card name: %s\n",capability.card);

    video_set_format(fd, 900, 400, V4L2_PIX_FMT_YUYV); // V4L2_PIX_FMT_MJPEG

    memset(&format, 0, sizeof(format));
    format.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    if(-1 == ioctl(fd, VIDIOC_G_FMT, &format)) {
        perror("VIDIOC_G_FMT");
        exit(EXIT_FAILURE);
    }

    printf("Current capture is %u x %u\n", format.fmt.pix.width, format.fmt.pix.height);
    printf("format %4.4s, %u bytes-per-line\n", (char*)&format.fmt.pix.pixelformat, format.fmt.pix.bytesperline);

    return 1;
}

int vid_init() {
    int i, j;

    vw = format.fmt.pix.width;
    vh = format.fmt.pix.height;
    gw = vw;
    gh = vh;

    greysize = gw * gh;
    grey = malloc(greysize);
    printf("Grey buffer is %i bytes\n", greysize);

    memset (&reqbuf, 0, sizeof (reqbuf));
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    reqbuf.memory = V4L2_MEMORY_MMAP;
    reqbuf.count = 2;

    if (-1 == ioctl (fd, VIDIOC_REQBUFS, &reqbuf)) {
        if (errno == EINVAL) {
            printf ("Fatal: Video capturing by mmap-streaming is not supported\n");
        }
        else {
            perror ("VIDIOC_REQBUFS");
        }
        exit (EXIT_FAILURE);
    }

    buffers = calloc (reqbuf.count, sizeof (*buffers));
    if (buffers == NULL){
        printf("calloc failure!");
        exit (EXIT_FAILURE);
    }

    for (i = 0; i < reqbuf.count; i++) {
        memset (&buffer, 0, sizeof (buffer));
        buffer.type = reqbuf.type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;
        if (-1 == ioctl (fd, VIDIOC_QUERYBUF, &buffer)) {
            perror ("VIDIOC_QUERYBUF");
            exit (EXIT_FAILURE);
        }
        buffers[i].length = buffer.length; /* remember for munmap() */
        buffers[i].start = mmap (NULL, buffer.length,
                                 PROT_READ | PROT_WRITE, /* recommended */
                                 MAP_SHARED,             /* recommended */
                                 fd, buffer.m.offset);
        if (MAP_FAILED == buffers[i].start) {
            perror ("mmap");
            exit (EXIT_FAILURE);
        }
    }

    for (i = 0; i < reqbuf.count; i++) {
        memset (&buffer, 0, sizeof (buffer));
        buffer.type = reqbuf.type;
        buffer.memory = V4L2_MEMORY_MMAP;
        buffer.index = i;

        if (-1 == ioctl (fd, VIDIOC_QBUF, &buffer)) {
            perror ("VIDIOC_QBUF");
            exit (EXIT_FAILURE);
        }
    }

    // turn on streaming
    if(-1 == ioctl(fd, VIDIOC_STREAMON, &buftype)) {
        perror("VIDIOC_STREAMON");
        exit(EXIT_FAILURE);
    }
}

char *brichars = " .-:+*o8";

void grab_one () {
    int i, bri;
    int x, y;
    int o;

    if (-1 == ioctl (fd, VIDIOC_DQBUF, &buffer)) {
        perror ("VIDIOC_DQBUF");
        exit (EXIT_FAILURE);
    }

    YUV422_to_grey(buffers[buffer.index].start, grey, vw, vh);
    y = (framenum * 15) % gh;// / 2;
    for(i=0; i<120; i++) {
        x = (i * gw) / 120;
        o = (y * gw) + x;
        bri = (grey[o] * 8 / 256);
        // printf("%d", bri);
        printf("%c", brichars[bri]);
        // printf("%03d %03d %03d\n", grey[0], grey[1], grey[2]);
    }

    if (-1 == ioctl (fd, VIDIOC_QBUF, &buffer)) {
        perror ("VIDIOC_QBUF");
        exit (EXIT_FAILURE);
    }
}


void save_jpeg(char *filename) {
    // bool ipl2jpeg(IplImage *frame, unsigned char **outbuffer, long unsigned int *outlen) {

    // unsigned char *outdata = (uchar *) frame->imageData;
    struct jpeg_compress_struct cinfo = {0};
    struct jpeg_error_mgr jerr;
    JSAMPROW row_ptr[1];
    int row_stride;

    FILE *fp = fopen(filename, "wb");

    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, fp);

    cinfo.image_width = gw;
    cinfo.image_height = gh;
    cinfo.input_components = 1;
    cinfo.in_color_space = JCS_GRAYSCALE; // JCS_RGB;
    jpeg_set_quality(&cinfo, 95, FALSE);

    jpeg_set_defaults(&cinfo);
    jpeg_start_compress(&cinfo, TRUE);
    row_stride = gw * 3;

    while (cinfo.next_scanline < cinfo.image_height) {
        row_ptr[0] = grey + cinfo.next_scanline * gw;// image + (cinfo.next_scanline * row_stride);
 //       jpeg_write_scanlines(&cinfo, row_ptr, 1);
        jpeg_write_scanlines(&cinfo, row_ptr, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    fclose(fp);
}



int main (int argc, char **argv) {
    int i;
    char temp[160];
    struct timeval start, ts;
    float fps;

    if (signal (SIGINT, quitproc) == SIG_ERR) {
        perror ("Couldn't install SIGINT handler");
        exit (1);
    }

    struct stat st;
    if( stat("/dev/video",&st) < 0) {
        strcpy(device,"/dev/video0");
    }
    else {
        strcpy(device,"/dev/video");
    }

    if( vid_detect(device) > 0 ) {
        vid_init();
    } else {
        exit(-1);
    }

    fprintf(stderr,"\n");

    osc_init();

    gettimeofday(&start, NULL);

    while (userbreak <1) {

        printf("%08d: [ ", framenum);

        grab_one ();

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

    }

    if(-1 == ioctl(fd, VIDIOC_STREAMOFF, &buftype)) {
        perror("VIDIOC_STREAMOFF");
    }

    for (i = 0; i < reqbuf.count; i++) {
        munmap (buffers[i].start, buffers[i].length);
    }

    free(grey);
    if(fd>0) close(fd);
    fprintf (stderr, "cya!\n");
    exit (0);
}

void quitproc (int sig) {
    fprintf (stderr, "interrupt caught, exiting.\n");
    userbreak = 1;
}




