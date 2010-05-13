/*
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

#include <stdlib.h>

#ifdef USE_MAEMO
#include <libosso.h>
#endif

#include "config.h"
#include "menu.h"
#include "track.h"
#include "geonames.h"
#include "hxm.h"

#ifdef MAEMO5
#include <gdk/gdkx.h>
#include <X11/Xatom.h>
#endif

#include <locale.h>

/* any defined key enables key support */
#if (defined(MAP_KEY_FULLSCREEN) || \
     defined(MAP_KEY_ZOOMIN) || \
     defined(MAP_KEY_ZOOMOUT) || \
     defined(MAP_KEY_UP) || \
     defined(MAP_KEY_DOWN) || \
     defined(MAP_KEY_LEFT) || \
     defined(MAP_KEY_RIGHT))
#include <gdk/gdkkeysyms.h>
#endif

#include <locale.h>
#include <libintl.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

#include "osm-gps-map.h"
#include "osm-gps-map-osd-classic.h"
#include "converter.h"
#include "misc.h"

#include "gps.h"

#define _(String) gettext(String)
#define N_(String) (String)

/* default values */
#define MAP_SOURCE  OSM_GPS_MAP_SOURCE_OPENCYCLEMAP

#define PROXY_KEY  "/system/http_proxy/"


/* we need to tell the map widget about possible network proxies */
/* so the network transfer can pass these */
static const char *get_proxy_uri(void) {
  static char proxy_buffer[64] = "";
  
  /* use environment settings if preset (e.g. for scratchbox) */
  const char *proxy = g_getenv("http_proxy");
  if(proxy) return proxy;

  GConfClient *gconf_client = gconf_client_get_default();

  /* ------------- get proxy settings -------------------- */
  if(gconf_client_get_bool(gconf_client, 
			   PROXY_KEY "use_http_proxy", NULL)) {

    /* we can savely ignore things like "ignore_hosts" since we */
    /* are pretty sure not inside the net of one of our map renderers */
    /* (unless the user works at google :-) */
      
    /* get basic settings */
    char *host = 
      gconf_client_get_string(gconf_client, PROXY_KEY "host", NULL);
    if(host) {
      int port =
	gconf_client_get_int(gconf_client, PROXY_KEY "port", NULL);

      snprintf(proxy_buffer, sizeof(proxy_buffer),
	       "http://%s:%u", host, port);

      g_free(host);
    }
    return proxy_buffer;
  }

  return NULL;
}

/* the gconf keys the map state will be stored under */
#define GCONF_KEY_ZOOM       "zoom"
#define GCONF_KEY_SOURCE     "source"
#define GCONF_KEY_LATITUDE   "latitude"
#define GCONF_KEY_LONGITUDE  "longitude"
#define GCONF_KEY_DOUBLEPIX  "double-pixel"

/* save the entire map state into gconf to be able */
/* to restore the state at the next startup */
void map_save_state(GtkWidget *widget) {
  gint zoom, source;
  gfloat lat, lon;
  gboolean dpix;

  /* get state information from map ... */
  g_object_get(OSM_GPS_MAP(widget), 
	       "zoom", &zoom, 
	       "map-source", &source, 
	       "latitude", &lat, "longitude", &lon,
	       "double-pixel", &dpix,
	       NULL);

  /* ... and store it in gconf */
  gconf_set_int(GCONF_KEY_ZOOM, zoom);
  gconf_set_int(GCONF_KEY_SOURCE, source);
  gconf_set_float(GCONF_KEY_LATITUDE, lat);
  gconf_set_float(GCONF_KEY_LONGITUDE, lon);
  gconf_set_bool(GCONF_KEY_DOUBLEPIX, dpix);
}

static int dist2pixel(OsmGpsMap *map, float km) {
  return 1000.0*km/osm_gps_map_get_scale(map);
}

static void
cb_map_gps(osd_button_t but, void *data) {
  OsmGpsMap *map = OSM_GPS_MAP(data);
  struct gps_t *fix = g_object_get_data(G_OBJECT(map), "gps_fix");

  if(but == OSD_GPS && fix) {

    osm_gps_map_set_center(map, fix->latitude, fix->longitude);

    /* re-enable centering */
    g_object_set(map, "auto-center", TRUE, NULL);
  }
}

#ifdef USE_MAEMO
/* http://mathforum.org/library/drmath/view/51722.html */
static float get_distance(coord_t *p1, coord_t *p2) {
  return acos(cos(p1->rlat) * cos(p2->rlat) * cos(p2->rlon - p1->rlon) +
	      sin(p1->rlat) * sin(p2->rlat)) * 6378137.0;
}
#endif

