/*
 * Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
 *
 * Contributions by
 * Damien Caliste 2013-2014 <dcaliste@free.fr>
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

#define _XOPEN_SOURCE /* glibc2 needs this */
#define __USE_XOPEN
#include <time.h>

#include "config.h"
#include "track.h"
#include "converter.h"
#include "misc.h"
/* #include "hxm.h" */

#include <stdio.h>
#include <math.h>
#include <libxml/parser.h>
#include <libxml/tree.h>
#include <string.h>
#include <strings.h>

#ifndef NAN
#define NAN (0.0/0.0)
#endif

#define DATE_FORMAT "%FT%T"
#define TRACK_CAPTURE_ENABLED "track_capture_enabled"
#define TRACK_CAPTURE_LAST    "track_capture_last"

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

static void track_state_update_bb(track_state_t *track_state);
static void track_state_update_length(track_state_t *track_state);

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
#ifndef SAILFISH
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
#else
  return g_strdup_printf("%s/%s/track.gpx", g_get_user_data_dir(), APP);
#endif
}

static GQuark error_quark = 0;
GQuark track_get_quark()
{
  if (!error_quark)
    error_quark = g_quark_from_static_string("MAEP_TRACK");
  return error_quark;
}

/* --------------------------------------------------------------- */

track_seg_t* track_seg_new()
{
  track_seg_t *seg;

  seg = g_new0(track_seg_t, 1);
  seg->track_points = g_array_new(FALSE, FALSE, sizeof(track_point_t));

  return seg;
}
void track_seg_free(track_seg_t *seg) {
  g_array_unref(seg->track_points);

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
  track_set_autosave_period(track_state, 0);
  track_set_autosave_path(track_state, NULL);

  track_t *track = track_state->track;
  while(track) {
    track_t *next = track->next;
    track_free(track);
    track = next;
  }

  g_free(track_state);
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

static gboolean track_parse_trkpt(track_point_t *point,
                                  xmlDocPtr doc, xmlNode *a_node) {
  point->altitude = NAN;
  point->speed = NAN;
  point->hr = NAN;
  point->cad = NAN;
  point->coord.rlat = NAN;
  point->coord.rlon = NAN;

  /* parse position */
  if(!track_get_prop_pos(a_node, &point->coord))
    return FALSE;

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

  return TRUE;
}

static void 
track_parse_trkseg(track_t *track, xmlDocPtr doc, xmlNode *a_node) {
  xmlNode *cur_node = NULL;
  track_seg_t **seg = &(track->track_seg);

  /* search end of track_seg list */
  while(*seg) seg = &((*seg)->next);

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "trkpt") == 0) {
        track_point_t cpnt;
	if (track_parse_trkpt(&cpnt, doc, cur_node)) {
	  if(! *seg)
	    /* start a new segment */
	    *seg = track_seg_new();
	  /* attach point to chain */
          g_array_append_vals((*seg)->track_points, &cpnt, 1);
	} else {
	  /* end segment if point could not be parsed and start a new one */
	  /* close segment if there is one */
	  if(*seg) {
	    seg = &((*seg)->next);
	  }
	}
      } else
	g_message("found unhandled gpx/trk/trkseg/%s\n", cur_node->name);
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
	g_message("found unhandled gpx/trk/%s\n", cur_node->name);
      
    }
  }
  return track;
}

static track_state_t *track_parse_gpx(xmlDocPtr doc, xmlNode *a_node) {
  track_state_t *track_state = track_state_new();
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
	g_message("found unhandled gpx/%s\n", cur_node->name);      
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
	g_message("found unhandled %s\n", cur_node->name);
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
   * This should not be called if parsing is to be used again.
   */
  /* xmlCleanupParser(); */

  return track_state;
}

