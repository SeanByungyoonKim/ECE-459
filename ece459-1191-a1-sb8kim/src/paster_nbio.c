/*
 * Paste together PNG files downloaded from the network.
 *
 * Downloads a bunch of PNG files from BASE_URL and concatenates them.
 *
 * Derived from curl and libpng base code.
 * curl examples are from simple.c included with curl distribution:
 * Copyright (C) 1998 - 2013, Daniel Stenberg, <daniel@haxx.se>, et al.
 * libpng examples are from http://zarb.org/~gc/html/libpng.html
 * Copyright 2002-2011 Guillaume Cottenceau and contributors.
 * 
 * Modifications to integrate the code are
 * Copyright 2013 Patrick Lam.
 *
 * This software may be freely redistributed under the terms
 * of the X11 license.
 *
 */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <stdbool.h>

#define PNG_DEBUG 3
#include <png.h>
#include <curl/curl.h>

struct bufdata {
  png_bytep buf;
  int len, pos;
  size_t max_size;
};

#define N 20
#define WIDTH 4000
#define HEIGHT 3000

// #define BASE_URL "http://berkeley.uwaterloo.ca:4590/image?img=%d"
#define BASE_URL "ece459-%d.patricklam.ca:4590/image?img=%d"
#define BUF_WIDTH WIDTH/N
#define BUF_HEIGHT HEIGHT
#define BUF_SIZE 10485760
#define ECE459_HEADER "X-Ece459-Fragment: "

#define MAX_WAIT 30*1000 //10 seconds

/* error handling macro */
void abort_(const char * s, ...)
{
  va_list args;
  va_start(args, s);
  vfprintf(stderr, s, args);
  fprintf(stderr, "\n");
  va_end(args);
  abort();
}

/***********************************************************************************/
/* routines to parse PNG data and copy it to an internal buffer */

void read_cb (png_structp png_ptr, png_bytep outBytes, png_size_t byteCountToRead);

/* Given PNG-formatted data at bd, read the data into a buffer that we allocate 
 * and return (row_pointers, here).
 *
 * Note: caller must free the returned value. */
png_bytep* read_png_file(png_structp png_ptr, png_infop * info_ptr, struct bufdata * bd)
{
  int y;

  int height;
  png_byte bit_depth;

  png_bytep * row_pointers;

  if (png_sig_cmp(bd->buf, 0, 8))
    abort_("[read_png_file] Input is not recognized as a PNG file");
    
  *info_ptr = png_create_info_struct(png_ptr);
  if (!*info_ptr)
    abort_("[read_png_file] png_create_info_struct failed");
  
  if (setjmp(png_jmpbuf(png_ptr)))
    abort_("[read_png_file] Error during init_io");

  bd->pos = 0;
  png_set_read_fn(png_ptr, bd, read_cb);
  png_read_info(png_ptr, *info_ptr);
  height = png_get_image_height(png_ptr, *info_ptr);
  bit_depth = png_get_bit_depth(png_ptr, *info_ptr);
  if (bit_depth != 8)
    abort_("[read_png_file] bit depth 16 PNG files unsupported");
  
  if (setjmp(png_jmpbuf(png_ptr)))
    abort_("[read_png_file] Error during read_image");

  row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * BUF_HEIGHT);
  for (y=0; y<height; y++)
    row_pointers[y] = (png_byte*) malloc(png_get_rowbytes(png_ptr, *info_ptr));
  
  png_read_image(png_ptr, row_pointers);

  return row_pointers;
}

/* libpng calls this (at read_png_data's request)
 * to copy data from the in-RAM PNG into our bitmap */
void read_cb (png_structp png_ptr, png_bytep outBytes, png_size_t byteCountToRead) {
  struct bufdata * bd = png_get_io_ptr(png_ptr);

  if (bd == NULL)
    abort_("[read_png_file/read_cb] invalid memory passed to png reader");
  if (bd->pos + byteCountToRead >= bd->len)
    abort_("[read_png_file/read_cb] attempting to read beyond end of buffer");

  memcpy(outBytes, bd->buf+bd->pos, byteCountToRead);
  bd->pos += byteCountToRead;
}

/* copy from row_pointers data array to dest data array, at offset (x0, y0) */
void paint_destination(png_structp png_ptr, png_bytep * row_pointers, 
           int x0, int y0, png_byte* dest)
{
  int x, y, i;
  
  for (y=0; y<BUF_HEIGHT && (y0+y) < HEIGHT; y++) {
    png_byte* row = row_pointers[y];
    for (x=0; x<BUF_WIDTH; x++) {
      png_byte* ptr = &(row[x*4]);
      int index = ((y0+y)*WIDTH+(x0+x))*4;
      for (i = 0; i < 4; i++)
  dest[index+i] = ptr[i];
    }
  }
}

