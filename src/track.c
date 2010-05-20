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

#define _XOPEN_SOURCE /* glibc2 needs this */
#define __USE_XOPEN
#include <time.h>

#include "config.h"
#include "track.h"
#include "osm-gps-map.h"
#include "converter.h"
#include "misc.h"
#include "menu.h"
#include "gps.h"
#include "graph.h"
#include "hxm.h"

#include <stdio.h>
#include <math.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>
#include <strings.h>

#define DATE_FORMAT "%FT%T"
#define TRACK_CAPTURE_ENABLED "track_capture_enabled"
#define TRACK_CAPTURE_LAST    "track_capture_last"

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

#ifdef USE_MAEMO
#define GTK_FM_OK  GTK_RESPONSE_OK
#else
#define GTK_FM_OK  GTK_RESPONSE_ACCEPT
#endif

#ifdef USE_MAEMO
#include <hildon/hildon-file-chooser-dialog.h>
#endif

/* --------------------------------------------------------------- */

static void track_point_free(track_point_t *point) {
  g_free(point);
}

static void track_seg_free(track_seg_t *seg) {
  track_point_t *point = seg->track_point;
  while(point) {
    track_point_t *next = point->next;
    track_point_free(point);
    point = next;
  }

  g_free(seg);
}

static void track_free(track_t *trk) {
  if(trk->name) g_free(trk->name);

  track_seg_t *seg = trk->track_seg;
  while(seg) {
    track_seg_t *next = seg->next;
    track_seg_free(seg);
    seg = next;
  }

  g_free(trk);
}

static void track_state_free(track_state_t *track_state) {

  /* stop running timeout timer if present */
  if(track_state->timer_handler) {
    printf("TRACK: removing timeout\n");
    gtk_timeout_remove(track_state->timer_handler);
    track_state->timer_handler = 0;
  }

  track_t *track = track_state->track;
  while(track) {
    track_t *next = track->next;
    track_free(track);
    track = next;
  }

  g_free(track_state);
}

static void filemgr_setup(GtkWidget *dialog, gboolean save) {
  char *track_path = gconf_get_string("track_path");

  if(track_path) {
    if(!g_file_test(track_path, G_FILE_TEST_IS_REGULAR)) {
      char *last_sep = strrchr(track_path, '/');
      if(last_sep) {
	*last_sep = 0;  // seperate path from file 
	
	/* the user just created a new document */
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), 
					    track_path);
	if(save)
	  gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), 
					    last_sep+1);
      }
    } else 
      gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), track_path);
  }
}

static gboolean track_get_prop_pos(xmlNode *node, coord_t *pos) {
  char *str_lat = (char*)xmlGetProp(node, BAD_CAST "lat");
  char *str_lon = (char*)xmlGetProp(node, BAD_CAST "lon");

  if(!str_lon || !str_lat) {
    if(!str_lon) xmlFree(str_lon);
    if(!str_lat) xmlFree(str_lat);
    return FALSE;
  }

  pos->rlat = deg2rad(g_ascii_strtod(str_lat, NULL));
  pos->rlon = deg2rad(g_ascii_strtod(str_lon, NULL));

  xmlFree(str_lon);
  xmlFree(str_lat);

  return TRUE;
}

static void track_parse_tpext(xmlDocPtr doc, xmlNode *a_node, 
			      track_point_t *point) {
  
  /* scan for children */
  xmlNode *cur_node = NULL;
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {

      /* heart rate */
      if(strcasecmp((char*)cur_node->name, "hr") == 0) {
	char *str = (char*)xmlNodeGetContent(cur_node);
	point->hr = g_ascii_strtod(str, NULL);
 	xmlFree(str);
      }

      /* cadence */
      if(strcasecmp((char*)cur_node->name, "cad") == 0) {
	char *str = (char*)xmlNodeGetContent(cur_node);
	point->cad = g_ascii_strtod(str, NULL);
 	xmlFree(str);
      }
    }
  }
}

