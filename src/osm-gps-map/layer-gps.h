/*
 * Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
 *
 * Contributions by
 * Damien Caliste 2014 <dcaliste@free.fr>
 *
 * This file is part of Maep.
 *
 * Maep is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Maep is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Maep.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LAYER_GPS_H
#define LAYER_GPS_H

#include <glib-object.h>

G_BEGIN_DECLS

#define MAEP_TYPE_LAYER_GPS	     (maep_layer_gps_get_type ())
#define MAEP_LAYER_GPS(obj)	     (G_TYPE_CHECK_INSTANCE_CAST(obj, MAEP_TYPE_LAYER_GPS, MaepLayerGps))
#define MAEP_LAYER_GPS_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST(klass, MAEP_TYPE_LAYER_GPS, MaepLayerGpsClass))
#define MAEP_IS_LAYER_GPS(obj)    (G_TYPE_CHECK_INSTANCE_TYPE(obj, MAEP_TYPE_LAYER_GPS))
#define MAEP_IS_LAYER_GPS_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE(klass, MAEP_TYPE_LAYER_GPS))
#define MAEP_LAYER_GPS_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS(obj, MAEP_TYPE_LAYER_GPS, MaepLayerGpsClass))

#define MAEP_COMPASS_MODES \
  COMPASS_MODE_OFF = 0, \
  COMPASS_MODE_NORTH, \
  COMPASS_MODE_DEVICE, \
  COMPASS_N_MODES

typedef struct _MaepLayerGps        MaepLayerGps;
typedef struct _MaepLayerGpsPrivate MaepLayerGpsPrivate;
typedef struct _MaepLayerGpsClass   MaepLayerGpsClass;

struct _MaepLayerGps
{
  GObject parent;

  MaepLayerGpsPrivate *priv;
};

struct _MaepLayerGpsClass
{
  GObjectClass parent;
};

typedef enum {
    MAEP_COMPASS_MODES
} MaepLayerCompassMode;

GType maep_layer_gps_get_type(void);

MaepLayerGps* maep_layer_gps_new(void);
gboolean maep_layer_gps_set_coordinates(MaepLayerGps *gps, gfloat lat, gfloat lon,
                                        gfloat hprec, gfloat heading);
gboolean maep_layer_gps_set_active(MaepLayerGps *gps, gboolean status);
gboolean maep_layer_gps_set_azimuth(MaepLayerGps *gps, gfloat azimuth);
gboolean maep_layer_gps_set_compass_mode(MaepLayerGps *gps, MaepLayerCompassMode mode);

G_END_DECLS

#endif