static void gps_callback(gps_mask_t set, struct gps_t *fix, void *data) {
  OsmGpsMap *map = OSM_GPS_MAP(data);
#ifdef USE_MAEMO
  GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(map));
#endif

  gps_mask_t gps_set = 
    (gps_mask_t)g_object_get_data(G_OBJECT(map), "gps_set"); 

  /* ... and enable "goto" button if it's valid */
  if((set & FIX_LATLON_SET) != (gps_set & FIX_LATLON_SET)) {
    osm_gps_map_osd_enable_gps(map,  
       OSM_GPS_MAP_OSD_CALLBACK((set&FIX_LATLON_SET)?cb_map_gps:NULL), map);

    g_object_set_data(G_OBJECT(map), "gps_set", (gpointer)set); 
  }

  if(set & FIX_LATLON_SET) {
    /* save fix in case the user later wants to enable gps */
    g_object_set_data(G_OBJECT(map), "gps_fix", fix); 

    /* get error */
    int radius = (set & FIX_HERR_SET)?dist2pixel(map, fix->eph/1000):0;

    g_object_set(map, "gps-track-highlight-radius", radius, NULL);
    osm_gps_map_draw_gps(map, fix->latitude, fix->longitude, fix->track);

  } else {
    g_object_set_data(G_OBJECT(map), "gps_fix", NULL); 
    osm_gps_map_clear_gps(map);
  }

#ifdef USE_MAEMO
  if((set & FIX_LATLON_SET) && toplevel) {

    /* check if toplevel has focus */
    if(GTK_WIDGET_HAS_FOCUS(toplevel)) {

      /* check gps position every 10 seconds and trigger */
      /* screen saver if useful */
      time_t last = (time_t)g_object_get_data(G_OBJECT(map), "gps_timer"); 
      time_t now = time(NULL);
      if(now-last > 10) {
	/* get last saved position */
	coord_t *last_pos = g_object_get_data(G_OBJECT(map), "gps_pos"); 
	coord_t cur = { .rlat = deg2rad(fix->latitude),
			.rlon = deg2rad(fix->longitude) };
	
	if(last_pos) {
	  /* consider everything under 3 kph (~2 mph) to be static and */
	  /* disable (trigger) screensaver only above this. 3kph = ~8m/10sec */
	  
	  /* compare with reported position */
	  if(get_distance(last_pos, &cur) > 8) {
	    printf("trigger screen saver after %d ...\n", (int)(now-last));
	    
	    osso_context_t *osso_context =  
	      g_object_get_data(G_OBJECT(toplevel), "osso-context");
	    g_assert(osso_context);
	    
	    osso_display_blanking_pause(osso_context);
	    
	  } else
	    printf("too slow, no trigger\n");
	} else
	  last_pos = g_new0(coord_t, 1);
	
	/* make current position the old one */
	last_pos->rlat = cur.rlat;
	last_pos->rlon = cur.rlon;
	
	g_object_set_data(G_OBJECT(map), "gps_timer", (gpointer)now); 
	g_object_set_data(G_OBJECT(map), "gps_pos", last_pos); 
      }
    }
  }
#endif

}

static gboolean on_focus_change(GtkWidget *widget, GdkEventFocus *event,
				gpointer user_data) {
  GtkWidget *map = GTK_WIDGET(user_data);

  gps_state_t *gps = g_object_get_data(G_OBJECT(map), "gps_state");
  g_assert(gps);

  printf("map focus-%s event\n", event->in?"in":"out");

  /* disconnect from gps if map looses focus */
  /* this is to save energy if maep runs in background */
  
  if(event->in) {
    /* request all GPS information required for map display */
    gps_register_callback(gps, LATLON_CHANGED | TRACK_CHANGED | HERR_CHANGED, 
			  gps_callback, map);
  } else
    gps_unregister_callback(gps, gps_callback);

  return TRUE;
}