static void track_parse_ext(xmlDocPtr doc, xmlNode *a_node, 
			    track_point_t *point) {

  /* scan for children */
  xmlNode *cur_node = NULL;
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {

      if(strcasecmp((char*)cur_node->name, "TrackPointExtension") == 0) 
	track_parse_tpext(doc, cur_node, point); 
    }
  }
}

static track_point_t *track_parse_trkpt(xmlDocPtr doc, xmlNode *a_node) {
  track_point_t *point = g_new0(track_point_t, 1);
  point->altitude = NAN;
  point->speed = NAN;
  point->hr = NAN;
  point->cad = NAN;
  point->coord.rlat = NAN;
  point->coord.rlon = NAN;

  /* parse position */
  if(!track_get_prop_pos(a_node, &point->coord)) {
    g_free(point);
    return NULL;
  }

  /* scan for children */
  xmlNode *cur_node = NULL;
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {

      /* elevation (altitude) */
      if(strcasecmp((char*)cur_node->name, "ele") == 0) {
	char *str = (char*)xmlNodeGetContent(cur_node);
	point->altitude = g_ascii_strtod(str, NULL);
 	xmlFree(str);
      }

      /* time */
      if(strcasecmp((char*)cur_node->name, "time") == 0) {
	struct tm time;
	char *ptr, *str = (char*)xmlNodeGetContent(cur_node);

	/* mktime may adjust the time zone settings which in turn affect */
	/* strptime. Doing this twice is an ugly hack, but solves the */
	/* problem */
	ptr = strptime(str, DATE_FORMAT, &time);
	if(ptr) point->time = mktime(&time);
	ptr = strptime(str, DATE_FORMAT, &time);
	if(ptr) point->time = mktime(&time);
 	xmlFree(str);
      }

      /* extensions */
      if(strcasecmp((char*)cur_node->name, "extensions") == 0)
	track_parse_ext(doc, cur_node, point);
    }
  }

  return point;
}

static void 
track_parse_trkseg(track_t *track, xmlDocPtr doc, xmlNode *a_node) {
  xmlNode *cur_node = NULL;
  track_point_t **point = NULL;
  track_seg_t **seg = &(track->track_seg);

  /* search end of track_seg list */
  while(*seg) seg = &((*seg)->next);

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "trkpt") == 0) {
	track_point_t *cpnt = track_parse_trkpt(doc, cur_node);
	if(cpnt) {
	  if(!point) {
	    /* start a new segment */
	    *seg = g_new0(track_seg_t, 1);
	    point = &((*seg)->track_point);
	  }
	  /* attach point to chain */
	  *point = cpnt;
	  point = &((*point)->next);
	} else {
	  /* end segment if point could not be parsed and start a new one */
	  /* close segment if there is one */
	  if(point) {
	    seg = &((*seg)->next);
	    point = NULL;
	  }
	}
      } else
	printf("found unhandled gpx/trk/trkseg/%s\n", cur_node->name);
    }
  }
}

static track_t *track_parse_trk(xmlDocPtr doc, xmlNode *a_node) {
  track_t *track = g_new0(track_t, 1);
  xmlNode *cur_node = NULL;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "name") == 0) {
	char *str = (char*)xmlNodeGetContent(cur_node);
	if(str && !track->name) track->name = g_strdup(str);
	xmlFree(str);
      } else if(strcasecmp((char*)cur_node->name, "trkseg") == 0) {
	track_parse_trkseg(track, doc, cur_node);
      } else
	printf("found unhandled gpx/trk/%s\n", cur_node->name);
      
    }
  }
  return track;
}

static track_state_t *track_parse_gpx(xmlDocPtr doc, xmlNode *a_node) {
  track_state_t *track_state = g_new0(track_state_t, 1);
  track_t **track = &(track_state->track);
  xmlNode *cur_node = NULL;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "trk") == 0) {
	*track = track_parse_trk(doc, cur_node);
	if(*track) {
	  /* check if track really contains segments */
	  if(!(*track)->track_seg)
	    track_free(*track);
	  else
	    track = &(*track)->next;
	}
      } else
	printf("found unhandled gpx/%s\n", cur_node->name);      
    }
  }
  return track_state;
}

