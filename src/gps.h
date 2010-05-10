/*
 * Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
 * 
 * This file is based upon parts of gpsd/libgps
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
 *
 */

#ifndef GPS_H
#define GPS_H

#include <glib.h>
#include <math.h>  // for isnan

#include <libgnomevfs/gnome-vfs.h>
#include <libgnomevfs/gnome-vfs-inet-connection.h>

#ifdef USE_LIBGPS
#include <gps.h>
#endif

#ifndef NAN
#define NAN (0.0/0.0)
#endif /* !NAN */

#define MAXCHANNELS 20
#define MAXTAGLEN    8       /* maximum length of sentence tag name */

typedef struct {
  int prn;     /* prn of satellite, -1 if no info */
  int ss;      /* signal to noise ratio */
  int used;    /* satellite is is being used */
} gps_sat_t;

struct gps_t {
  double latitude;	/* Latitude in degrees */
  double longitude;	/* Longitude in degrees */
  double altitude;      /* Altitude in meters */
  double eph;  	        /* Horizontal position uncertainty, meters */
  double track;	        /* Course made good (relative to true north) */

  int sat_num;
  gps_sat_t sat_data[MAXCHANNELS];
};

typedef unsigned int gps_mask_t;

#define FIX_LATLON_SET	   (1<<0)
#define FIX_ALTITUDE_SET   (1<<1)
#define FIX_TRACK_SET	   (1<<2)
#define FIX_HERR_SET	   (1<<3)
#define FIX_SATELLITE_SET  (1<<4)

#define LATLON_CHANGED    (FIX_LATLON_SET    << 8)
#define ALTITUDE_CHANGED  (FIX_ALTITUDE_SET  << 8)
#define TRACK_CHANGED     (FIX_TRACK_SET     << 8)
#define HERR_CHANGED      (FIX_HERR_SET      << 8)
#define SATELLITE_CHANGED (FIX_SATELLITE_SET << 8)

#define CHANGED_MASK 0xff00

#ifdef USE_MAEMO
#ifdef ENABLE_GPSBT
#include <gpsbt.h>
#include <gpsmgr.h>
#endif
#include <errno.h>
#endif

typedef void (*gps_cb)(gps_mask_t set, struct gps_t *fix, void *data);
#define GPS_CB(f) ((gps_cb)(f))

typedef struct {
  gps_mask_t mask;
  gps_cb cb;
  void *data;
} gps_callback_t;

#ifdef ENABLE_LIBLOCATION
#include <location/location-gps-device.h>
#include <location/location-gpsd-control.h>
#endif

typedef struct gps_state {
#ifndef ENABLE_LIBLOCATION
#ifdef ENABLE_GPSBT
  gpsbt_t context;
#endif

  GThread* thread_p;
  GMutex *mutex, *control_mutex;

#ifdef USE_LIBGPS
  struct gps_data_t *data;
#else
  GnomeVFSInetConnection *iconn;
  GnomeVFSSocket *socket;
#endif

#else
  LocationGPSDevice *device;
  LocationGPSDControl *control;
  gboolean connected;
  guint idd_changed;
#endif

  struct {
    struct gps_t fix;
    gps_mask_t set;
  } last;

  struct gps_t fix;   
  gps_mask_t set;

  GSList *callbacks;

} gps_state_t;

gps_state_t *gps_init(void);
void gps_register_callback(gps_state_t *gps_state, int mask, 
			   gps_cb cb, void *data);
void gps_unregister_callback(gps_state_t *gps_state, gps_cb cb);
void gps_release(gps_state_t *gps_state);

#endif // GPS_H