static GtkWidget *map_new(GtkWidget *window) {
  /* It is recommanded that all applications share these same */
  /* map path, so data is only cached once. The path should be: */
  /* ~/.osm-gps-map on standard PC     (users home) */
  /* /home/user/MyDocs/.maps on Maemo5 (vfat on internal card) */
  /* /media/mmc2/osm-gps-map on Maemo4 (vfat on internal card) */
  char *path;

#if !defined(USE_MAEMO)
  char *p = getenv("HOME");
  if(!p) p = "/tmp"; 
  path = g_strdup_printf("%s/.osm-gps-map", p);
#else
#if MAEMO_VERSION_MAJOR == 5
  /* early maep releases used the ext3 for the tile cache */
#define OLD_PATH "/home/user/.osm-gps-map"
#define NEW_PATH "/home/user/MyDocs/.maps"
  /* check if the old path exists, and is not a symlink, then use it */
  if( g_file_test(OLD_PATH, G_FILE_TEST_IS_DIR) &&
     !g_file_test(OLD_PATH, G_FILE_TEST_IS_SYMLINK))
    path = g_strdup(OLD_PATH);
  else
    path = g_strdup(NEW_PATH);
#else
  path = g_strdup("/media/mmc2/osm-gps-map");
#endif
#endif
  gint source = gconf_get_int(GCONF_KEY_SOURCE, MAP_SOURCE);

  const char *proxy = get_proxy_uri();

  /* get zoom, latitude and longitude from gconf if possible */
  gint zoom = gconf_get_int(GCONF_KEY_ZOOM, 3);
  gfloat lat = gconf_get_float(GCONF_KEY_LATITUDE, 50.0);
  gfloat lon = gconf_get_float(GCONF_KEY_LONGITUDE, 21.0);
  gboolean dpix = gconf_get_bool(GCONF_KEY_DOUBLEPIX, FALSE);
    
  GtkWidget *widget = g_object_new(OSM_TYPE_GPS_MAP,
		 "map-source",               source,
		 "tile-cache",               OSM_GPS_MAP_CACHE_FRIENDLY,
		 "tile-cache-base",          path,
		 "auto-center",              FALSE,
		 "record-trip-history",      FALSE, 
		 "show-trip-history",        FALSE, 
		 "gps-track-point-radius",   10,
		 proxy?"proxy-uri":NULL,     proxy,
		 "drag-limit",               MAP_DRAG_LIMIT,
		 "double-pixel",             dpix,
		 NULL);

  g_free(path);

  OsmGpsMap *map = OSM_GPS_MAP(widget);

#ifdef MAP_KEY_FULLSCREEN
  osm_gps_map_set_keyboard_shortcut(map, OSM_GPS_MAP_KEY_FULLSCREEN, MAP_KEY_FULLSCREEN);
#endif
#ifdef MAP_KEY_ZOOMIN
  osm_gps_map_set_keyboard_shortcut(map, OSM_GPS_MAP_KEY_ZOOMIN, MAP_KEY_ZOOMIN);
#endif
#ifdef MAP_KEY_ZOOMOUT
  osm_gps_map_set_keyboard_shortcut(map, OSM_GPS_MAP_KEY_ZOOMOUT, MAP_KEY_ZOOMOUT);
#endif
#ifdef MAP_KEY_UP
  osm_gps_map_set_keyboard_shortcut(map, OSM_GPS_MAP_KEY_UP, MAP_KEY_UP);
#endif
#ifdef MAP_KEY_DOWN
  osm_gps_map_set_keyboard_shortcut(map, OSM_GPS_MAP_KEY_DOWN, MAP_KEY_DOWN);
#endif
#ifdef MAP_KEY_LEFT
  osm_gps_map_set_keyboard_shortcut(map, OSM_GPS_MAP_KEY_LEFT, MAP_KEY_LEFT);
#endif
#ifdef MAP_KEY_RIGHT
  osm_gps_map_set_keyboard_shortcut(map, OSM_GPS_MAP_KEY_RIGHT, MAP_KEY_RIGHT);
#endif

  osm_gps_map_set_mapcenter(OSM_GPS_MAP(widget), lat, lon, zoom);

  osm_gps_map_osd_classic_init(OSM_GPS_MAP(widget));

  /* connect to GPS */
  gps_state_t *gps = gps_init();

  /* request all GPS information required for map display */
  gps_register_callback(gps, LATLON_CHANGED | TRACK_CHANGED | HERR_CHANGED, 
			gps_callback, widget);
  g_object_set_data(G_OBJECT(widget), "gps_state", gps);

  g_signal_connect(G_OBJECT(window), "focus-in-event", 
		   G_CALLBACK(on_focus_change), widget);
  
  g_signal_connect(G_OBJECT(window), "focus-out-event", 
		   G_CALLBACK(on_focus_change), widget);
  

  osm_gps_map_osd_draw_hr (OSM_GPS_MAP(widget), 299);
  return widget;
}

#if defined(MAEMO5) && !defined(__i386__)
/* get access to zoom buttons */
static void
on_window_realize(GtkWidget *widget, gpointer data) {
  if (widget->window) {
    unsigned char value = 1;
    Atom hildon_zoom_key_atom = 
      gdk_x11_get_xatom_by_name("_HILDON_ZOOM_KEY_ATOM"),
      integer_atom = gdk_x11_get_xatom_by_name("INTEGER");
    Display *dpy = 
      GDK_DISPLAY_XDISPLAY(gdk_drawable_get_display(widget->window));
    Window w = GDK_WINDOW_XID(widget->window);

    XChangeProperty(dpy, w, hildon_zoom_key_atom, 
		    integer_atom, 8, PropModeReplace, &value, 1);
  }
}
#endif