/* parse root element and search for "track" */
static track_state_t *track_parse_root(xmlDocPtr doc, xmlNode *a_node) {
  track_state_t *track_state = NULL;
  xmlNode *cur_node = NULL;

  for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      /* parse track file ... */
      if(strcasecmp((char*)cur_node->name, "gpx") == 0) 
      	track_state = track_parse_gpx(doc, cur_node);
      else 
	printf("found unhandled %s\n", cur_node->name);
    }
  }
  return track_state;
}

static track_state_t *track_parse_doc(xmlDocPtr doc) {
  track_state_t *track_state;

  /* Get the root element node */
  xmlNode *root_element = xmlDocGetRootElement(doc);

  track_state = track_parse_root(doc, root_element);  

  /*free the document */
  xmlFreeDoc(doc);

  /*
   * Free the global variables that may
   * have been allocated by the parser.
   */
  xmlCleanupParser();

  return track_state;
}

static void track_state_save(track_state_t *track_state);

static gboolean track_autosave(gpointer data) {
  track_state_t *track_state = (track_state_t *)data;

  printf("TRACK: autosave\n");

  track_state_save(track_state);

  return FALSE;
}

static track_state_t *track_read(char *filename, gboolean is_restore) {
  xmlDoc *doc = NULL;

  LIBXML_TEST_VERSION;
  
  /* parse the file and get the DOM */
  if((doc = xmlReadFile(filename, NULL, 0)) == NULL) {
    xmlErrorPtr	errP = xmlGetLastError();
    printf("While parsing \"%s\":\n\n%s\n", filename, errP->message);
    return NULL;
  }

  track_state_t *track_state = track_parse_doc(doc); 

  if(!track_state || !track_state->track) {
    printf("track was empty/invalid track\n");
    return NULL;
  }

  if(!track_state->dirty && !is_restore) { 
    printf("TRACK: adding timeout\n");

    g_assert(!track_state->timer_handler);
    track_state->timer_handler = gtk_timeout_add(5*1000, track_autosave, track_state);
    track_state->dirty = TRUE;
  }

  return track_state;
}

void track_draw(GtkWidget *map, track_state_t *track_state) {
  /* erase any previous track */
  track_clear(map);

  if(!track_state) return;

  track_t *track = track_state->track;
  while(track) {
    track_seg_t *seg = track->track_seg;
    while(seg) {
      GSList *points = NULL;
      track_point_t *point = seg->track_point;
    
      while(point) {
	/* we need to create a copy of the coordinate since */
	/* the map will free them */
	coord_t *new_point = g_memdup(&point->coord, sizeof(coord_t));
	points = g_slist_append(points, new_point);
	point = point->next;
      }
      osm_gps_map_add_track(OSM_GPS_MAP(map), points);

      seg = seg->next;    
    }
    track = track->next;    
  }

  /* save track reference in map */
  g_object_set_data(G_OBJECT(map), "track_state", track_state);

  GtkWidget *toplevel = gtk_widget_get_toplevel(map);
  menu_enable(toplevel, "Track/Clear", TRUE);
  menu_enable(toplevel, "Track/Export", TRUE);
  menu_enable(toplevel, "Track/Graph", TRUE);
}

/* this imports a track and adds it to the set of existing tracks */
void track_import(GtkWidget *map) {
  GtkWidget *toplevel = gtk_widget_get_toplevel(map);
  
  /* open a file selector */
  GtkWidget *dialog;

  track_state_t *track_state = NULL;
  
#ifdef USE_MAEMO
  dialog = hildon_file_chooser_dialog_new(GTK_WINDOW(toplevel), 
					  GTK_FILE_CHOOSER_ACTION_OPEN);
#else
  dialog = gtk_file_chooser_dialog_new (_("Import track file"),
			GTK_WINDOW(toplevel),
			GTK_FILE_CHOOSER_ACTION_OPEN,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
			NULL);
#endif

  filemgr_setup(dialog, FALSE);
  gtk_widget_show_all (GTK_WIDGET(dialog));
  if (gtk_dialog_run (GTK_DIALOG(dialog)) == GTK_FM_OK) {
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

    /* load a track */
    track_state = track_read(filename, FALSE);
    if(track_state) 
      gconf_set_string("track_path", filename);

    g_free(filename);

    track_draw(map, track_state);
  }

  gtk_widget_destroy (dialog);
}

