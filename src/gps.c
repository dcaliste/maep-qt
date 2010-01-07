/*
 * Copyright (C) 2009 Till Harbaum <till@harbaum.org>.
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

#include <stdio.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>

#ifdef USE_MAEMO
#ifdef ENABLE_GPSBT
#include <gpsbt.h>
#include <gpsmgr.h>
#endif
#include <errno.h>
#endif

#include "gps.h"
#include "track.h"

static void gps_unregister_all(gps_state_t *gps_state);

#ifndef ENABLE_LIBLOCATION

/* maybe user configurable later on ... */
#define GPSD_HOST "127.0.0.1"
#define GPSD_PORT 2947

static int gps_connect(gps_state_t *gps_state) {
  GnomeVFSResult vfs_result;

#ifdef USE_MAEMO
  char errstr[256] = "";

  if(!gps_state) {
    printf("No gps state\n");
    return -1;
  }

  /* We need to start gpsd (via gpsbt) first. */
  memset(&gps_state->context, 0, sizeof(gpsbt_t));
  errno = 0;

  if(gpsbt_start(NULL, 0, 0, 0, errstr, sizeof(errstr),
		 0, &gps_state->context) < 0) {
    printf("gps: Error connecting to GPS receiver: (%d) %s (%s)\n",
	   errno, strerror(errno), errstr);
  }
#endif

  /************** from here down pure gnome/gtk/gpsd ********************/

  /* try to connect to gpsd */
  /* Create a socket to interact with GPSD. */

  int retries = 5;
  while(retries && 
	(GNOME_VFS_OK != (vfs_result = gnome_vfs_inet_connection_create(
		&gps_state->iconn, GPSD_HOST, GPSD_PORT, NULL)))) {
    printf("gps: Error creating connection to GPSD, retrying ...\n");

    retries--;
    sleep(1);
  }

  if(!retries) {
    printf("gps: Finally failed ...\n");
    return -1;
  }

  retries = 5;
  while(retries && ((gps_state->socket = 
     gnome_vfs_inet_connection_to_socket(gps_state->iconn)) == NULL)) {
    printf("gps: Error connecting GPSD socket, retrying ...\n");

    retries--;
    sleep(1);
  }

  if(!retries) {
    printf("gps: Finally failed ...\n");
    return -1;
  }

  GTimeVal timeout = { 10, 0 };
  if(GNOME_VFS_OK != (vfs_result = gnome_vfs_socket_set_timeout(
	gps_state->socket, &timeout, NULL))) {
    printf("gps: Error setting GPSD timeout\n");
    return -1;
  } 

  printf("gps: GPSD connected ...\n");

  return 0;
}	