static void on_window_destroy (GtkWidget *widget, gpointer data) {
#ifdef USE_MAEMO
  osso_context_t *osso_context =  g_object_get_data(G_OBJECT(widget), "osso-context");
  g_assert(osso_context);

  if(osso_context)
    osso_deinitialize(osso_context);
#endif

  gtk_main_quit();
}

static void hxm_callback(hxm_t *hxm, void *data) {
  printf("callback with %u!!!\n", hxm->hr);
}

void hxm_enable(GtkWidget *map, gboolean enable) {
  printf("%sabling heart rate capture\n", enable?"en":"dis");

  /* verify that tracking isn't already in the requested state */
  gboolean cur_state = 
    (gboolean)g_object_get_data(G_OBJECT(map), HXM_ENABLED);

  g_assert(cur_state != enable);

  /* save new tracking state */
  g_object_set_data(G_OBJECT(map), HXM_ENABLED, (gpointer)enable);

  hxm_t *hxm = g_object_get_data(G_OBJECT(map), "hxm");

  if(enable) {
    g_assert(!hxm);

    hxm = hxm_init();
    hxm_register_callback(hxm, hxm_callback, map);
    g_object_set_data(G_OBJECT(map), "hxm", hxm);
  } else {
    g_assert(hxm);

    g_object_set_data(G_OBJECT(map), "hxm", NULL);
    hxm_release(hxm);
  }
}

static void on_map_destroy (GtkWidget *widget, gpointer data) {
  track_save(widget);

  coord_t *last_pos = g_object_get_data(G_OBJECT(widget), "gps_pos"); 
  if(last_pos) g_free(last_pos);

  gps_state_t *state = g_object_get_data(G_OBJECT(widget), "gps_state");
  g_assert(state);
  gps_release(state);

  hxm_t *hxm = g_object_get_data(G_OBJECT(widget), "hxm");
  if(hxm) {
    g_object_set_data(G_OBJECT(widget), "hxm", NULL);
    hxm_release(hxm);
  }

  gconf_set_bool(HXM_ENABLED, 
    (gboolean)g_object_get_data(G_OBJECT(widget), HXM_ENABLED));

  map_save_state(widget);
}

int main(int argc, char *argv[]) {

#if 0
  setlocale(LC_ALL, "");
  bindtextdomain(PACKAGE, LOCALEDIR);
  bind_textdomain_codeset(PACKAGE, "UTF-8");
  textdomain(PACKAGE);
#endif

  /* prepare thread system. the map uses soup which in turn seems */
  /* to need this */
  g_thread_init(NULL);

  gtk_init (&argc, &argv);

#ifdef USE_MAEMO
  /* Create the hildon program and setup the title */
  HildonProgram *program = HILDON_PROGRAM(hildon_program_get_instance());
  g_set_application_name("Mæp");
  
  /* Create HildonWindow and set it to HildonProgram */
#ifdef MAEMO5
  GtkWidget *window = hildon_stackable_window_new();
#else
  GtkWidget *window = hildon_window_new();
#endif

  hildon_program_add_window(program, HILDON_WINDOW(window));

  g_object_set_data(G_OBJECT(window), "osso-context",
	    osso_initialize("org.harbaum."APP, VERSION, TRUE, NULL));

#if MAEMO_VERSION_MAJOR >= 5
  gtk_window_set_title(GTK_WINDOW(window), "Mæp");
#ifndef __i386__
  g_signal_connect(G_OBJECT(window), "realize", 
		   G_CALLBACK(on_window_realize), NULL);

#endif
#endif // MAEMO_VERSION

#else
  /* Create a Window. */
  GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  /* Set a decent default size for the window. */
  gtk_window_set_default_size(GTK_WINDOW(window), 640, 480);

  gtk_window_set_title(GTK_WINDOW(window), "Mæp");
#endif

  g_signal_connect(G_OBJECT(window), "destroy", 
		   G_CALLBACK(on_window_destroy), NULL);

  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(window), vbox);

  /* create map widget */
  GtkWidget *map = map_new(window);

  /* attach menu to main window */
  menu_create(vbox, map);

  g_signal_connect(G_OBJECT(map), "destroy", 
		   G_CALLBACK(on_map_destroy), NULL);

  gtk_box_pack_start_defaults(GTK_BOX(vbox), map);

  /* heart rate data */
  if(gconf_get_bool(HXM_ENABLED, FALSE)) 
    menu_check_set_active(window, "Heart Rate", TRUE);
  
  track_restore(map);
  geonames_wikipedia_restore(map);

  gtk_widget_show_all(GTK_WIDGET(window));
  gtk_main();

  return 0;
}