static void track_point_new(GtkWidget *map, track_point_t *new_point) {

  /* update visual representation */
  GSList *points = (GSList*)g_object_get_data(G_OBJECT(map), "track_current_draw");
  
  /* append new point */
  coord_t *tmp_point = g_memdup(&(new_point->coord), sizeof(coord_t));
  GSList *new_points = g_slist_append(points, tmp_point);
  
  osm_gps_map_replace_track(OSM_GPS_MAP(map), points, new_points);
  g_object_set_data(G_OBJECT(map), "track_current_draw", new_points);
  
  /* get current segment */
  track_seg_t *seg = g_object_get_data(G_OBJECT(map), "track_current_segment");
  
  if(!seg) {
    /* append a new segment */
    track_state_t *track_state = g_object_get_data(G_OBJECT(map), "track_state");
    if(!track_state) {
      /* no tracks at all so far -> enable menu */
      GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(map));
      menu_enable(toplevel, "Track/Clear", TRUE);
      menu_enable(toplevel, "Track/Export", TRUE);
      
      printf("track: creating new track\n");
      track_state = g_new0(track_state_t, 1);
      g_object_set_data(G_OBJECT(map), "track_state", track_state);
    }
    
    if(!track_state->dirty) {
      g_assert(!track_state->timer_handler);
      track_state->timer_handler = gtk_timeout_add(60*5*1000, track_autosave, track_state);
      track_state->dirty = TRUE;
    }
    
    printf("track: now active segment, starting new one\n");

    /* get last track, create one if none present */
    track_t *track = track_state->track;
    if(!track) {
      printf("track: no track so far, creating new track\n");

      track = track_state->track = g_new0(track_t, 1);

      time_t tval = time(NULL);
      struct tm *loctime = localtime(&tval);
      char str[64];
      strftime(str, sizeof(str), "Track started %x %X", loctime);
      track->name = g_strdup(str);
    }
    
    /* search last segment */
    if((seg = track->track_seg)) {
      /* append to existing chain */
      while(seg->next) seg = seg->next;
      seg = (seg->next = g_new0(track_seg_t, 1));
    } else
      /* create new chain */
      seg = track->track_seg = g_new0(track_seg_t, 1);
  }
  
  printf("gps: creating new point\n");
  track_point_t *point = seg->track_point;
  if(point) {
    /* append to existing chain */
    while(point->next) point = point->next;
    point = (point->next = new_point);
  } else
    /* create new chain */
    point = seg->track_point = new_point;
  
  g_object_set_data(G_OBJECT(map), "track_current_segment", seg);
  
  /* if track length just became 2, then also enable the graph */
  track_state_t *track_state = g_object_get_data(G_OBJECT(map), "track_state");
  if(track_length(track_state) == 2) {
    GtkWidget *toplevel = gtk_widget_get_toplevel(map);
    menu_enable(toplevel, "Track/Graph", TRUE);
  }
}

void track_clear(GtkWidget *map) {
  track_state_t *track_state = g_object_get_data(G_OBJECT(map), "track_state");

  g_object_set_data(G_OBJECT(map), "track_current", NULL);

  GtkWidget *toplevel = gtk_widget_get_toplevel(map);
  menu_enable(toplevel, "Track/Clear", FALSE);
  menu_enable(toplevel, "Track/Export", FALSE);
  menu_enable(toplevel, "Track/Graph", FALSE);

  osm_gps_map_clear_tracks(OSM_GPS_MAP(map));

  /* also remove everything related to track currently being drawn */
  g_object_set_data(G_OBJECT(map), "track_current_draw", NULL);
  g_object_set_data(G_OBJECT(map), "track_current_segment", NULL);

  if (!track_state) return;

  g_object_set_data(G_OBJECT(map), "track_state", NULL);
  track_state_free(track_state);

  /* if we are still captureing a track and we have a valid fix, then */
  /* start a new one immediately */
  if((gboolean)g_object_get_data(G_OBJECT(map), TRACK_CAPTURE_ENABLED)) {
    track_point_t *last = g_object_get_data(G_OBJECT(map), TRACK_CAPTURE_LAST);

    if(last) track_point_new(GTK_WIDGET(map), g_memdup(last, sizeof(track_point_t)));
  }
}