static gboolean track_autosave(gpointer data) {
  track_state_t *track_state = (track_state_t*)data;
  GError *error;

  g_message("TRACK: autosave to '%s'.\n", track_state->path);

  /* make sure directory exists */
  gchar *dirname = g_path_get_dirname(track_state->path);
  g_mkdir_with_parents(dirname, 0700);
  g_free(dirname);

  error = (GError*)0;
  track_write(track_state, track_state->path, &error);
  if (error)
    {
      g_warning("%s", error->message);
      g_error_free(error);
    }
  
  return TRUE;
}

gboolean track_set_autosave_period(track_state_t *track_state, guint elaps)
{
  g_return_val_if_fail(track_state, FALSE);

  if (track_state->timer_handler) {
    g_source_remove(track_state->timer_handler);
    track_state->timer_handler = 0;
  }

  if (elaps > 0) {
    g_message("Track: adding timeout every %ds.", elaps);
    if (!track_state->path)
      track_state->path = build_path();
#if GLIB_MINOR_VERSION > 13
    track_state->timer_handler = g_timeout_add_seconds(elaps, track_autosave, track_state);
#else
    track_state->timer_handler = g_timeout_add(elaps*1000, track_autosave, track_state);
#endif
  }

  return TRUE;
}
const gchar* track_get_autosave_path(track_state_t *track_state)
{
  g_return_val_if_fail(track_state, NULL);

  return track_state->path;
}
gboolean track_set_autosave_path(track_state_t *track_state, const gchar *path)
{
  g_return_val_if_fail(track_state, FALSE);

  if (track_state->path)
    g_free(track_state->path);
  track_state->path = NULL;

  if (path && path[0])
    track_state->path = g_strdup(path);

  if (track_state->timer_handler && !track_state->path)
    track_state->path = build_path();

  return TRUE;
}

track_state_t *track_read(const char *filename, GError **error) {
  xmlDoc *doc = NULL;

  LIBXML_TEST_VERSION;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
  
  /* parse the file and get the DOM */
  if((doc = xmlReadFile(filename, NULL, 0)) == NULL) {
    xmlErrorPtr	errP = xmlGetLastError();
    g_set_error(error, MAEP_TRACK_ERROR, MAEP_TRACK_ERROR_XML,
                "Wrong track file:\n%s", g_strstrip(errP->message));
    return NULL;
  }

  track_state_t *track_state = track_parse_doc(doc);

  if(!track_state || !track_state->track) {
    g_set_error(error, MAEP_TRACK_ERROR, MAEP_TRACK_ERROR_EMPTY,
                "%s", "Track was empty or invalid");
    return NULL;
  }
  track_state_update_bb(track_state);
  track_state_update_length(track_state);

  return track_state;
}

/* ----------------------  saving track --------------------------- */

void track_save_point(track_point_t *point, xmlNodePtr node) {
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
}