/***********************************************************************************/
/* routine used by curl to read from the network                                   */

/* curl calls this to transfer data from the network into RAM */
size_t write_cb(char * ptr, size_t size, size_t nmemb, void *userdata) {
  struct bufdata * bd = userdata;

  if (size * nmemb >= bd->max_size) {
    return 0;
  }

  memcpy(bd->buf+bd->pos, ptr, size * nmemb);
  bd->pos += size*nmemb;
  bd->len += size*nmemb;
  return size * nmemb;
}

/***********************************************************************************/
/* routine to write data to disk                                                   */

/* write output_row_pointers back to PNG file as specified by file_name. */
void write_png_file(char* file_name, png_bytep * output_row_pointers)
{
  png_structp png_ptr;
  png_infop info_ptr;

  /* create file */
  FILE *fp = fopen(file_name, "wb");
  if (!fp)
    abort_("[write_png_file] File %s could not be opened for writing", file_name);
  
  
  /* initialize stuff */
  png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  
  if (!png_ptr)
    abort_("[write_png_file] png_create_write_struct failed");
  
  info_ptr = png_create_info_struct(png_ptr);
  if (!info_ptr)
    abort_("[write_png_file] png_create_info_struct failed");
  
  if (setjmp(png_jmpbuf(png_ptr)))
    abort_("[write_png_file] Error during init_io");
  
  png_init_io(png_ptr, fp);
  
  /* write header */
  if (setjmp(png_jmpbuf(png_ptr)))
    abort_("[write_png_file] Error during writing header");
  
  png_set_IHDR(png_ptr, info_ptr, WIDTH, HEIGHT,
         8, 6, PNG_INTERLACE_NONE,
         PNG_COMPRESSION_TYPE_BASE, PNG_FILTER_TYPE_BASE);
  
  png_write_info(png_ptr, info_ptr);
  
  /* write bytes */
  if (setjmp(png_jmpbuf(png_ptr)))
    abort_("[write_png_file] Error during writing bytes");
  
  png_write_image(png_ptr, output_row_pointers);  
  
  if (setjmp(png_jmpbuf(png_ptr)))
    abort_("[write_png_file] Error during end of write");
  
  png_write_end(png_ptr, NULL);
  png_destroy_write_struct(&png_ptr, &info_ptr);    
  fclose(fp);
}

struct headerdata {
  int n;
  bool * received_fragments;
};

size_t header_cb (char * buf, size_t size, size_t nmemb, void * userdata)
{
  struct headerdata * hd = userdata;
  int bytes_in_header = size * nmemb;

  if (bytes_in_header > strlen(ECE459_HEADER) && strncmp(buf, ECE459_HEADER, strlen(ECE459_HEADER)) == 0) {
    // one ought to check that buf is 0-terminated
    //  not guaranteed by spec (!)
    hd->n = atoi(buf+strlen(ECE459_HEADER));
    hd->received_fragments[hd->n] = true;
    printf("received fragment %d\n", hd->n);
  }
  return bytes_in_header;
}

#define NUM_THREADS 64
struct bufdata bd_arr[NUM_THREADS];
struct headerdata hd_arr[NUM_THREADS];
CURL *handles[NUM_THREADS];

void init(CURLM* cm, int i, struct bufdata bd, struct headerdata hd, CURL *handle) {
  char* url = malloc(sizeof(char)*strlen(BASE_URL)+4*5);
  int img = 1;
  curl_easy_setopt(handles[i], CURLOPT_WRITEFUNCTION, write_cb);
  curl_easy_setopt(handles[i], CURLOPT_WRITEDATA, &bd_arr[i]);

  curl_easy_setopt(handles[i], CURLOPT_HEADERDATA, &hd_arr[i]);
  curl_easy_setopt(handles[i], CURLOPT_HEADERFUNCTION, header_cb);

  sprintf(url, BASE_URL, i%3+1, img);
  curl_easy_setopt(handles[i], CURLOPT_URL, url); 
  curl_multi_add_handle(cm, handles[i]);
  printf("CURL %d is requesting URL %s\n", i, url);
  free(url);
}

/***********************************************************************************/

