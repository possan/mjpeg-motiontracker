#include <curl/curl.h>
#include <jpeglib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

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

int g_state = STATE_MAIN_HEADER;
int jpeg_frame_size = 0;
int jpeg_frame_index = 0;
long first_timestamp = 0;
int jpeg_frame_position = 0;
int jpeg_num_headers = 0;
int block_counter = 0;
char jpeg_filename[100];
FILE *jpeg_file;
unsigned char *jpeg_buffer;
char headerline[1000] = { 0, };

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
	int treshold;
	int trig;
	float percent_motion;
	float treshold_percent;
	// float *average_bitmap;
	// float *last_bitmap;
};

int num_areas;
Area *areas;

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

void check_area_motion(Area *area) {
	// printf("FRAME: check motion in area id #%d...\n", area->id);

	int diff_sum = 0;
	for(int j=0; j<area->height; j++) {
		int o = ((area->y + j) * frame_width + area->x) * 3;
		for(int i=0; i<area->width; i++) {
			diff_sum += diff_frame[o];
			o ++;
		}
	}

	area->value = diff_sum;
	int diff_max = area->width * area->height * 255;
	area->percent_motion = 100.0f * (float)diff_sum / (float)diff_max;
	area->trig = area->percent_motion > area->treshold_percent;

	if (area->trig)
		printf("MOTION: area #%d - diff_sum=%6d %6d %6d percent=%1.2f %1.2f\n", area->id, diff_sum, diff_max, area->treshold, area->percent_motion, area->treshold_percent);
}

void check_frame_motion() {
	// printf("FRAME: check motion...\n");
	for(int i=0; i<num_areas; i++) {
		Area *area = (Area *)&areas[i];
		check_area_motion(area);
	}
}

void rotate_history() {
	memcpy(history2_frame, history_frame, frame_width*frame_height*4);
	memcpy(history_frame, current_frame, frame_width*frame_height*4);
	memset(current_frame, 0, frame_width*frame_height*4);
}

void update_average_and_diff() {
	for(int o=0; o<frame_width*frame_height*3; o++) {
		average_frame[o] = (history_frame[o] + history2_frame[o]) >> 1;
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

void on_frame(unsigned char *ptr, int len) {
	/*
	sprintf(jpeg_filename, "output/frame%08d.jpg", jpeg_frame_index);
	printf("FRAME: %s\n", jpeg_filename);
	FILE *f = fopen(jpeg_filename, "wb");
	fwrite(ptr, 1, len, f);
	fclose(f);
	*/

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
	// printf("Proc: Image is %d by %d with %d components", width, height, pixel_size);
 	int row_stride = cinfo.output_width * cinfo.output_components;
	JSAMPARRAY buffer;
	buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

	// printf("row_stride = %d\n", row_stride);
	// printf("cinfo.output_height = %d\n", cinfo.output_height);

	int o = 0;

	while (cinfo.output_scanline < cinfo.output_height) {
		jpeg_read_scanlines(&cinfo, buffer, 1);
		// put_scanline_someplace(buffer[0], row_stride);
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

	// jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	// fclose(infile);

	update_average_and_diff();
	check_frame_motion();

	if (jpeg_frame_index % 100 == 99) {
		int t = (int)time(NULL);
		float fps = (float)jpeg_frame_index / ((float)(t - first_timestamp));
		printf("%1.1f FPS after %d frames.\n", fps, jpeg_frame_index);
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
	CURL *curl = curl_easy_init();
	// FILE *fp = fopen("trans.txt","wb");
	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_data);
	// curl_easy_setopt(curl, CURLOPT_WRITEDATA, fp);
	CURLcode res = curl_easy_perform(curl);
	curl_easy_cleanup(curl);
	// fclose(fp);
}

int main() {
	num_areas = 3;
	areas = (Area *)malloc(sizeof(Area) * num_areas);

	Area *area = (Area *)&areas[0];
	area->id = 1;
	area->x = 0;
	area->y = 0;
	area->width = 20;
	area->height = 20;
	area->value = 0;
	area->treshold = 200;
	area->treshold_percent = 5.0f;

	area = (Area *)&areas[1];
	area->id = 2;
	area->x = 160 - 20;
	area->y = 90 - 20;
	area->width = 40;
	area->height = 40;
	area->value = 0;
	area->treshold = 200;
	area->treshold_percent = 5.0f;

	area = (Area *)&areas[2];
	area->id = 3;
	area->x = 320 - 20;
	area->y = 180 - 20;
	area->width = 20;
	area->height = 20;
	area->value = 0;
	area->treshold = 200;
	area->treshold_percent = 5.0f;

	jpeg_buffer = (unsigned char *)malloc(5*1024*1024);
	first_timestamp = (int)time(NULL);
	while(true) {
		g_state = STATE_MAIN_HEADER;
		jpeg_num_headers = 0;
		// stream("http://195.235.198.107:3346/axis-cgi/mjpg/video.cgi?resolution=320x240");
		stream("http://192.168.1.3:8080/?action=stream");
	}
	free(jpeg_buffer);
}