/* this callback is called from the gps layer as long as */
/* captureing is enabled */
static void gps_callback(gps_mask_t set, struct gps_t *fix, void *data) {
  OsmGpsMap *map = OSM_GPS_MAP(data);

  track_point_t *last = g_object_get_data(G_OBJECT(map), TRACK_CAPTURE_LAST);

  /* save point as we may need it later to (re-)enable caturing */
  if((set & FIX_LATLON_SET) && (fix->eph < 100)) {

    /* create storage if not present yet */
    if(!last) last = g_new0(track_point_t, 1);
    
    /* save current position for later use */
    last->altitude = fix->altitude;
    last->speed = NAN;
    last->hr = NAN;
    last->cad = NAN;
    last->time = time(NULL);
    last->coord.rlat = deg2rad(fix->latitude);
    last->coord.rlon = deg2rad(fix->longitude);

    /* add hxm data if available */
    hxm_t *hxm = g_object_get_data(G_OBJECT(map), "hxm");
    if(hxm && (time(NULL) - hxm->time) < 5) {
      last->hr  = hxm->hr;
      last->cad = hxm->cad;
    }

    g_object_set_data(G_OBJECT(map), TRACK_CAPTURE_LAST, last);
  } else {
    /* no fix -> forget about last point */
    track_point_free(last);
    g_object_set_data(G_OBJECT(map), TRACK_CAPTURE_LAST, NULL);
  }
  
  if(!(gboolean)g_object_get_data(G_OBJECT(map), TRACK_CAPTURE_ENABLED)) 
     return;

  /* save gps position in track */
  if(set & FIX_LATLON_SET) {
    track_point_t *point = g_new0(track_point_t, 1);
    point->altitude = fix->altitude;
    point->time = time(NULL);
    point->speed = NAN;
    point->hr = NAN;
    point->cad = NAN;
    point->coord.rlat = deg2rad(fix->latitude);
    point->coord.rlon = deg2rad(fix->longitude);

    /* add hxm data if available */
    hxm_t *hxm = g_object_get_data(G_OBJECT(map), "hxm");
    if(hxm && (time(NULL) - hxm->time) < 5) {
      point->hr  = hxm->hr;
      point->cad = hxm->cad;
    }

    track_point_new(GTK_WIDGET(map), point);
  } else {
    g_object_set_data(G_OBJECT(map), "track_current_draw", NULL);
    g_object_set_data(G_OBJECT(map), "track_current_segment", NULL);
  }
}

void track_capture_enable(GtkWidget *map, gboolean enable) {
  printf("%sabling track capture\n", enable?"en":"dis");

  /* verify that tracking isn't already in the requested state */
  gboolean cur_state = 
    (gboolean)g_object_get_data(G_OBJECT(map), TRACK_CAPTURE_ENABLED);

  g_assert(cur_state != enable);

  /* save new tracking state */
  g_object_set_data(G_OBJECT(map), TRACK_CAPTURE_ENABLED, (gpointer)enable);

  gps_state_t *gps_state = g_object_get_data(G_OBJECT(map), "gps_state");

  if(enable) {
    track_point_t *last = g_object_get_data(G_OBJECT(map), TRACK_CAPTURE_LAST);

    /* if a "last" point exists, use it as the start of the new track */
    if(last) 
      track_point_new(GTK_WIDGET(map), g_memdup(last, sizeof(track_point_t)));

    /* request all GPS information required for track capturing */
    gps_register_callback(gps_state, LATLON_CHANGED | ALTITUDE_CHANGED, 
			  gps_callback, map);
  } else {
    /* stop all visual things */
    g_object_set_data(G_OBJECT(map), "track_current_draw", NULL);
    g_object_set_data(G_OBJECT(map), "track_current_segment", NULL);

    gps_unregister_callback(gps_state, gps_callback);
  }
}