int main(int argc, char **argv)
{
  int c;
  int img = 1;
  int num_threads = 32;
  bool received_all_fragments = false;
  bool * received_fragments = calloc(N, sizeof(bool));

  while ((c = getopt (argc, argv, "t:i:")) != -1) {
    switch (c) {
    case 't':
      num_threads = strtoul(optarg, NULL, 10);
      if (num_threads == 0) {
  printf("%s: option requires an argument > 0 -- 't'\n", argv[0]);
  return -1;
      }
      break;
    case 'i':
      img = strtoul(optarg, NULL, 10);
      if (img == 0) {
  printf("%s: option requires an argument > 0 -- 'i'\n", argv[0]);
  return -1;
      }
      break;
    default:
      return -1;
    }
  }

  CURLM *mult = NULL;
  CURL *curl;
  CURLMsg *msg; 
  CURLcode ret_code = 0;
  int still_running = 0;
  int msgs_left;

  png_structp png_ptr;
  png_infop info_ptr;
  
  png_byte * output_buffer = calloc(WIDTH*HEIGHT*4, sizeof(png_byte));

  curl_global_init(CURL_GLOBAL_NOTHING);
  mult = curl_multi_init();

  for (int i = 0; i < NUM_THREADS; i++) {
    handles[i] = curl_easy_init();
    bd_arr[i].buf = malloc(sizeof(png_byte)*BUF_SIZE);
    bd_arr[i].len = bd_arr[i].pos = 0; bd_arr[i].max_size = BUF_SIZE;
    hd_arr[i].received_fragments = received_fragments;
    init(mult, i, bd_arr[i], hd_arr[i], handles[i]);
  }

  while (!received_all_fragments) {
    curl_multi_perform(mult, &still_running);
    printf("Curls started running!\n");
    while (still_running > 0) {
      int numfds = 0;
      int res = curl_multi_wait(mult, NULL, 0, MAX_WAIT, &numfds);

      if (res != CURLM_OK) {
        fprintf(stderr, "error:_curl_multi_wait()_returned_%d\n", res);
        return EXIT_FAILURE;
      }
      
      while ( (msg = curl_multi_info_read(mult, &msgs_left) ) ) {
        if (msg->msg == CURLMSG_DONE) {
          curl = msg->easy_handle; 
          ret_code = msg->data.result;
          if (ret_code != CURLE_OK) {
            fprintf(stderr, "CURL_error_code: %d\n", msg->data.result);
            continue; 
          }

          int j; 
          for (j = 0; j < NUM_THREADS; j++) {
            if (curl == handles[j]) {
              printf("j: %d\n", j);
              break;
            }
          }

          struct bufdata bd = bd_arr[j]; 
          struct headerdata hd = hd_arr[j];   

          png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
          if (!png_ptr)
            abort_("[main] png_create_read_struct failed");

          // read PNG (as downloaded from network) and copy it to output buffer
          png_bytep* row_pointers = read_png_file(png_ptr, &info_ptr, &bd);
          paint_destination(png_ptr, row_pointers, hd.n*BUF_WIDTH, 0, output_buffer);

          // reset input buffer
          bd_arr[j].len = bd_arr[j].pos = 0; bd_arr[j].max_size = BUF_SIZE;

          // free allocated memory
          for (int y=0; y<BUF_HEIGHT; y++)
            free(row_pointers[y]);
          free(row_pointers);
          png_destroy_read_struct(&png_ptr, &info_ptr, NULL); 
          printf("saved to buffer\n");

          received_all_fragments = true;
          for (int k = 0; k < N; k++) {
            if (!received_fragments[k])
              received_all_fragments = false;
          }

          curl_multi_remove_handle(mult, handles[j]);
          curl_easy_cleanup(handles[j]);
          if (!received_all_fragments) {
            handles[j] = curl_easy_init();
            // bd_arr[j].buf = malloc(sizeof(png_byte)*BUF_SIZE);
            bd_arr[j].len = bd_arr[j].pos = 0; bd_arr[j].max_size = BUF_SIZE;
            hd_arr[j].received_fragments = received_fragments;
            init(mult, j, bd_arr[j], hd_arr[j], handles[j]);
          }
        }
      }    
      curl_multi_perform(mult, &still_running);
    }
  }

  // now, write the array back to disk using write_png_file
  png_bytep * output_row_pointers = (png_bytep*) malloc(sizeof(png_bytep) * HEIGHT);

  for (int i = 0; i < HEIGHT; i++)
    output_row_pointers[i] = &output_buffer[i*WIDTH*4];

  write_png_file("output.png", output_row_pointers);
  printf("output.png written!\n");

  for (int i = 0; i < NUM_THREADS; i++) {
    free(bd_arr[i].buf);
  }

  curl_multi_cleanup(mult);

  curl_global_cleanup();
  free(output_row_pointers);
  free(output_buffer);
  free(received_fragments);
  
  return 0;
}