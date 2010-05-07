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

/* 
 * This version focusses on low power consumption. This means that 
 * a component can register for GPS callbacks for certain events 
 * (e.g. position change, altitude change, track change ...) and it
 * will only get notified if one of these changes. Also it's been 
 * verified that the change in question exceed a certain limit.
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
#else
#ifdef USE_LIBGPS
#include <gps.h>
#endif
#endif

#include "gps.h"
#include "track.h"

static void gps_unregister_all(gps_state_t *gps_state);

/* limits below which a fix is considered different */
#define TRACK_PRECISION (1.0)           // one degree
#define POS_PRECISION (1.0/60000.0)     // 0.001 minute
#define EPH_PRECISION (2.0)             // 2 meters
#define ALTITUDE_PRECISION (1.0)        // 1 meters

static gboolean 
track_differs(struct gps_t *fix1, struct gps_t *fix2) {
  return( fabsf(fix1->track - fix2->track) >= TRACK_PRECISION);
}

static gboolean 
latlon_differs(struct gps_t *fix1, struct gps_t *fix2) {
  if( fabsf(fix1->latitude - fix2->latitude) >= POS_PRECISION)
    return TRUE;

  if( fabsf(fix1->longitude - fix2->longitude) >= POS_PRECISION)
    return TRUE;

  return FALSE;
}

static gboolean 
eph_differs(struct gps_t *fix1, struct gps_t *fix2) {
  return( fabsf(fix1->eph - fix2->eph) >= EPH_PRECISION);
}

static gboolean 
altitude_differs(struct gps_t *fix1, struct gps_t *fix2) {
  return( fabsf(fix1->altitude - fix2->altitude) >= ALTITUDE_PRECISION);
}

static void gps_cb_func(gpointer data, gpointer user_data) {
  gps_callback_t *callback = (gps_callback_t*)data;
  gps_state_t *gps_state = (gps_state_t*)user_data;

  /* we can save some energy here if we make sure that */
  /* we really only wake up the main app whenever the */
  /* gps state changed in a way that the map needs to */
  /* be updated */
  
  /* for all data check if its status has changed (data is available */
  /* or not) or if the data itself changed */

  /* clear "changed" bits */
  gps_state->set &= ~CHANGED_MASK;

  /* check for changes in position */
  if(callback->mask & LATLON_CHANGED &&
     (((gps_state->set & FIX_LATLON_SET) != 
       (gps_state->last.set & FIX_LATLON_SET)) ||
      ((gps_state->set & FIX_LATLON_SET) && 
       latlon_differs(&gps_state->last.fix, &gps_state->fix)))) 
    gps_state->set |= LATLON_CHANGED;

  /* check for changes in eph */
  if(callback->mask & HERR_CHANGED &&
     (((gps_state->set & FIX_HERR_SET) != 
       (gps_state->last.set & FIX_HERR_SET)) ||
      ((gps_state->set & FIX_HERR_SET) && 
       eph_differs(&gps_state->last.fix, &gps_state->fix))))
    gps_state->set |= HERR_CHANGED;

  /* check for changes in track */
  if(callback->mask & TRACK_CHANGED &&
     (((gps_state->set & FIX_TRACK_SET) != 
       (gps_state->last.set & FIX_TRACK_SET)) ||
      ((gps_state->set & FIX_TRACK_SET) && 
       track_differs(&gps_state->last.fix, &gps_state->fix))))
    gps_state->set |= TRACK_CHANGED;

  /* check for changes in altitude */
  if(callback->mask & ALTITUDE_CHANGED &&
     (((gps_state->set & FIX_ALTITUDE_SET) != 
       (gps_state->last.set & FIX_ALTITUDE_SET)) ||
      ((gps_state->set & FIX_ALTITUDE_SET) && 
       altitude_differs(&gps_state->last.fix, &gps_state->fix))))
    gps_state->set |= ALTITUDE_CHANGED;

  if(gps_state->set & CHANGED_MASK)
    callback->cb(gps_state->set, &gps_state->fix, callback->data);
}

