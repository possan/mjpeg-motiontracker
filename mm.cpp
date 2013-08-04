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
int jpeg_frame_position = 0;
int jpeg_num_headers = 0;
int block_counter = 0;
char jpeg_filename[100];
FILE *jpeg_file;
unsigned char *jpeg_buffer;
char headerline[1000] = { 0, };

void on_headerline(char *buf) {
	printf("HEADER: %s\n", buf);
	if (strncmp(buf, "Content-Length:", 15) == 0) {
		jpeg_frame_size = atoi(buf + 16);
		printf("HEADER: Got content length = %d bytes\n", jpeg_frame_size);
	}
	if (strncmp(buf, "Content-Type:", 13) == 0) {
		printf("HEADER: Got content type = %s\n", buf + 14);
	}
}

void on_frame(unsigned char *ptr, int len) {
	sprintf(jpeg_filename, "output/frame%08d.jpg", jpeg_frame_index);
	printf("FRAME: %s\n", jpeg_filename);
	FILE *f = fopen(jpeg_filename, "wb");
	fwrite(ptr, 1, len, f);
	fclose(f);

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
	int pixel_size = cinfo.output_components;
	printf("Proc: Image is %d by %d with %d components", width, height, pixel_size);

 	/* JSAMPLEs per row in output buffer */
 	int row_stride = cinfo.output_width * cinfo.output_components;

	JSAMPARRAY buffer;
	buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

	printf("row_stride = %d\n", row_stride);
	printf("cinfo.output_height = %d\n", cinfo.output_height);

	while (cinfo.output_scanline < cinfo.output_height) {
		(void) jpeg_read_scanlines(&cinfo, buffer, 1);
		// put_scanline_someplace(buffer[0], row_stride);

		
	}

	// jpeg_finish_decompress(&cinfo);
	jpeg_destroy_decompress(&cinfo);
	// fclose(infile);
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
				jpeg_frame_index ++;
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
	jpeg_buffer = (unsigned char *)malloc(5*1024*1024);
	while(true) {
		g_state = STATE_MAIN_HEADER;
		jpeg_num_headers = 0;
		// stream("http://195.235.198.107:3346/axis-cgi/mjpg/video.cgi?resolution=320x240");
		stream("http://192.168.1.3:8080/?action=stream");
	}
	free(jpeg_buffer);
}
