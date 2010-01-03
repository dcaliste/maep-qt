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

#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>
#include <strings.h>

#define DATE_FORMAT "%FT%T"

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

#ifdef USE_MAEMO
#define GTK_FM_OK  GTK_RESPONSE_OK
#else
#define GTK_FM_OK  GTK_RESPONSE_ACCEPT
#endif

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

static track_point_t *track_parse_trkpt(xmlDocPtr doc, xmlNode *a_node) {
  track_point_t *point = g_new0(track_point_t, 1);

  /* parse position */
  if(!track_get_prop_pos(a_node, &point->coord)) {
    g_free(point);
    return NULL;
  }

  /* scan for children */
  xmlNode *cur_node = NULL;
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {

#if 0 // not yet supported
      /* elevation (altitude) */
      if(strcasecmp((char*)cur_node->name, "ele") == 0) {
	char *str = (char*)xmlNodeGetContent(cur_node);
	point->altitude = g_ascii_strtod(str, NULL);
 	xmlFree(str);
      }
#endif

      /* time */
      if(strcasecmp((char*)cur_node->name, "time") == 0) {
	struct tm time;
	char *str = (char*)xmlNodeGetContent(cur_node);
	char *ptr = strptime(str, DATE_FORMAT, &time);
	if(ptr) point->time = mktime(&time);
 	xmlFree(str);
      }
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
	    printf("ending track segment leaving bounds\n");
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
      if(strcasecmp((char*)cur_node->name, "trkseg") == 0) {
	track_parse_trkseg(track, doc, cur_node);
      } else
	printf("found unhandled gpx/trk/%s\n", cur_node->name);
      
    }
  }
  return track;
}

static track_t *track_parse_gpx(xmlDocPtr doc, xmlNode *a_node) {
  track_t *track = NULL;
  xmlNode *cur_node = NULL;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "trk") == 0) {
	if(!track) 
	  track = track_parse_trk(doc, cur_node);
	else
	  printf("ignoring additional track\n");
      } else
	printf("found unhandled gpx/%s\n", cur_node->name);      
    }
  }
  return track;
}

/* parse root element and search for "track" */
static track_t *track_parse_root(xmlDocPtr doc, xmlNode *a_node) {
  track_t *track = NULL;
  xmlNode *cur_node = NULL;

  for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      /* parse track file ... */
      if(strcasecmp((char*)cur_node->name, "gpx") == 0) 
      	track = track_parse_gpx(doc, cur_node);
      else 
	printf("found unhandled %s\n", cur_node->name);
    }
  }
  return track;
}

static track_t *track_parse_doc(xmlDocPtr doc) {
  track_t *track;

  /* Get the root element node */
  xmlNode *root_element = xmlDocGetRootElement(doc);

  track = track_parse_root(doc, root_element);  

  /*free the document */
  xmlFreeDoc(doc);

  /*
   * Free the global variables that may
   * have been allocated by the parser.
   */
  xmlCleanupParser();

  return track;
}

static track_t *track_read(char *filename) {
  xmlDoc *doc = NULL;

  LIBXML_TEST_VERSION;
  
  /* parse the file and get the DOM */
  if((doc = xmlReadFile(filename, NULL, 0)) == NULL) {
    xmlErrorPtr	errP = xmlGetLastError();
    printf("While parsing \"%s\":\n\n%s\n", filename, errP->message);
    return NULL;
  }

  track_t *track = track_parse_doc(doc); 

  if(!track || !track->track_seg) {
    printf("track was empty/invalid track\n");
    return NULL;
  }

  track->dirty = TRUE;
  
  return track;
}

void track_draw(GtkWidget *map, track_t *track) {
  /* erase any previous track */
  track_clear(map);

  if(!track) return;

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
    seg = seg->next;
    
    osm_gps_map_add_track(OSM_GPS_MAP(map), points);
  }

  /* save track reference in map */
  g_object_set_data(G_OBJECT(map), "track", track);
}

/* this imports a track and adds it to the set of existing tracks */
void track_import(GtkWidget *map) {
  GtkWidget *toplevel = gtk_widget_get_toplevel(map);
  
  /* open a file selector */
  GtkWidget *dialog;

  track_t *track = NULL;
  
#ifdef USE_HILDON
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

  char *track_path = gconf_get_string("track_path");
  
  if(track_path) {
    if(!g_file_test(track_path, G_FILE_TEST_EXISTS)) {
      char *last_sep = strrchr(track_path, '/');
      if(last_sep) {
	*last_sep = 0;  // seperate path from file 
	
	/* the user just created a new document */
	gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(dialog), 
					    track_path);
	gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), 
					  last_sep+1);
	
	/* restore full filename */
	*last_sep = '/';
      }
    } else 
      gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), 
				    track_path);
  }
  
  gtk_widget_show_all (GTK_WIDGET(dialog));
  if (gtk_dialog_run (GTK_DIALOG(dialog)) == GTK_FM_OK) {
    char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

    /* load a track */
    track = track_read(filename);
    if(track) 
      gconf_set_string("track_path", filename);

    g_free (filename);
  }

  track_draw(map, track);

  gtk_widget_destroy (dialog);
}

/* --------------------------------------------------------------- */

void track_point_free(track_point_t *point) {
  g_free(point);
}

void track_seg_free(track_seg_t *seg) {
  track_point_t *point = seg->track_point;
  while(point) {
    track_point_t *next = point->next;
    track_point_free(point);
    point = next;
  }

  g_free(seg);
}

void track_clear(GtkWidget *map) {
  track_t *track = g_object_get_data(G_OBJECT(map), "track");
  if (!track) return;

  g_object_set_data(G_OBJECT(map), "track", NULL);
  osm_gps_map_clear_tracks(OSM_GPS_MAP(map));

  track_seg_t *seg = track->track_seg;
  while(seg) {
    track_seg_t *next = seg->next;
    track_seg_free(seg);
    seg = next;
  }
  g_free(track);
}

void track_capture_enable(GtkWidget *map, gboolean enable) {
  printf("%sabling track capture\n", enable?"en":"dis");

  g_object_set_data(G_OBJECT(map), "track-enable", (gpointer)enable);

}