#ifndef ENABLE_LIBLOCATION

static gboolean gps_notify(gpointer data) {
  gps_state_t *gps_state = (gps_state_t*)data;

  if(gps_state->callbacks) {
    g_mutex_lock(gps_state->mutex);

    /* tell all clients */
    g_slist_foreach(gps_state->callbacks, gps_cb_func, gps_state);

    g_mutex_unlock(gps_state->mutex);
  }

  /* remember last state reported */
  gps_state->last.set = gps_state->set;
  gps_state->last.fix = gps_state->fix;
  
  return FALSE; 
}

#ifndef USE_LIBGPS

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

static double parse_double(char *val) {
  if(val[0] == '?') return NAN;
  return g_ascii_strtod(val, NULL);
}

/* unpack a daemon response into a status structure */
static void gps_unpack(char *buf, gps_state_t *gps_state) {
  char *ns, *sp, *tp;
  int j;

  for(ns = buf; ns; ns = strstr(ns+1, "GPSD")) {
    if(strncmp(ns, "GPSD", 4) == 0) {
      /* the following should execute each time we have a good next sp */
      for (sp = ns + 5; *sp != '\0'; sp = tp+1) {
	tp = sp + strcspn(sp, ",\r\n");
	if (*tp == '\0') tp--;
	else *tp = '\0';
	
	switch (*sp) {
	case 'O':
	  if (sp[2] == '?') {
	    gps_state->fix.latitude = NAN;
	    gps_state->fix.longitude = NAN;
	    gps_state->fix.altitude = NAN;
	    gps_state->fix.track = NAN;
	    gps_state->fix.eph = NAN;
	  } else {
	    struct gps_t nf;
	    char tag[MAXTAGLEN+1], alt[20];
	    char eph[20], track[20],speed[20];
	    char lat[20], lon[20];
	    if(sscanf(sp+2, 
		      "%8s %*s %*s %19s %19s "
		      "%19s %19s %*s %19s %19s %*s "
		      "%*s %*s %*s %*s",
		      tag, lat, lon,
		      alt, eph, track, speed) == 7) {

	      nf.latitude = parse_double(lat);
	      nf.longitude = parse_double(lon);
	      nf.altitude = parse_double(alt);
	      nf.eph = parse_double(eph);
	      nf.track = parse_double(track);

	      if (!isnan(nf.eph))
		gps_state->set |= FIX_HERR_SET;

	      if (!isnan(nf.track))
		gps_state->set |= FIX_TRACK_SET;

	      gps_state->fix = nf;
	      gps_state->set |= FIX_LATLON_SET;

	      if(!isnan(nf.altitude))
		gps_state->set |= FIX_ALTITUDE_SET;
	    }
	  }
	  break;

        case 'Y': 
	  gps_state->fix.sat_num = 0;

          if (sp[2] != '?') {
            (void)sscanf(sp, "Y=%*s %*s %d ", &gps_state->fix.sat_num);

	    /* clear all slots */
            for (j = 0; j < gps_state->fix.sat_num; j++) 
	      gps_state->fix.sat_data[j].prn = 
	      gps_state->fix.sat_data[j].ss = 
	      gps_state->fix.sat_data[j].used = 0;

	    printf("gps: sats = %d\n", gps_state->fix.sat_num);
            for (j = 0; j < gps_state->fix.sat_num; j++) {
              if ((sp != NULL) && ((sp = strchr(sp, ':')) != NULL)) {
                sp++;
                (void)sscanf(sp, "%d %*d %*d %d %d", 
			     &gps_state->fix.sat_data[j].prn,
			     &gps_state->fix.sat_data[j].ss, 
			     &gps_state->fix.sat_data[j].used);
              }
            }
          }
          gps_state->set |= FIX_SATELLITE_SET;
	  break;
	}
      }
    }
  }
}