/* unpack a daemon response into a status structure */
static void gps_unpack(char *buf, struct gps_data_t *gpsdata) {
  char *ns, *sp, *tp;

  for(ns = buf; ns; ns = strstr(ns+1, "GPSD")) {
    if(strncmp(ns, "GPSD", 4) == 0) {
      /* the following should execute each time we have a good next sp */
      for (sp = ns + 5; *sp != '\0'; sp = tp+1) {
	tp = sp + strcspn(sp, ",\r\n");
	if (*tp == '\0') tp--;
	else *tp = '\0';
	
	switch (*sp) {
	  /* A - altitude isn't supported by maep */
	  /* B - baudrate isn't supported by maep */
	  /* C - cycle isn't supported by maep */
	  /* D - utc time isn't supported by maep */
	case 'E':
	  gpsdata->fix.eph = NAN;
	  /* epe should always be present if eph or epv is */
	  if (sp[2] != '?') {
	    char epe[20], eph[20], epv[20];
	    (void)sscanf(sp, "E=%s %s %s", epe, eph, epv);
#define DEFAULT(val) (val[0] == '?') ? NAN : g_ascii_strtod(val, NULL)
	    gpsdata->fix.eph = DEFAULT(eph);
#undef DEFAULT
	  }
	  break;
	  /* F - device name isn't supported by maep */
	  /* I - gps id isn't supported by maep */
	  /* K - known devices list isn't supported by maep */
	case 'M':
	  if (sp[2] == '?') {
	    gpsdata->fix.mode = MODE_NOT_SEEN;
	  } else {
	    gpsdata->fix.mode = atoi(sp+2);
	    gpsdata->set |= MODE_SET;
	  }
	  break;
	  /* N - driver mode reporting isn't supported by maep */
	case 'O':
	  if (sp[2] == '?') {
	    gpsdata->set = MODE_SET | STATUS_SET; 
	    gpsdata->status = STATUS_NO_FIX;
	    gpsdata->fix.latitude = NAN;
	    gpsdata->fix.longitude = NAN;
	    gpsdata->fix.track = NAN;
	    gpsdata->fix.eph = NAN;
	  } else {
	    struct gps_fix_t nf;
	    char tag[MAXTAGLEN+1], alt[20];
	    char eph[20], epv[20], track[20],speed[20], climb[20];
	    char epd[20], eps[20], epc[20], mode[2];
	    char timestr[20], ept[20], lat[20], lon[20];
	    int st = sscanf(sp+2, 
			    "%8s %19s %19s %19s %19s %19s %19s %19s "
			    "%19s %19s %19s %19s %19s %19s %1s",
			    tag, timestr, ept, lat, lon,
			    alt, eph, epv, track, speed, climb,
			    epd, eps, epc, mode);
	    if (st >= 14) {
#define DEFAULT(val) (val[0] == '?') ? NAN : g_ascii_strtod(val, NULL)
	      nf.latitude = DEFAULT(lat);
	      nf.longitude = DEFAULT(lon);
	      nf.eph = DEFAULT(eph);
	      nf.track = DEFAULT(track);
#undef DEFAULT
	      if (st >= 15)
		nf.mode = (mode[0] == '?') ? MODE_NOT_SEEN : atoi(mode);
	      else
		nf.mode = (alt[0] == '?') ? MODE_2D : MODE_3D;
	      if (isnan(nf.eph)==0)
		gpsdata->set |= HERR_SET;
	      if (isnan(nf.track)==0)
		gpsdata->set |= TRACK_SET;
	      gpsdata->fix = nf;
	      gpsdata->set |= LATLON_SET|MODE_SET;
	      gpsdata->status = STATUS_FIX;
	      gpsdata->set |= STATUS_SET;
	    }
	  }
	  break;
	case 'P':
	  if (sp[2] == '?') {
	    gpsdata->fix.latitude = NAN;
	    gpsdata->fix.longitude = NAN;
	  } else {
	    char lat[20], lon[20];
	    (void)sscanf(sp, "P=%19s %19s", lat, lon);
	    gpsdata->fix.latitude = g_ascii_strtod(lat, NULL);
	    gpsdata->fix.longitude = g_ascii_strtod(lon, NULL);
	    gpsdata->set |= LATLON_SET;
	  }
	  break;
	  /* Q - satellite info isn't supported by maep */
	case 'S':
	  if (sp[2] == '?') {
	    gpsdata->status = -1;
	  } else {
	    gpsdata->status = atoi(sp+2);
	    gpsdata->set |= STATUS_SET;
	  }
	  break;
	case 'T':
	  if (sp[2] == '?') {
	    gpsdata->fix.track = NAN;
	  } else {
	    (void)sscanf(sp, "T=%lf", &gpsdata->fix.track);
	    gpsdata->set |= TRACK_SET;
	  }
	  break;
	  /* U - climb isn't supported by maep */
	  /* V - velocity isn't supported by maep */
	  /* X - online status isn't supported by maep */
	  /* Y - sat info isn't supported by maep */
	  /* Z and $ - profiling isn't supported by maep */
	}
      }
    }
  }
}

static void cb_func(gpointer data, gpointer user_data) {
  gps_callback_t *callback = (gps_callback_t*)data;
  gps_state_t *gps_state = (gps_state_t*)user_data;

  callback->cb(gps_state->gpsdata.last_reported.status,
	       &gps_state->gpsdata.last_reported.fix, 
	       callback->data);
}

