#include <curl/curl.h>
#include <jpeglib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

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

int debug_output_frame = 0;
int g_state = STATE_MAIN_HEADER;
int jpeg_frame_size = 0;
int jpeg_frame_index = 0;

int jpeg_every = 1;

int fps_frame = 0;
long fps_start = 0;

int jpeg_frame_position = 0;
int jpeg_num_headers = 0;
int block_counter = 0;
char jpeg_filename[100];
FILE *jpeg_file;
unsigned char *jpeg_buffer;
char headerline[1000] = { 0, };
char stream_url[1000];
int debug_image_written = false;

int frame_width = 0;
int frame_height = 0;
unsigned char *history4_frame = NULL;
unsigned char *history3_frame = NULL;
unsigned char *history2_frame = NULL;
unsigned char *history_frame = NULL;
unsigned char *current_frame = NULL;
unsigned char *average_frame = NULL;
unsigned char *diff_frame = NULL;

struct Area {
    int id;
    int x;
    int y;
    int width;
    int height;
    int value;
    int trig;
    float percent_motion;
    float treshold_percent;
    char *message;
    float percent_history[100];
    int trig_history[100];
};

int num_areas;
Area *areas;

char osc_ip[100] = "127.0.0.1";
int osc_port = 8000;
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

void save_jpeg(unsigned char *image, char *filename) {
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

    cinfo.image_width = frame_width;
    cinfo.image_height = frame_height;
    cinfo.input_components = 3;
    cinfo.in_color_space = JCS_RGB;
    jpeg_set_quality(&cinfo, 95, FALSE);

    jpeg_set_defaults(&cinfo);
    jpeg_start_compress(&cinfo, TRUE);
    row_stride = frame_width * 3;

    while (cinfo.next_scanline < cinfo.image_height) {
        row_ptr[0] = image + (cinfo.next_scanline * row_stride);
        jpeg_write_scanlines(&cinfo, row_ptr, 1);
    }

    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);

    fclose(fp);
}


void check_frame_size(int new_width, int new_height) {
    if (new_width == frame_width && new_height == frame_height)
        return;

    printf("FRAME: resize buffers to %d x %d\n", new_width, new_height);

    frame_width = new_width;
    frame_height = new_height;

    if (history4_frame != NULL) {
        free(history4_frame);
        history4_frame = NULL;
    }

    if (history3_frame != NULL) {
        free(history3_frame);
        history3_frame = NULL;
    }

    if (history_frame != NULL) {
        free(history_frame);
        history_frame = NULL;
    }

    if (current_frame != NULL) {
        free(current_frame);
        current_frame = NULL;
    }

    if (average_frame != NULL) {
        free(average_frame);
        average_frame = NULL;
    }

    if (diff_frame != NULL) {
        free(diff_frame);
        diff_frame = NULL;
    }

    history4_frame = (unsigned char *)malloc(3 * frame_width * frame_height);
    history3_frame = (unsigned char *)malloc(3 * frame_width * frame_height);
    history2_frame = (unsigned char *)malloc(3 * frame_width * frame_height);
    history_frame = (unsigned char *)malloc(3 * frame_width * frame_height);
    current_frame = (unsigned char *)malloc(3 * frame_width * frame_height);
    average_frame = (unsigned char *)malloc(3 * frame_width * frame_height);
    diff_frame = (unsigned char *)malloc(3 * frame_width * frame_height);
}

char *shades = (char *)" .,:oO#";

void check_area_motion(Area *area) {
    // printf("FRAME: check motion in area id #%d...\n", area->id);
    area->trig = 0;

    bool debug = false;// area->id == 1;

    // for directional support compare diff against x/y-offset average images and pick the one with least diff.

    int diff_sum = 0;
    for(int j=0; j<area->height; j++) {
        int o = ((area->y + j) * frame_width + area->x) * 3;
        for(int i=0; i<area->width; i++) {
            diff_sum += (diff_frame[o] + diff_frame[o+1] + diff_frame[o+2]) / 3;
            // diff_sum += (current_frame[o] - average_frame[o]);
            if (debug) {
                int sh = (diff_frame[o] * 7) / 255;
                printf("%c", shades[sh]);
            }
            o += 3;
        }
        if (debug) printf("\n");
    }

    area->value = diff_sum;
    int diff_max = area->width * area->height * 255;
    area->percent_motion = 100.0f * (float)diff_sum / (float)diff_max;

    if (area->percent_motion > area->treshold_percent) {
        area->trig = 1;
    }

    for(int i=99; i>=1; i--) {
        area->percent_history[i] = area->percent_history[i-1];
        area->trig_history[i] = area->trig_history[i-1];
    }
    area->percent_history[0] = area->percent_motion;
    area->trig_history[0] = area->trig;

    if (area->trig) {
        // printf("MOTION: area triggered - diff_sum=%6d/%6d percent=%1.2f %1.2f\n", diff_sum, diff_max, area->percent_motion, area->treshold_percent);
        osc_send(area->message, area->percent_motion);
    }
}

