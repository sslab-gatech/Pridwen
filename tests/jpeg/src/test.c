#include <stdarg.h>
#include <stdint.h>
#include <stdio.h> /* vsnprintf */
#include <stdlib.h>

#include "jpeglib.h"

unsigned char* raw_image;
int image_height;
int image_width;

GLOBAL(void)
write_JPEG_file (char * filename, int quality)
{
  struct jpeg_compress_struct cinfo;
  struct jpeg_error_mgr jerr;
  FILE * outfile;   /* target file */
  JSAMPROW row_pointer[1];  /* pointer to JSAMPLE row[s] */
  int row_stride;   /* physical row width in image buffer */

  /* Step 1: allocate and initialize JPEG compression object */
  cinfo.err = jpeg_std_error(&jerr);
  /* Now we can initialize the JPEG compression object. */
  jpeg_create_compress(&cinfo);

  /* Step 2: specify data destination (eg, a file) */
  /* Note: steps 2 and 3 can be done in either order. */
  if ((outfile = fopen(filename, "wb")) == NULL) {
    printf("can't open %s\n", filename);
    exit(1);
  }
  jpeg_stdio_dest(&cinfo, outfile);

  /* Step 3: set parameters for compression */
  cinfo.image_width = image_width;  /* image width and height, in pixels */
  cinfo.image_height = image_height;
  cinfo.input_components = 3;   /* # of color components per pixel */
  cinfo.in_color_space = JCS_RGB;   /* colorspace of input image */
  
  jpeg_set_defaults(&cinfo);
  jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

  /* Step 4: Start compressor */
  jpeg_start_compress(&cinfo, TRUE);

  /* Step 5: while (scan lines remain to be written) */
  /*           jpeg_write_scanlines(...); */
  row_stride = image_width * 3; /* JSAMPLEs per row in image_buffer */

  while (cinfo.next_scanline < cinfo.image_height) {
    row_pointer[0] = &raw_image[cinfo.next_scanline * row_stride];
    (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
  }

  /* Step 6: Finish compression */

  jpeg_finish_compress(&cinfo);
  /* After finish_compress, we can close the output file. */
  fclose(outfile);

  /* Step 7: release JPEG compression object */

  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_compress(&cinfo);
}

GLOBAL(int)
read_JPEG_file (char * filename)
{
  struct jpeg_decompress_struct cinfo;
  struct jpeg_error_mgr jerr;
  /* More stuff */
  FILE * infile;    /* source file */
  JSAMPROW row_pointer[1];  /* pointer to JSAMPLE row[s] */
  int row_stride;   /* physical row width in output buffer */
  unsigned long location = 0;

  if ((infile = fopen(filename, "rb")) == NULL) {
    printf("can't open %s\n", filename);
    return 0;
  }

  /* Step 1: allocate and initialize JPEG decompression object */

  cinfo.err = jpeg_std_error(&jerr);

  jpeg_create_decompress(&cinfo);

  /* Step 2: specify data source (eg, a file) */
  jpeg_stdio_src(&cinfo, infile);

  /* Step 3: read file parameters with jpeg_read_header() */
  (void) jpeg_read_header(&cinfo, TRUE);

  /* Step 4: set parameters for decompression */

  /* In this example, we don't need to change any of the defaults set by
   * jpeg_read_header(), so we do nothing here.
   */

  /* Step 5: Start decompressor */

  (void) jpeg_start_decompress(&cinfo);
  row_stride = cinfo.output_width * cinfo.output_components;
  /* Make a one-row-high sample array that will go away when done with image */
  raw_image = (unsigned char *) malloc(cinfo.output_height * row_stride);
  row_pointer[0] = (unsigned char *) malloc(row_stride);

  /* Step 6: while (scan lines remain to be read) */
  /*           jpeg_read_scanlines(...); */

  while (cinfo.output_scanline < cinfo.output_height) {
    (void) jpeg_read_scanlines(&cinfo, row_pointer, 1);
    for (int i = 0; i < row_stride; i++) {
      raw_image[location++] = row_pointer[0][i];
    }
  }

  image_height = cinfo.image_height;
  image_width = cinfo.image_width;

  /* Step 7: Finish decompression */

  (void) jpeg_finish_decompress(&cinfo);

  /* Step 8: Release JPEG decompression object */

  jpeg_destroy_decompress(&cinfo);

  fclose(infile);

  /* And we're done! */
  return 1;
}

// void enclave_main()
int
main()
{
  printf("read_JPEG_file\n");
  read_JPEG_file("test.jpg");

  printf("write JPEG_file\n");
  write_JPEG_file("testout.jpg", 75);
  return 0;
}
