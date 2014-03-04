#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>
#include <getopt.h>
#include <errno.h>
#include <fcntl.h>
#include <ctype.h>
#include "bitmap.h"
#include "capture.h"
#include <curl/curl.h>
#include <jpeglib.h>

// http://stackoverflow.com/questions/4801933/how-to-parse-mjpeg-http-stream-within-c

// http header from
// 200 OK\r\n
// ...x\r\n
// content-type ...;boundary=xxx\r\n
// \r\n
// --xxx\r\n
// content-type: image/jpeg\r\n
// content-length: 123\r\n
// \r\n
// data\r\n
// --xxx\r\n
// ...\r\n

#define STATE_FRAME 0
#define STATE_MAIN_HEADER 30
#define STATE_FRAME_HEADER 36

#define JPEG_BUFFER_SIZE 5*1024*1024

BITMAP *bmp;

int debug_output_frame = 0;
int g_state = STATE_MAIN_HEADER;
int jpeg_frame_size = 0;
int jpeg_frame_index = 0;

int jpeg_every = 1;

int jpeg_frame_position = 0;
int jpeg_num_headers = 0;
int block_counter = 0;
char jpeg_filename[100];
FILE *jpeg_file;
unsigned char *jpeg_buffer;
char headerline[1000] = { 0, };
char stream_url[1000];
char stream_auth[100];
int debug_image_written = false;

int frame_width = 0;
int frame_height = 0;

int fps_frame = 0;
long fps_start = 0;

CURL *curl = NULL;

CaptureFrameCallback current_callback;

void on_headerline(char *buf) {
    // printf("HEADER: %s\n", buf);
    if (strncmp(buf, "Content-Length:", 15) == 0) {
        jpeg_frame_size = atoi(buf + 16);
        // printf("HEADER: Got content length = %d bytes\n", jpeg_frame_size);
    }
    if (strncmp(buf, "Content-Type:", 13) == 0) {
        // printf("HEADER: Got content type = %s\n", buf + 14);
    }
}

void decode_into_current(unsigned char *ptr, int len) {

    JSAMPROW row_pointer[1];
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;
    memset(&cinfo, 0, sizeof(jpeg_decompress_struct));
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_decompress(&cinfo);
    jpeg_mem_src(&cinfo, ptr, len);
    int rc = jpeg_read_header(&cinfo, TRUE);
    if (rc != 1) {
        printf("File does not seem to be a normal JPEG\n");
        return;
    }

    jpeg_start_decompress(&cinfo);
    int width = cinfo.output_width;
    int height = cinfo.output_height;
    // check_frame_size(width, height);

    if (bmp == NULL) {
        bmp = bitmap_init(width, height, 1);
    }
    bitmap_resize(bmp, width, height);

    int pixel_size = cinfo.output_components;
    int row_stride = cinfo.output_width * cinfo.output_components;
    JSAMPARRAY buffer;
    buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

    int o = 0;
    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        int oi = 0;
        int oo = cinfo.output_scanline * bmp->stride;
        unsigned char *bytebuffer = buffer[0];
        for(int c=0; c<cinfo.output_width; c++) {
            if (pixel_size == 3) {
                bmp->buffer[oo] = (bytebuffer[oi] + bytebuffer[oi+1] + bytebuffer[oi+2]) / 3;
                oi += 3;
                oo ++;
            }
        }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);
}

void on_frame(unsigned char *ptr, int len) {

    if (jpeg_frame_index % jpeg_every == 0) {
        decode_into_current(ptr, len);
        current_callback(bmp);
    }

    jpeg_frame_index ++;
}

long total_bytes = 0;

