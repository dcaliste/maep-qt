#include <glib.h>

#include <cairo.h>

#include <jpeglib.h>
#include <setjmp.h>

static GQuark loader_quark = 0;
GQuark loader_get_quark()
{
  if (!loader_quark)
    loader_quark = g_quark_from_static_string("MAEP_IMG_LOADER");
  return loader_quark;
}

struct my_error_mgr {
  struct jpeg_error_mgr pub;	/* "public" fields */
  GError **error;

  jmp_buf setjmp_buffer;	/* for return to caller */
};

typedef struct my_error_mgr * my_error_ptr;

static void _error_exit(j_common_ptr cinfo)
{
  char msg[JMSG_LENGTH_MAX];

  /* cinfo->err really points to a my_error_mgr struct, so coerce pointer */
  my_error_ptr myerr = (my_error_ptr) cinfo->err;

  /* Always display the message. */
  /* We could postpone this until after returning, if we chose. */
  (*cinfo->err->output_message) (cinfo);

  (*cinfo->err->output_message) (cinfo, msg);
  g_set_error(myerr->error, MAEP_IMG_LOADER_ERROR, MAEP_IMG_LOADER_ERROR_JPEG, msg);

  /* Return control to the setjmp point */
  longjmp(myerr->setjmp_buffer, 1);
}

static cairo_surface_t* read_jpeg_from_file(const char *filename, GError **error)
{
  FILE * infile;
  struct jpeg_decompress_struct cinfo;
  struct my_error_mgr jerr;
  int jpeg_stride, cairo_stride;
  unsigned char *data;
  cairo_surface_t *surf = NULL;

  if ((infile = fopen(filename, "rb")) == NULL) {
    g_set_error(error, MAEP_IMG_LOADER_ERROR, MAEP_IMG_LOADER_ERROR_FILE,
                "can't open %s", filename);
    return NULL;
  }

  /* Step 1: allocate and initialize JPEG decompression object */

  /* We set up the normal JPEG error routines, then override error_exit. */
  jerr.error = error;
  cinfo.err = jpeg_std_error(&jerr.pub);
  jerr.pub.error_exit = _error_exit;
  /* Establish the setjmp return context for my_error_exit to use. */
  if (setjmp(jerr.setjmp_buffer)) {
    /* If we get here, the JPEG code has signaled an error.
     * We need to clean up the JPEG object, close the input file, and return.
     */
    jpeg_destroy_decompress(&cinfo);
    if (surf)
      cairo_surface_destroy(surf);
    fclose(infile);
    return NULL;
  }
  /* Now we can initialize the JPEG decompression object. */
  jpeg_create_decompress(&cinfo);

  /* Step 2: specify data source (eg, a file) */
  jpeg_stdio_src(&cinfo, infile);

  /* Step 3: read file parameters with jpeg_read_header() */
  (void) jpeg_read_header(&cinfo, TRUE);

  /* Step 5: Start decompressor */
  (void) jpeg_start_decompress(&cinfo);

  /* Image dimensions. */
  surf = cairo_image_surface_create(CAIRO_FORMAT_RGB24,
                                    cinfo.output_width, cinfo.output_height);
  cairo_stride = cairo_image_surface_get_stride(surf);
  jpeg_stride = cinfo.output_width * cinfo.output_components;

  /* Step 6: Read data to Cairo surface. */
  cairo_surface_flush(surf);
  data = cairo_image_surface_get_data(surf);
  if (jpeg_stride == cairo_stride)
    (void) jpeg_read_scanlines(&cinfo, data, cinfo.output_height);
  else {
    while (cinfo.output_scanline < cinfo.output_height) {
      (void) jpeg_read_scanlines(&cinfo, data, 1);
      data += cairo_stride;
    }
  }
  cairo_surface_mark_dirty(surf);

  /* Step 7: Finish decompression */
  (void) jpeg_finish_decompress(&cinfo);

  /* This is an important step since it will release a good deal of memory. */
  jpeg_destroy_decompress(&cinfo);

  fclose(infile);
  
  surf = cairo_image_surface_create_for_data(data, CAIRO_FORMAT_RGB24,
                                             cinfo.output_width, cinfo.output_height,
                                             row_stride);

  return surf;
}

int main(int argc, const char **argv)
{
  GError *error = NULL;
  cairo_surface_t *surf;

  surf = read_jpeg_from_file(argv[1], &error);
  if (error)
    {
      g_error("%s", error->message);
      g_error_free(error);
      return 1;
    }
  cairo_surface_write_to_png(surf, "test.png");
  cairo_surface_destroy(surf);

  return 0;
}