void highlight_areas(unsigned char *temp) {
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
}

void check_frame_motion() {
    // printf("FRAME: check motion...\n");
    for(int i=0; i<num_areas; i++) {
        Area *area = (Area *)&areas[i];
        check_area_motion(area);
    }

    if (!debug_image_written) {
        printf("DEBUG: Writing zone debug jpeg.\n");
        // write a debug image containing the loaded zones.
        unsigned char *temp = (unsigned char *)malloc(frame_width * frame_height * 3);
        memcpy(temp, average_frame, frame_width * frame_height * 3);
        highlight_areas(temp);
        save_jpeg(temp, (char *)"output/debug.jpg");
        free(temp);
        debug_image_written = true;
    }
}

void rotate_history() {
    memcpy(history4_frame, history3_frame, frame_width*frame_height*3);
    memcpy(history3_frame, history2_frame, frame_width*frame_height*3);
    memcpy(history2_frame, history_frame, frame_width*frame_height*3);
    memcpy(history_frame, current_frame, frame_width*frame_height*3);
    memset(current_frame, 0, frame_width*frame_height*3);
}

void update_average_and_diff() {
    for(int o=0; o<frame_width*frame_height*3; o++) {
        // average_frame[o] = (history_frame[o] + history2_frame[o] + history3_frame[o] + history4_frame[o]) >> 2;
        average_frame[o] = (history_frame[o] + history2_frame[o]) >> 1;// + history3_frame[o] + history4_frame[o]) >> 2;
    }

    for(int o=0; o<frame_width*frame_height*3; o++) {
        diff_frame[o] = abs(current_frame[o] - average_frame[o]);
    }
}

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
    check_frame_size(width, height);
    rotate_history();

    int pixel_size = cinfo.output_components;
    int row_stride = cinfo.output_width * cinfo.output_components;
    JSAMPARRAY buffer;
    buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

    int o = 0;
    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        int oi = 0;
        int oo = cinfo.output_scanline * frame_width * 3;
        unsigned char *bytebuffer = buffer[0];
        for(int c=0; c<cinfo.output_width; c++) {
            if (cinfo.output_components == 3) {
                // R
                current_frame[oo] = bytebuffer[oi];
                oi ++;
                oo ++;
                // G
                current_frame[oo] = bytebuffer[oi];
                oi ++;
                oo ++;
                // B
                current_frame[oo] = bytebuffer[oi];
                oi ++;
                oo ++;
            }
        }
    }

    jpeg_finish_decompress(&cinfo);
    jpeg_destroy_decompress(&cinfo);

    update_average_and_diff();

    if (jpeg_frame_index > 10) {
        check_frame_motion();
    } else {
        printf("MOTION: Skipping frame #%d\n", jpeg_frame_index);
    }

    // if (jpeg_frame_index % 10 == 0) {
    for(int i=0; i<num_areas; i++) {
        Area *area = (Area *)&areas[i];
        if (area->trig)
            printf("\x1B[32m%3.2f %%   ", area->percent_motion);
        else
            printf("\x1B[31m%3.2f %%   ", area->percent_motion);
    }
    printf("\x1B[37m\n");

}

void on_frame(unsigned char *ptr, int len) {

    if (jpeg_frame_index % jpeg_every == 0) {
        decode_into_current(ptr, len);
    }

    if ((fps_frame % 50) == 49) {
        int t = (int)time(NULL);
        float fps = (float)jpeg_frame_index / ((float)(t - fps_start));
        printf("%1.1f FPS after %d frames.\n", fps, fps_frame);
    }
    fps_frame ++;

    if ((jpeg_frame_index % 100) == 30) {
        // sprintf(jpeg_filename, "output/frame%04d.jpg", debug_output_frame % 50);
        sprintf(jpeg_filename, "output/frame.jpg");
        printf("FRAME: Saving input: %s\n", jpeg_filename);
        FILE *f = fopen(jpeg_filename, "wb");
        fwrite(ptr, 1, len, f);
        fclose(f);
    }

    if ((jpeg_frame_index % 100) == 60) {
        // sprintf(jpeg_filename, "output/frame%04d-diff.jpg", debug_output_frame % 50);
        sprintf(jpeg_filename, "output/frame-diff.jpg");
        printf("FRAME: Saving diff: %s\n", jpeg_filename);
        highlight_areas(diff_frame);
        save_jpeg(diff_frame, jpeg_filename);
    }

    if ((jpeg_frame_index % 100) == 90) {
        // sprintf(jpeg_filename, "output/frame%04d-avg.jpg", debug_output_frame % 50);
        sprintf(jpeg_filename, "output/frame-avg.jpg");
        printf("FRAME: Saving average: %s\n", jpeg_filename);
        highlight_areas(average_frame);
        save_jpeg(average_frame, jpeg_filename);

        // debug_output_frame ++;
    }

    jpeg_frame_index ++;
}