static gpointer gps_thread(gpointer data) {
  gps_state_t *gps_state = (gps_state_t*)data;
  
  GnomeVFSFileSize bytes_read;
  GnomeVFSResult vfs_result;
  char str[512];

  gps_state->set = 0;

  gboolean connected = FALSE;

  while(1) {
    /* just lock and unlock the control mutex. This stops the thread */
    /* while the main process locks this mutex */
    g_mutex_lock(gps_state->control_mutex);
    g_mutex_unlock(gps_state->control_mutex);

    if(!connected) {
      printf("gps: trying to connect\n");

      if(gps_connect(gps_state) < 0)
	sleep(10);
      else 
	connected = TRUE;

    } else {
      const char *msg = "o\r\n";   /* pos request */

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
	    
	    /* assume we could't parse anything ... */
	    gps_state->set = 0;
	      
	    gps_unpack(str, gps_state);

	    /* notify applications of state if useful */
	    g_idle_add(gps_notify, gps_state); 

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
  gps_state->control_mutex = g_mutex_new();
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

#else // USE_LIBGPS

static gpointer gps_thread(gpointer data) {
  gps_state_t *gps_state = (gps_state_t*)data;

  /* the following is required for libgps to be able to parse */
  /* the gps messages. Unfortunately this also affect the rest of */
  /* the program and the main thread */
  setlocale(LC_NUMERIC, "C");

  while(1) {
    /* just lock and unlock the control mutex. This stops the thread */
    /* while the main process locks this mutex */
    g_mutex_lock(gps_state->control_mutex);
    g_mutex_unlock(gps_state->control_mutex);

    gps_poll(gps_state->data);

    g_mutex_lock(gps_state->mutex);
    
    /* assume we could't parse anything ... */
    gps_state->set = 0;
    
    if(gps_state->data->fix.mode >= MODE_2D) {
      /* latlon valid */
      gps_state->set |= FIX_LATLON_SET;
      gps_state->fix.latitude = gps_state->data->fix.latitude;
      gps_state->fix.longitude = gps_state->data->fix.longitude;

      gps_state->fix.eph = 
	gps_state->data->fix.epy > gps_state->data->fix.epx ?
	gps_state->data->fix.epy : gps_state->data->fix.epx;
      if(!isnan(gps_state->fix.eph)) gps_state->set |= FIX_HERR_SET;

      gps_state->fix.track = gps_state->data->fix.track;
      if(!isnan(gps_state->fix.track)) gps_state->set |= FIX_TRACK_SET;
    }

    if(gps_state->data->fix.mode >= MODE_3D) {
      /* altitude valid */
      gps_state->set |= FIX_ALTITUDE_SET;
      gps_state->fix.altitude = gps_state->data->fix.altitude;
    } else
      gps_state->fix.altitude = NAN;

    /* notify applications of state if useful */
    g_idle_add(gps_notify, gps_state); 
    
    g_mutex_unlock(gps_state->mutex);	    
    
  }

  printf("gps: thread ended???\n");
  return NULL;
}

gps_state_t *gps_init(void) {
  gps_state_t *gps_state = g_new0(gps_state_t, 1);

  gps_state->mutex = g_mutex_new();
  gps_state->control_mutex = g_mutex_new();

  gps_state->data = gps_open("127.0.0.1", DEFAULT_GPSD_PORT );
  if(!gps_state->data)
    perror("gps_open()");

  (void)gps_stream(gps_state->data, WATCH_ENABLE, NULL);

  gps_state->thread_p = 
    g_thread_create(gps_thread, gps_state, FALSE, NULL);

  return gps_state;
}

void gps_release(gps_state_t *gps_state) { 
  gps_unregister_all(gps_state);
  gps_close(gps_state->data); 
  g_free(gps_state);
}

#endif // USE_LIBGPS

static void gps_background_enable(gps_state_t *gps_state, gboolean enable) {
  printf("GPS: %sable background process\n", enable?"en":"dis");

  /* start and stop gps thread by locking and unlocking the control mutex */
  if(enable) g_mutex_unlock(gps_state->control_mutex);
  else       g_mutex_lock(gps_state->control_mutex);
}

#else

static void
location_changed(LocationGPSDevice *device, gps_state_t *gps_state) {

  gps_state->set = 0;

  if(device->fix->fields & LOCATION_GPS_DEVICE_LATLONG_SET) { 
    gps_state->set |= FIX_LATLON_SET | FIX_HERR_SET;
    gps_state->fix.latitude = device->fix->latitude;
    gps_state->fix.longitude = device->fix->longitude;
    gps_state->fix.eph = device->fix->eph/100.0;  // we want eph in meters
  }

  if(device->fix->fields & LOCATION_GPS_DEVICE_ALTITUDE_SET) {
    gps_state->set |= FIX_ALTITUDE_SET;
    gps_state->fix.altitude = device->fix->altitude;
  } else
    gps_state->fix.altitude = NAN;

  if(device->fix->fields & LOCATION_GPS_DEVICE_TRACK_SET) {
    gps_state->set |= FIX_TRACK_SET;
    gps_state->fix.track = device->fix->track;
  }

  /* tell all clients */
  g_slist_foreach(gps_state->callbacks, gps_cb_func, gps_state);

  /* remember last state reported */
  gps_state->last.set = gps_state->set;
  gps_state->last.fix = gps_state->fix;
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
  gps_state->connected = TRUE;

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
  gps_state->connected = FALSE;
  
  g_free(gps_state);
}

static void gps_background_enable(gps_state_t *gps_state, gboolean enable) {
  printf("GPS: %sable background process\n", enable?"en":"dis");

  if(enable) {
    if(!gps_state->connected) {
      gps_state->idd_changed = 
	g_signal_connect(gps_state->device, "changed", 
			 G_CALLBACK(location_changed), gps_state);
      gps_state->connected = TRUE;
    }
  } else {
    if(gps_state->connected) {
      /* Disconnect signal */
      g_signal_handler_disconnect(gps_state->device, gps_state->idd_changed);
      gps_state->connected = FALSE;
    }
  }
}

#endif // USE_LIBLOCATION

static gint compare(gconstpointer a, gconstpointer b) {
  return ((gps_callback_t*)a)->cb != b;
}

void gps_register_callback(gps_state_t *gps_state, int mask, 
			   gps_cb cb, void *data) {
  /* make sure the callback isn't already registered */
  GSList *list = g_slist_find_custom(gps_state->callbacks, cb, compare);
  if(list) {
    printf("GPS register: ignoring duplicate\n"); 
    return;
  }

  gps_callback_t *callback = g_new0(gps_callback_t, 1);
  
  callback->mask = mask;
  callback->cb = cb;
  callback->data = data;
  
  gps_state->callbacks = g_slist_append(gps_state->callbacks, callback);
  
  if(g_slist_length(gps_state->callbacks) == 1)
    gps_background_enable(gps_state, TRUE);
}

void gps_unregister_callback(gps_state_t *gps_state, gps_cb cb) {
  /* find callback in list */
  GSList *list = g_slist_find_custom(gps_state->callbacks, cb, compare);
  g_assert(list);

  /* and de-chain and free it */
  g_free(list->data);
  gps_state->callbacks = g_slist_remove(gps_state->callbacks, list->data);

  if(g_slist_length(gps_state->callbacks) == 0)
    gps_background_enable(gps_state, FALSE);
}

static void gps_unregister_all(gps_state_t *gps_state) {
  g_slist_foreach(gps_state->callbacks, (GFunc)g_free, NULL);
  g_slist_free(gps_state->callbacks);
}