/* async gps callback */
static gboolean gps_idle_cb(gpointer data) {
  gps_state_t *gps_state = (gps_state_t*)data;

  if(gps_state->callbacks) {
    g_mutex_lock(gps_state->mutex);

    /* tell all clients */
    g_slist_foreach(gps_state->callbacks, cb_func, gps_state);

    g_mutex_unlock(gps_state->mutex);
  }

  return FALSE;
}

/* limits below which a fix is considered different */
#define TRACK_PRECISION (1.0)
#define POS_PRECISION (1.0/60000.0)
#define EPH_PRECISION (5.0)

static gboolean 
pos_differs(struct gps_fix_t *fix1, struct gps_fix_t *fix2) {
  if( fabsf(fix1->track - fix2->track) >= TRACK_PRECISION)
    return TRUE;

  if( fabsf(fix1->latitude - fix2->latitude) >= POS_PRECISION)
    return TRUE;

  if( fabsf(fix1->longitude - fix2->longitude) >= POS_PRECISION)
    return TRUE;

  return FALSE;
}

static gboolean 
eph_differs(struct gps_fix_t *fix1, struct gps_fix_t *fix2) {
  return( fabsf(fix1->eph - fix2->eph) >= EPH_PRECISION);
}

gpointer gps_thread(gpointer data) {
  gps_state_t *gps_state = (gps_state_t*)data;
  
  GnomeVFSFileSize bytes_read;
  GnomeVFSResult vfs_result;
  char str[512];

  const char *msg = "o\r\n";   /* pos request */

  gps_state->gpsdata.set = 0;

  gboolean connected = FALSE;

  while(1) {
    if(!connected) {
      printf("gps: trying to connect\n");

      if(gps_connect(gps_state) < 0)
	sleep(10);
      else 
	connected = TRUE;
    } else {
      if(GNOME_VFS_OK == 
	 (vfs_result = gnome_vfs_socket_write(gps_state->socket,
		      msg, strlen(msg)+1, &bytes_read, NULL))) {

	/* update every second, wait here to make sure a complete */
	/* reply is received */
	sleep(1);

	if(bytes_read == (strlen(msg)+1)) {
	  vfs_result = gnome_vfs_socket_read(gps_state->socket,
	     str, sizeof(str)-1, &bytes_read, NULL);
	  if(vfs_result == GNOME_VFS_OK) {
	    str[bytes_read] = 0; 
	    
	    g_mutex_lock(gps_state->mutex);
	    
	    gps_state->gpsdata.set &= 
	      ~(LATLON_SET|MODE_SET|STATUS_SET);
	      
	    gps_unpack(str, &gps_state->gpsdata);

	    /* we can save some energy here if we make sure that */
	    /* we really only wake up the main app whenever the */
	    /* gps state changed in a way that the map needs to */
	    /* be updated */

	    /* most important change: status changed */
	    gboolean report_change = FALSE;
	    if((gps_state->gpsdata.set & STATUS_SET) &&
	       (gps_state->gpsdata.last_reported.status != 
		gps_state->gpsdata.status))
	      report_change = TRUE;
	    else {
	      if((gps_state->gpsdata.set & STATUS_SET) &&
		 gps_state->gpsdata.status) {

		/* we have a fix, check if it has been changed */
		if((gps_state->gpsdata.set & LATLON_SET) &&
		   pos_differs(&gps_state->gpsdata.last_reported.fix,
			       &gps_state->gpsdata.fix)) 
		  report_change = TRUE;

		if((gps_state->gpsdata.set & HERR_SET) &&
		   eph_differs(&gps_state->gpsdata.last_reported.fix,
			       &gps_state->gpsdata.fix)) 
		  report_change = TRUE;
	      }
	    }
	    
	    /* inform application that new GPS data is available */
	    if(report_change) {
	      /* remember what we are about to report */
	      gps_state->gpsdata.last_reported.status = 
		gps_state->gpsdata.status;
	      gps_state->gpsdata.last_reported.fix = 
		gps_state->gpsdata.fix;

	      g_idle_add(gps_idle_cb, gps_state); 
	    }

	    g_mutex_unlock(gps_state->mutex);	    
	  }
	}
      }
    }
  }

  printf("gps: thread ended???\n");
  return NULL;
}