/* ----------------------  saving track --------------------------- */

void track_save_points(track_point_t *point, xmlNodePtr node) {
  while(point) {
    char str[G_ASCII_DTOSTR_BUF_SIZE];

    xmlNodePtr node_point = xmlNewChild(node, NULL, BAD_CAST "trkpt", NULL);

    g_ascii_formatd(str, sizeof(str), "%.07f", rad2deg(point->coord.rlat));
    xmlNewProp(node_point, BAD_CAST "lat", BAD_CAST str);
    
    g_ascii_formatd(str, sizeof(str), "%.07f", rad2deg(point->coord.rlon));
    xmlNewProp(node_point, BAD_CAST "lon", BAD_CAST str);

    if(!isnan(point->altitude)) {
      g_ascii_formatd(str, sizeof(str), "%.02f", point->altitude);
      xmlNewTextChild(node_point, NULL, BAD_CAST "ele", BAD_CAST str);
    }

    if(!isnan(point->hr) || !isnan(point->cad)) {
      xmlNodePtr ext = 
	xmlNewChild(node_point, NULL, BAD_CAST "extensions", NULL);

      xmlNodePtr tpext = 
	xmlNewChild(ext, NULL, BAD_CAST "gpxtpx:TrackPointExtension", NULL);

      if(!isnan(point->hr)) {
	char *lstr = g_strdup_printf("%u", (unsigned)point->hr);
	xmlNewTextChild(tpext, NULL, BAD_CAST "gpxtpx:hr", BAD_CAST lstr);
	g_free(lstr);
      }

      if(!isnan(point->cad)) {
	char *lstr = g_strdup_printf("%u", (unsigned)point->cad);
	xmlNewTextChild(tpext, NULL, BAD_CAST "gpxtpx:cad", BAD_CAST lstr);
	g_free(lstr);
      }
    }

    if(point->time) {
      strftime(str, sizeof(str), DATE_FORMAT, localtime(&point->time));
      xmlNewTextChild(node_point, NULL, BAD_CAST "time", BAD_CAST str);
    }

    point = point->next;
  }
}

void track_save_segs(track_seg_t *seg, xmlNodePtr node) {
  while(seg) {
    xmlNodePtr node_seg = xmlNewChild(node, NULL, BAD_CAST "trkseg", NULL);
    track_save_points(seg->track_point, node_seg);
    seg = seg->next;
  }
}

void track_save_tracks(track_t *track, xmlNodePtr node) {
  while(track) {
    xmlNodePtr trk_node = xmlNewChild(node, NULL, BAD_CAST "trk", NULL);
    if(track->name) 
      xmlNewTextChild(trk_node, NULL, BAD_CAST "name", BAD_CAST track->name);

    track_save_segs(track->track_seg, trk_node);
    track = track->next;
  }
}

void track_write(char *name, track_state_t *track_state) {
  LIBXML_TEST_VERSION;
 
  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root_node = xmlNewNode(NULL, BAD_CAST "gpx");
  xmlNewProp(root_node, BAD_CAST "version", BAD_CAST "1.0");
  xmlNewProp(root_node, BAD_CAST "creator", BAD_CAST PACKAGE " v" VERSION);
  xmlNewProp(root_node, BAD_CAST "xmlns", BAD_CAST 
	     "http://www.topografix.com/GPX/1/0/gpx.xsd");

  /* add TrackPointExtension only if it will actually be used */
  if(track_contents(track_state) & (TRACK_HR | TRACK_CADENCE))
    xmlNewProp(root_node, BAD_CAST "xmlns:gpxtpx", BAD_CAST 
	       "http://www.garmin.com/xmlschemas/TrackPointExtension/v1");

  xmlDocSetRootElement(doc, root_node);

  track_save_tracks(track_state->track, root_node);

  xmlSaveFormatFileEnc(name, doc, "UTF-8", 1);
  xmlFreeDoc(doc);
  xmlCleanupParser();

  track_state->dirty = FALSE;

  if(track_state->timer_handler) {
    gtk_timeout_remove(track_state->timer_handler);
    track_state->timer_handler = 0;
  }
}