size_t write_data(void *ptr, size_t size, size_t nmemb, FILE *stream) {
    size_t written;
    // unsigned char nbytes = size * nmemb;
    // printf("got %d bytes...\n", nbytes);

    for(int i=0; i<nmemb; i++) {
        unsigned char b = ((unsigned char *)ptr)[i];
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
            jpeg_buffer[jpeg_frame_position] = b;
            jpeg_frame_position ++;
            // printf("position %d / %d\n", jpeg_frame_position, jpeg_frame_size);
            if (jpeg_frame_position >= jpeg_frame_size) {
                // printf("end of frame.\n");
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
    return nmemb;
}

void stream(char *url) {
    printf("STREAM: Downloading %s", url);
    g_state = STATE_MAIN_HEADER;
    fps_start = (int)time(NULL);
    fps_frame = 0;
    jpeg_num_headers = 0;
    CURL *curl = curl_easy_init();
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_VERBOSE, 1);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT, 10000);
    curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME, 3);
    CURLcode res = curl_easy_perform(curl);
    printf("STREAM: Transfer done, status code %d\n", res);
    curl_easy_cleanup(curl);
}

void handle_config_token(char *token) {
    printf("CONFIG: Token: \"%s\"\n", token);
}

bool read_config(char *filename) {
    printf("CONFIG: Loading \"%s\"...\n", filename);
    char buf[200];
    char buf2[200];
    FILE *f = fopen(filename, "rt");
    if (f == NULL) {
        printf("CONFIG: Unable to read config.\n");
        return false;
    }
    while(fgets(buf, 200, f) != NULL) {
        // printf("CONFIG: Line: \"%s\"\n", buf);
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
                strcpy(osc_ip, str_host);
                osc_port = atoi(str_port);
            }
        }
        else if (strcmp(cmd, "area") == 0) {
            char *str_x = strsep(&ptr, " \n\r");
            char *str_y = strsep(&ptr, " \n\r");
            char *str_w = strsep(&ptr, " \n\r");
            char *str_h = strsep(&ptr, " \n\r");
            char *str_treshold = strsep(&ptr, " \n\r");
            char *str_message = strsep(&ptr, " \n\r");
            if (str_x != NULL && str_y != NULL && str_w != NULL && str_h != NULL && str_treshold != NULL && str_message != NULL) {
                Area *area = (Area *)&areas[num_areas];
                area->id = 0;
                area->x = atoi(str_x);
                area->y = atoi(str_y);
                area->width = atoi(str_w);
                area->height = atoi(str_h);
                area->value = 0;
                area->treshold_percent = atof(str_treshold);
                area->message = strdup(str_message);
                num_areas ++;
                printf("CONFIG: Creating area #%d at %d,%d size %dx%d treshold %1.1f, message \"%s\"\n",
                    num_areas,
                    area->x,
                    area->y,
                    area->width,
                    area->height,
                    area->treshold_percent,
                    area->message);
            }
        }
        else if (strcmp(cmd, "source") == 0) {
            char *str_url = strsep(&ptr, " \n\r");
            if (str_url != NULL) {
                strcpy(stream_url, str_url);
            }
        }
        else if (strcmp(cmd, "every") == 0) {
            char *str_every = strsep(&ptr, " \n\r");
            if (str_every != NULL) {
                jpeg_every = atoi(str_every);
                printf("CONFIG: Only using every %d frame.\n", jpeg_every);
            }
        }
    }
    fclose(f);
    return true;
}

int main(int argc, char *argv[]) {

    if (argc != 2) {
        printf("Syntax: mm [config.txt]\n");
        return 1;
    }

    areas = (Area *)malloc(sizeof(Area) * 100);
    num_areas = 0;

    if (!read_config(argv[1])) {
        return 1;
    }

    osc_init();

    jpeg_buffer = (unsigned char *)malloc(5*1024*1024);
    while(true) {
        stream(stream_url);
        printf("Waiting a bit before reconnecting...\n");
        sleep(3);
    }
    free(jpeg_buffer);
}