size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written;

    unsigned char *bptr = (unsigned char *)ptr;

    long nbytes = size * nmemb;
    printf("got %d bytes (%d, %d) (total %d)...\n", nbytes, size, nmemb, total_bytes);

    total_bytes += nbytes;

    for(int i=0; i<nbytes; i++) {
        unsigned char b = bptr[i];
        // printf("byte #%d.%d %02x (%c)\n", block_counter, i, b, (b > 32 && b < 128) ? b : '?');
        // printf("%c", (b > 32 && b < 128) ? b : '?');
        if (g_state == STATE_MAIN_HEADER || g_state == STATE_FRAME_HEADER) {
            int p = strlen(headerline);
            if (p < 1000)
                headerline[p] = b;
            if (b == '\n') {
                if (headerline[p-1] == '\r') {
                    headerline[p] = 0;
                    headerline[p-1] = 0;
                    // printf("got header newline after \"%s\".\n", headerline);
                    on_headerline(headerline);
                    if (strncmp(headerline, "--", 2) == 0) {
                        // printf("got boundary.\n");
                        g_state = STATE_FRAME_HEADER;
                    }
                    if (strlen(headerline) == 0 && jpeg_num_headers > 0) {
                        // did we get an empty line, switch state.
                        // printf("empty header, switch state.\n");
                        if (g_state == STATE_FRAME_HEADER) {
                            // printf("starting new jpeg frame.\n");
                            g_state = STATE_FRAME;
                            jpeg_frame_position = 0;
                        }
                    }
                    memset(&headerline, 0, 1000);
                    jpeg_num_headers ++;
                }
            }
        }
        else if (g_state == STATE_FRAME) {
            if (jpeg_frame_position < JPEG_BUFFER_SIZE) {
                jpeg_buffer[jpeg_frame_position] = b;
            }
            jpeg_frame_position ++;
            if (jpeg_frame_position >= jpeg_frame_size) {
                printf("position %d / %d\n", jpeg_frame_position, jpeg_frame_size);
                printf("end of frame.\n");
                on_frame(jpeg_buffer, jpeg_frame_size);
                g_state = STATE_FRAME_HEADER;
                memset(headerline, 0, 1000);
                jpeg_frame_position = 0;
                jpeg_num_headers = 0;
            }
        }
    }

    // written = fwrite(ptr, size, nmemb, stream);

    block_counter ++;
    return nbytes;
}

class MotionJpegCapture : public ICapture {
public:
	MotionJpegCapture(const char *url, const char *auth);
    ~MotionJpegCapture();
    bool block(CaptureFrameCallback callback);
};

MotionJpegCapture::MotionJpegCapture(const char *url, const char *auth) {
    printf("MJPEG CAPTURE: Start capturing from %s\n", url);

    strcpy(stream_url, url);
    strcpy(stream_auth, "");
    if (auth != NULL) {
        strcpy(stream_auth, auth);
    }

    fps_start = (int)time(NULL);
    fps_frame = 0;

    jpeg_buffer = (unsigned char *)malloc(JPEG_BUFFER_SIZE);
}

MotionJpegCapture::~MotionJpegCapture() {
    if (curl != NULL) {
        curl_easy_cleanup(curl);
        curl = NULL;
    }
    if (bmp != NULL) {
        bitmap_free(bmp);
        bmp = NULL;
    }
    free(jpeg_buffer);
}

bool MotionJpegCapture::block(CaptureFrameCallback callback) {
    current_callback = callback;
    g_state = STATE_MAIN_HEADER;
    jpeg_num_headers = 0;
    curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, stream_url);
    if (strlen(stream_auth) > 0) {
        curl_easy_setopt(curl, CURLOPT_USERPWD, stream_auth);
    }
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 0); // no stream timeout
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 5000);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 10000);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 3);
    CURLcode res = curl_easy_perform(curl);
    printf("STREAM: Transfer done, status code %d\n", res);
    curl_easy_cleanup(curl);
    curl = NULL;
    // stream(stream_url);
    printf("Waiting a bit before reconnecting...\n");
    sleep(2);
    return TRUE;
}

ICapture *create_mjpeg_capture(const char *url, const char *auth) {
	return new MotionJpegCapture(url, auth);
}