void track_export(GtkWidget *map) {
  GtkWidget *toplevel = gtk_widget_get_toplevel(map);
  track_state_t *track_state = g_object_get_data(G_OBJECT(map), "track_state");

  /* the menu should be disabled when no track is present */
  g_assert(track_state);

  /* open a file selector */
  GtkWidget *dialog;

#ifdef USE_MAEMO
  dialog = hildon_file_chooser_dialog_new(GTK_WINDOW(toplevel), 
					  GTK_FILE_CHOOSER_ACTION_SAVE);
#else
  dialog = gtk_file_chooser_dialog_new(_("Export track file"),
				       GTK_WINDOW(toplevel),
				       GTK_FILE_CHOOSER_ACTION_SAVE,
				       GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
				       GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
				       NULL);
#endif

  filemgr_setup(dialog, TRUE);

  if(gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_FM_OK) {
    gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
    if(filename) {
      printf("export to %s\n", filename);

      if(!g_file_test(filename, G_FILE_TEST_EXISTS) ||
	 yes_no_f(dialog, _("Overwrite existing file?"), 
		  _("The file already exists. "
		    "Do you really want to replace it?"))) {

	gconf_set_string("track_path", filename);

	track_write(filename, track_state);
      }
    }
  }
  
  gtk_widget_destroy (dialog);
}

#ifdef USE_MAEMO
#ifdef MAEMO5
#define TRACK_PATH  "/home/user/." APP
#else
#define TRACK_PATH  "/media/mmc2/" APP
#endif
#else
#define TRACK_PATH  "~/." APP
#endif

static char *build_path(void) {
  const char track_path[] = TRACK_PATH;

  if(track_path[0] == '~') {
    int skip = 1;
    char *p = getenv("HOME");
    if(!p) return NULL;

    while(track_path[strlen(track_path)-skip] == '/')
      skip++;

    return g_strdup_printf("%s/%s/track.trk", p, track_path+skip);
  }

  return g_strdup_printf("%s/track.trk", track_path);
}

void track_restore(GtkWidget *map) {
  char *path = build_path();

  if(g_file_test(path, G_FILE_TEST_IS_REGULAR)) {
    track_state_t *track_state = track_read(path, TRUE);

    if(track_state) 
      track_draw(map, track_state);

    /* the track comes fresh from the disk */
    track_state->dirty = FALSE;
    
    if(track_state->timer_handler) {
      gtk_timeout_remove(track_state->timer_handler);
      track_state->timer_handler = 0;
    }

  } else {
    GtkWidget *toplevel = gtk_widget_get_toplevel(map);
    menu_enable(toplevel, "Track/Clear", FALSE);
    menu_enable(toplevel, "Track/Export", FALSE);
    menu_enable(toplevel, "Track/Graph", FALSE);
  }

  g_free(path);

  /* install callback for capturing */
  gps_state_t *gps_state = g_object_get_data(G_OBJECT(map), "gps_state");

  GtkWidget *toplevel = gtk_widget_get_toplevel(map);

  /* we may also have to restore track capture ... */
  if(gconf_get_bool(TRACK_CAPTURE_ENABLED, FALSE)) {
    menu_check_set_active(toplevel, "Track/Capture", TRUE);

    /* request all GPS information required for track capturing */
    gps_register_callback(gps_state, LATLON_CHANGED | ALTITUDE_CHANGED, 
			  gps_callback, map);
  }
}

static void 
track_state_save(track_state_t *track_state) {
  char *path = build_path();

  /* if there's no track state, remove any file already present */
  if(!track_state) {
    remove(path);
    g_free(path);
    return;
  }

  if(!track_state->dirty) {
    g_free(path);
    return;
  }

  /* make sure directory exists */
  char *last_sep = strrchr(path, '/');
  g_assert(last_sep);
  *last_sep = 0;
  g_mkdir_with_parents(path, 0700);
  *last_sep = '/';

  track_write(path, track_state);
  
  g_free(path);
}

