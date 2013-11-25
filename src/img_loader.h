#ifndef IMG_LOADER_H
#define IMG_LOADER_H

#include <glib.h>

#include <cairo.h>

enum {
  MAEP_LOADER_ERROR_JPEG,
  MAEP_LOADER_ERROR_JPEG_HEADER,
  MAEP_LOADER_ERROR_JPEG_DECOMPRESS,
  MAEP_LOADER_ERROR_JPEG_SCANLINES,
  MAEP_LOADER_ERROR_FILE
};

GQuark maep_img_loader_get_error();
#define MAEP_LOADER_ERROR maep_img_loader_get_error()

cairo_surface_t* maep_loader_jpeg_from_file(const char *filename, GError **error);
#if JPEG_LIB_VERSION >= 80
cairo_surface_t* maep_loader_jpeg_from_mem(const unsigned char *buffer,
                                           size_t len, GError **error);
#endif

#endif
