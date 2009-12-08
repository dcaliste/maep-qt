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

#ifndef NAN
#define NAN (0.0/0.0)
#endif /* !NAN */

#define MAXTAGLEN    8       /* maximum length of sentence tag name */
#define MPS_TO_KNOTS 1.9438445       /* Meters per second to knots */

struct gps_fix_t {
  int    mode;	/* Mode of fix */
#define MODE_NOT_SEEN	0	/* mode update not seen yet */
#define MODE_NO_FIX	1	/* none */
#define MODE_2D  	2	/* good for latitude/longitude */
#define MODE_3D  	3	/* good for altitude/climb too */
  double latitude;	/* Latitude in degrees (valid if mode >= 2) */
  double longitude;	/* Longitude in degrees (valid if mode >= 2) */
  double eph;  	        /* Horizontal position uncertainty, meters */
  double track;	        /* Course made good (relative to true north) */
};

typedef unsigned int gps_mask_t;

#define LATLON_SET	0x00000008u
#define TRACK_SET	0x00000040u
#define STATUS_SET	0x00000100u
#define MODE_SET	0x00000200u
#define HERR_SET	0x00008000u

#define STATUS_NO_FIX	0	/* no */
#define STATUS_FIX	1	/* yes, without DGPS */
#define STATUS_DGPS_FIX	2	/* yes, with DGPS */

struct gps_data_t {
  struct {
   struct gps_fix_t	fix;		/* accumulated PVT data */
    int    status;		/* Do we have a fix? */
  } last_reported;

  gps_mask_t set;	/* has field been set since this was last cleared? */
  struct gps_fix_t	fix;		/* accumulated PVT data */
  
  /* GPS status -- always valid */
  int    status;		/* Do we have a fix? */
};

#ifdef USE_MAEMO
#ifdef ENABLE_GPSBT
#include <gpsbt.h>
#include <gpsmgr.h>
#endif
#include <errno.h>
#endif

typedef void (*gps_cb)(int status, struct gps_fix_t *fix, void *data);
#define GPS_CB(f) ((gps_cb)(f))

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
  GMutex *mutex;
  GnomeVFSInetConnection *iconn;
  GnomeVFSSocket *socket;

  struct gps_data_t gpsdata;
#else
  LocationGPSDevice *device;
  LocationGPSDControl *control;
  guint idd_changed;
  int fields;

  struct gps_fix_t	fix;   
#endif

  gps_cb cb;
  void *cb_data;

} gps_state_t;

gps_state_t *gps_init(gps_cb cb, void *data);
void gps_release();

#endif // GPS_H