void track_save_segs(track_seg_t *seg, xmlNodePtr node) {
  guint i;
  while(seg) {
    xmlNodePtr node_seg = xmlNewChild(node, NULL, BAD_CAST "trkseg", NULL);
    for (i = 0; i < seg->track_points->len; i++)
      track_save_point(&g_array_index(seg->track_points, track_point_t, i), node_seg);
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

gboolean track_write(track_state_t *track_state, const char *name, GError **error) {
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

  g_message("Going to save track to '%s'.", name);
  xmlSaveFormatFileEnc(name, doc, "UTF-8", 1);
  xmlFreeDoc(doc);
  /*
   * Free the global variables that may
   * have been allocated by the parser.
   * This should not be called if parsing is to be used again.
   */
  /* xmlCleanupParser(); */

  return TRUE;
}

track_state_t* track_state_ref(track_state_t *track_state)
{
  track_state->ref_count += 1;
  return track_state;
}
void track_state_unref(track_state_t *track_state)
{
  track_state->ref_count -= 1;
  if (!track_state->ref_count)
    track_state_free(track_state);
}

int track_contents(track_state_t *track_state) {
  guint i;
  int flags = 0;
  if(track_state) {
    track_t *track = track_state->track;
    while(track) {
      track_seg_t *seg = track->track_seg; 
      while(seg) {
        for (i = 0; i < seg->track_points->len; i++) {
          track_point_t *point = &g_array_index(seg->track_points, track_point_t, i);
	  if(!isnan(point->speed))    flags |= TRACK_SPEED;
	  if(!isnan(point->altitude)) flags |= TRACK_ALTITUDE;
	  if(!isnan(point->hr))       flags |= TRACK_HR;
	  if(!isnan(point->cad))      flags |= TRACK_CADENCE;
	}
	seg = seg->next;
      }
      track = track->next;
    }
  }
  return flags;
}

int track_length(track_state_t *track_state) {
  int len = 0;

  if(track_state) {
    track_t *track = track_state->track;
    while(track) {
      track_seg_t *seg = track->track_seg;
      while(seg) {
        len += seg->track_points->len;
	seg = seg->next;
      }
      track = track->next;
    }
  }
  return len;
}

gfloat track_metric_length(track_state_t *track_state) {
  g_return_val_if_fail(track_state, 0.f);
  
  return track_state->metricLength;
}

guint track_duration(track_state_t *track_state) {
  track_t *track;
  track_seg_t *seg;
  track_point_t *start, *stop;

  g_return_val_if_fail(track_state, 0);

  if (!track_state->track ||
      !track_state->track->track_seg ||
      track_state->track->track_seg->track_points->len == 0)
    return 0;

  start = &g_array_index(track_state->track->track_seg->track_points, track_point_t, 0);
  for(track = track_state->track; track && track->next; track = track->next);
  for(seg = track->track_seg; seg && seg->next && seg->next->track_points->len > 0; seg = seg->next);
  stop  = &g_array_index(seg->track_points, track_point_t, seg->track_points->len - 1);

  g_message("Track: get duration %d %d.", start, stop);
  return stop - start;
}

gboolean track_bounding_box(track_state_t *track_state,
                            coord_t *top_left, coord_t *bottom_right)
{
  g_return_val_if_fail(track_state, FALSE);

  if (track_state->bb_top_left.rlat == G_MAXFLOAT ||
      track_state->bb_top_left.rlon == G_MAXFLOAT ||
      track_state->bb_bottom_right.rlat == -G_MAXFLOAT ||
      track_state->bb_bottom_right.rlon == -G_MAXFLOAT)
    return FALSE;

  g_message("Track, top left coord is %g x %g",
            track_state->bb_top_left.rlat, track_state->bb_top_left.rlon);
  if (top_left)
    *top_left = track_state->bb_top_left;
  g_message("Track, bottom right coord is %g x %g",
            track_state->bb_bottom_right.rlat, track_state->bb_bottom_right.rlon);
  if (bottom_right)
    *bottom_right = track_state->bb_bottom_right;

  return TRUE;
}


static void track_state_update_bb0(track_state_t *track_state, const track_point_t *point)
{
  if (point->coord.rlat < track_state->bb_top_left.rlat)
    track_state->bb_top_left.rlat = point->coord.rlat;
  if (point->coord.rlon < track_state->bb_top_left.rlon)
    track_state->bb_top_left.rlon = point->coord.rlon;

  if (point->coord.rlat > track_state->bb_bottom_right.rlat)
    track_state->bb_bottom_right.rlat = point->coord.rlat;
  if (point->coord.rlon > track_state->bb_bottom_right.rlon)
    track_state->bb_bottom_right.rlon = point->coord.rlon;
}

static void track_state_update_bb(track_state_t *track_state)
{
  guint i;
  if(track_state) {
    track_t *track = track_state->track;
    while(track) {
      track_seg_t *seg = track->track_seg;
      while(seg) {
        for (i = 0; i < seg->track_points->len; i++)
          track_state_update_bb0(track_state,
                                 &g_array_index(seg->track_points, track_point_t, i));
	seg = seg->next;
      }
      track = track->next;
    }
  }
}

/* http://www.movable-type.co.uk/scripts/latlong.html */
static float get_distance(float lat1, float lon1, float lat2, float lon2) {
  float aob = acos(CLAMP(cos(lat1) * cos(lat2) * cos(lon2 - lon1) +
                         sin(lat1) * sin(lat2), -1.f, +1.f));

  return(aob * 6371000.0);     /* great circle radius in meters */
}

static gfloat _seg_add_point(track_seg_t *seg, track_point_t *new_point)
{
  track_point_t *prev;

  g_array_append_vals(seg->track_points, new_point, 1);
  /* Calculate distance between previous point and new one. */
  if (seg->track_points->len > 1)
    {
      prev = &g_array_index(seg->track_points, track_point_t, seg->track_points->len - 2);
      return ABS(get_distance(prev->coord.rlat, prev->coord.rlon,
                              new_point->coord.rlat, new_point->coord.rlon));
    }
  else
    return 0.f;
}

static void track_state_update_length(track_state_t *track_state)
{
  guint i;
  track_point_t *prev, *cur;

  if(track_state) {
    track_state->metricLength = 0.f;
    track_t *track = track_state->track;
    while(track) {
      track_seg_t *seg = track->track_seg;
      while(seg) {
        if (seg->track_points->len > 0)
          prev = &g_array_index(seg->track_points, track_point_t, 0);
        for (i = 1; i < seg->track_points->len; i++)
          {
            cur = &g_array_index(seg->track_points, track_point_t, i);
            track_state->metricLength +=
              ABS(get_distance(prev->coord.rlat, prev->coord.rlon,
                               cur->coord.rlat, cur->coord.rlon));
            prev = cur;
          }
	seg = seg->next;
      }
      track = track->next;
    }
  }
}

static track_seg_t* _get_new_segment(track_state_t *track_state)
{
  g_return_val_if_fail(track_state, NULL);

  /* get last track, create one if none present */
  track_t *track = track_state->track;
  if(!track) {
    g_message("track: no track so far, creating new track");

    track = track_state->track = g_new0(track_t, 1);

    time_t tval = time(NULL);
    struct tm *loctime = localtime(&tval);
    char str[64];
    strftime(str, sizeof(str), "Track started %x %X", loctime);
    track->name = g_strdup(str);
  }

  g_message("track: no active segment, starting new one");
    
  track_seg_t *seg;
  /* search last segment */
  if((seg = track->track_seg)) {
    /* append to existing chain */
    while(seg->next) seg = seg->next;
    seg = (seg->next = track_seg_new());
  } else
    /* create new chain */
    seg = track->track_seg = track_seg_new();

  return seg;
}

track_state_t *track_state_new()
{
  track_state_t *track_state;

  track_state = g_new0(track_state_t, 1);
  track_state->ref_count = 1;

  track_state->bb_top_left.rlat = G_MAXFLOAT;
  track_state->bb_top_left.rlon = G_MAXFLOAT;

  track_state->bb_bottom_right.rlat = -G_MAXFLOAT;
  track_state->bb_bottom_right.rlon = -G_MAXFLOAT;

  return track_state;
 }

void track_point_new(track_state_t *track_state,
                     float latitude, float longitude,
                     float altitude, float speed,
                     float hr, float cad)
{
  g_return_if_fail(track_state);

  /* save gps position in track */
  track_point_t new_point;
  new_point.altitude = altitude;
  new_point.time = time(NULL);
  new_point.speed = speed;
  new_point.hr = hr;
  new_point.cad = cad;
  new_point.coord.rlat = deg2rad(latitude);
  new_point.coord.rlon = deg2rad(longitude);

  /* get current segment */
  track_seg_t *seg = track_state->current_seg;
  if (!seg)
    seg = track_state->current_seg = _get_new_segment(track_state);
  
  g_message("gps: creating new point %g", track_state->metricLength);
  track_state->metricLength += _seg_add_point(seg, &new_point);

  /* Updating bounding box. */
  track_state_update_bb0(track_state, &new_point);
}