gps_state_t *gps_init(void) {
  gps_state_t *gps_state = g_new0(gps_state_t, 1);

  /* start a new thread to listen to gpsd */
  gps_state->mutex = g_mutex_new();
  gps_state->thread_p = 
    g_thread_create(gps_thread, gps_state, FALSE, NULL);

  return gps_state;
}

void gps_release(gps_state_t *gps_state) { 
  gps_unregister_all(gps_state);

#ifdef USE_MAEMO
  gpsbt_stop(&gps_state->context);
#endif
  g_free(gps_state);
}

#else

static void cb_func(gpointer data, gpointer user_data) {
  gps_callback_t *callback = (gps_callback_t*)data;
  gps_state_t *gps_state = (gps_state_t*)user_data;

  callback->cb(gps_state->fields & LOCATION_GPS_DEVICE_LATLONG_SET,
	       &gps_state->fix, callback->data);
}

static void
location_changed(LocationGPSDevice *device, gps_state_t *gps_state) {

  gps_state->fields = device->fix->fields;

  if(gps_state->fields & LOCATION_GPS_DEVICE_LATLONG_SET) { 
    gps_state->fix.latitude = device->fix->latitude;
    gps_state->fix.longitude = device->fix->longitude;
    gps_state->fix.eph = device->fix->eph/100.0;  // we want eph in meters
  }

  if(gps_state->fields & LOCATION_GPS_DEVICE_TRACK_SET)
    gps_state->fix.track = device->fix->track;

  /* tell all clients */
  g_slist_foreach(gps_state->callbacks, cb_func, gps_state);
}

gps_state_t *gps_init(void) {
  gps_state_t *gps_state = g_new0(gps_state_t, 1);

  gps_state->device = g_object_new(LOCATION_TYPE_GPS_DEVICE, NULL);  
  if(!gps_state->device) {
    printf("gps: Unable to connect to liblocation\n");
    g_free(gps_state);
    return NULL;
  }

  gps_state->idd_changed = 
    g_signal_connect(gps_state->device, "changed", 
		     G_CALLBACK(location_changed), gps_state);

  gps_state->control = location_gpsd_control_get_default();

  if(gps_state->control
#if MAEMO_VERSION_MAJOR < 5
     && gps_state->control->can_control
#endif
     ) {

    printf("gps: Having control over GPSD and GPS is to be enabled, starting it\n");
    location_gpsd_control_start(gps_state->control);
  }
  return gps_state;
}

void gps_release(gps_state_t *gps_state) {
  gps_unregister_all(gps_state);

  if(gps_state->control
#if MAEMO_VERSION_MAJOR < 5
     && gps_state->control->can_control
#endif     
     ) {
    printf("gps: Having control over GPSD, stopping it\n");
    location_gpsd_control_stop(gps_state->control);
  }
  
  /* Disconnect signal */
  g_signal_handler_disconnect(gps_state->device, gps_state->idd_changed);
  
  g_free(gps_state);
}

#endif // USE_LIBLOCATION

void gps_register_callback(gps_state_t *gps_state, gps_cb cb, void *data) {
  gps_callback_t *callback = g_new0(gps_callback_t, 1);

  callback->cb = cb;
  callback->data = data;

  gps_state->callbacks = g_slist_append(gps_state->callbacks, callback);
}

static gint compare(gconstpointer a, gconstpointer b) {
  return ((gps_callback_t*)a)->cb != b;
}

void gps_unregister_callback(gps_state_t *gps_state, gps_cb cb) {
  /* find callback in list */
  GSList *list = g_slist_find_custom(gps_state->callbacks, cb, compare);
  g_assert(list);

  /* and de-chain and free it */
  g_free(list->data);
  gps_state->callbacks = g_slist_remove(gps_state->callbacks, list->data);
}

static void gps_unregister_all(gps_state_t *gps_state) {
  g_slist_foreach(gps_state->callbacks, (GFunc)g_free, NULL);
  g_slist_free(gps_state->callbacks);
}