void track_save(GtkWidget *map) {
  gboolean cur_state = 
    (gboolean)g_object_get_data(G_OBJECT(map), TRACK_CAPTURE_ENABLED);

  /* save state of capture engine */
  gconf_set_bool(TRACK_CAPTURE_ENABLED, cur_state);

  /* unregister callback if present */
  if(cur_state) {
    gps_state_t *gps_state = g_object_get_data(G_OBJECT(map), "gps_state");
    gps_unregister_callback(gps_state, gps_callback);
  }

  /* free "last" coordinate if present */
  track_point_t *last = g_object_get_data(G_OBJECT(map), TRACK_CAPTURE_LAST);
  if(last) {
    g_free(last);
    g_object_set_data(G_OBJECT(map), TRACK_CAPTURE_LAST, NULL);
  }

  track_state_t *track_state = g_object_get_data(G_OBJECT(map), "track_state");
  track_state_save(track_state);
}

void track_graph(GtkWidget *map) {
  GtkWidget *toplevel = gtk_widget_get_toplevel(map);
  track_state_t *track_state = g_object_get_data(G_OBJECT(map), "track_state");

  GtkWidget *dialog = gtk_dialog_new_with_buttons(_("Track graph"),
	  GTK_WINDOW(toplevel), GTK_DIALOG_MODAL,
          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE, NULL);

#ifdef USE_MAEMO
  gtk_window_set_default_size(GTK_WINDOW(dialog), 640, 480);
#else
  gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 200);
#endif

  GtkWidget *graph = graph_new(track_state);
  gtk_box_pack_start_defaults(GTK_BOX((GTK_DIALOG(dialog))->vbox), graph);

  gtk_widget_show_all(dialog);
  
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

int track_length(track_state_t *track_state) {
  int len = 0;

  if(track_state) {
    track_t *track = track_state->track;
    while(track) {
      track_seg_t *seg = track->track_seg;
      while(seg) {
	track_point_t *point = seg->track_point;
	while(point) {
	  len++;
	  point = point->next;
	}
	seg = seg->next;
      }
      track = track->next;
    }
  }
  return len;
}

int track_contents(track_state_t *track_state) {
  int flags = 0;
  if(track_state) {
    track_t *track = track_state->track;
    while(track) {
      track_seg_t *seg = track->track_seg; 
      while(seg) {
	track_point_t *point = seg->track_point;
	while(point) {
	  if(!isnan(point->speed))    flags |= TRACK_SPEED;
	  if(!isnan(point->altitude)) flags |= TRACK_ALTITUDE;
	  if(!isnan(point->hr))       flags |= TRACK_HR;
	  if(!isnan(point->cad))      flags |= TRACK_CADENCE;
	  
	  point = point->next;
	}
	seg = seg->next;
      }
      track = track->next;
    }
  }
  return flags;
}

void track_get_min_max(track_state_t *track_state, int flag, float *min, float *max) {
  *min =  MAXFLOAT;
  *max = -MAXFLOAT;
  
  if(track_state) {
    track_t *track = track_state->track;
    while(track) {
      track_seg_t *seg = track->track_seg;
      while(seg) {
	track_point_t *point = seg->track_point;
	while(point) {
	  switch(flag) {
	  case TRACK_SPEED:
	    if(!isnan(point->speed)) {
	      if(point->speed < *min) *min = point->speed;
	      if(point->speed > *max) *max = point->speed;
	    }
	    break;
	    
	  case TRACK_ALTITUDE:
	    if(!isnan(point->altitude)) {
	      if(point->altitude < *min) *min = point->altitude;
	      if(point->altitude > *max) *max = point->altitude;
	    }
	    break;
	    
	  case TRACK_HR:
	    if(!isnan(point->hr)) {
	      if(point->hr < *min) *min = point->hr;
	      if(point->hr > *max) *max = point->hr;
	    }
	    break;
	    
	  case TRACK_CADENCE:
	    if(!isnan(point->cad)) {
	      if(point->cad < *min) *min = point->cad;
	      if(point->cad > *max) *max = point->cad;
	    }
	    break;
	    
	  }
	  point = point->next;
	}
	seg = seg->next;
      }
      track = track->next;
    }
  }
}
