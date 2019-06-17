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

/* --------------------------------------------------------------- */

static void track_point_reset(track_point_t *point)
{
  point->time = 0;
  point->altitude = NAN;
  point->speed = NAN;
  point->hr = NAN;
  point->cad = NAN;
  point->coord.rlat = NAN;
  point->coord.rlon = NAN;
  point->h_acc = 0.;
}

static track_seg_t* track_seg_new()
{
  track_seg_t *seg;

  seg = g_new0(track_seg_t, 1);
  seg->track_points = g_array_new(FALSE, FALSE, sizeof(track_point_t));

  return seg;
}
static void track_seg_free(track_seg_t *seg) {
  g_array_unref(seg->track_points);

  g_free(seg);
}

static void way_point_free(way_point_t *wpt) {
  if (wpt->name) g_free(wpt->name);
  if (wpt->comment) g_free(wpt->comment);
  if (wpt->description) g_free(wpt->description);
}

static track_t* track_new() {
  track_t *track;

  track = g_malloc0(sizeof(track_t));

  return track;
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

/* the state of the track handler */
struct _MaepGeodataPrivate {
  track_t *track;

  /* The currently edited segment, or NULL if not any. */
  track_seg_t *current_seg;

  /* Waypoints. */
  GArray *way_points;
  gint iwpt_highlight; /* Negative for no highlight. */

  /* Timer for autosaving. */
  gchar *path;
  gboolean dirty;
  guint timer_handler;

  /* Bounding box of the track. */
  coord_t bb_top_left, bb_bottom_right;

  /* Total length of the track in meters. */
  gfloat metricLength;
  gfloat metricAccuracy;

  gboolean dispose_has_run;
};

enum
  {
    PROP_0,
    N_WPT_PROP,
    I_WPT_HL_PROP,
    N_PROP
  };
static GParamSpec *properties[N_PROP];
enum
  {
    DIRTY_SIG,
    N_SIG
  };
static guint signals[N_SIG];

static void track_finalize(GObject* obj);
static void track_dispose(GObject* obj);
static void track_get_property(GObject* obj, guint property_id,
                               GValue *value, GParamSpec *pspec);
static void track_set_property(GObject* obj, guint property_id,
                               const GValue *value, GParamSpec *pspec);

static void track_state_update_bb(MaepGeodata *track_state);
static void track_state_update_length(MaepGeodata *track_state);

G_DEFINE_TYPE(MaepGeodata, maep_geodata, G_TYPE_OBJECT)

static void maep_geodata_class_init(MaepGeodataClass *klass)
{
  /* Connect freeing methods. */
  G_OBJECT_CLASS(klass)->dispose = track_dispose;
  G_OBJECT_CLASS(klass)->finalize = track_finalize;
  G_OBJECT_CLASS(klass)->set_property = track_set_property;
  G_OBJECT_CLASS(klass)->get_property = track_get_property;

  /**
   * MaepGeodata::n-waypoints:
   *
   * Indicate the number of waypoints.
   *
   * Since: 1.4
   */
  properties[N_WPT_PROP] = g_param_spec_uint("n-waypoints", "Number of waypoints",
                                             "number of stored waypoints",
                                             0, G_MAXUINT, 0, G_PARAM_READABLE);
  g_object_class_install_property(G_OBJECT_CLASS(klass), N_WPT_PROP,
				  properties[N_WPT_PROP]);
  /**
   * MaepGeodata::waypoint-highlight-index:
   *
   * Indicate highlighted waypoint.
   *
   * Since: 1.4
   */
  properties[I_WPT_HL_PROP] = g_param_spec_int("waypoint-highlight-index",
                                                "Index of highlighted waypoint",
                                                "Index of highlighted waypoint",
                                                -1, G_MAXINT, -1, G_PARAM_READWRITE);
  g_object_class_install_property(G_OBJECT_CLASS(klass), I_WPT_HL_PROP,
				  properties[I_WPT_HL_PROP]);

  signals[DIRTY_SIG] = g_signal_new ("dirty", MAEP_TYPE_GEODATA,
                                     G_SIGNAL_RUN_FIRST, 0, NULL, NULL,
                                     g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);

  g_type_class_add_private(klass, sizeof(MaepGeodataPrivate));
}
static void track_dispose(GObject *obj)
{
  if (MAEP_GEODATA(obj)->priv->dispose_has_run)
    return;
  MAEP_GEODATA(obj)->priv->dispose_has_run = TRUE;

  g_message("dispose track.");

  /* Chain up to the parent class */
  G_OBJECT_CLASS(maep_geodata_parent_class)->dispose(obj);
}
static void track_finalize(GObject *obj)
{
  MaepGeodata *track_state;

  track_state = MAEP_GEODATA(obj);

  /* stop running timeout timer if present */
  maep_geodata_set_autosave_period(track_state, 0);
  maep_geodata_set_autosave_path(track_state, NULL);

  track_t *track = track_state->priv->track;
  while(track) {
    track_t *next = track->next;
    track_free(track);
    track = next;
  }

  g_array_free(track_state->priv->way_points, TRUE);

  /* Chain up to the parent class */
  G_OBJECT_CLASS(maep_geodata_parent_class)->finalize(obj);
}
static void track_get_property(GObject* obj, guint property_id,
                               GValue *value, GParamSpec *pspec)
{
  MaepGeodata *self = MAEP_GEODATA(obj);

  switch (property_id)
    {
    case N_WPT_PROP:
      g_value_set_uint(value, self->priv->way_points->len);
      break;
    case I_WPT_HL_PROP:
      g_value_set_int(value, self->priv->iwpt_highlight);
      break;
    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
      break;
    }
}
static void track_set_property(GObject* obj, guint property_id,
                               const GValue *value, GParamSpec *pspec)
{
  MaepGeodata *self = MAEP_GEODATA(obj);

  switch (property_id)
    {
    case I_WPT_HL_PROP:
      maep_geodata_waypoint_set_highlight(self, g_value_get_int(value));
      break;
    default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
      break;
    }
}

static void maep_geodata_init(MaepGeodata *obj)
{
  obj->priv = G_TYPE_INSTANCE_GET_PRIVATE(obj, MAEP_TYPE_GEODATA, MaepGeodataPrivate);
  obj->priv->dispose_has_run = FALSE;

  obj->priv->bb_top_left.rlat = G_MAXFLOAT;
  obj->priv->bb_top_left.rlon = G_MAXFLOAT;

  obj->priv->bb_bottom_right.rlat = -G_MAXFLOAT;
  obj->priv->bb_bottom_right.rlon = -G_MAXFLOAT;

  obj->priv->metricAccuracy = G_MAXFLOAT;

  obj->priv->way_points = g_array_new(FALSE, FALSE, sizeof(way_point_t));
  g_array_set_clear_func(obj->priv->way_points, (GDestroyNotify)way_point_free);
  obj->priv->iwpt_highlight = -1;
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
    error_quark = g_quark_from_static_string("MAEP_GEODATA");
  return error_quark;
}

gchar* maep_geodata_get_default_autosave_path(void)
{
  return build_path();
}

MaepGeodata *maep_geodata_new()
{
  GObject *track_state;

  track_state = g_object_new(MAEP_TYPE_GEODATA, NULL);

  return MAEP_GEODATA(track_state);
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

static void track_parse_tpext(G_GNUC_UNUSED xmlDocPtr doc, xmlNode *a_node, 
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

      /* horizontal accuracy */
      if(strcasecmp((char*)cur_node->name, "h_acc") == 0) {
	char *str = (char*)xmlNodeGetContent(cur_node);
	point->h_acc = g_ascii_strtod(str, NULL);
 	xmlFree(str);
      }
    }
  }
}

static gboolean track_parse_trkpt(track_point_t *point,
                                  xmlDocPtr doc, xmlNode *a_node) {
  track_point_reset(point);

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
	char *str = (char*)xmlNodeGetContent(cur_node);

        if (strptime(str, DATE_FORMAT,  &time))
            point->time = mktime(&time) - timezone;

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
	g_message("found unhandled gpx/trk/trkseg/%s", cur_node->name);
    }
  }
}

static track_t *track_parse_trk(xmlDocPtr doc, xmlNode *a_node) {
  track_t *track = track_new();
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
	g_message("found unhandled gpx/trk/%s", cur_node->name);
      
    }
  }
  return track;
}

static gboolean track_parse_wpt(way_point_t *wpt, xmlDocPtr doc, xmlNode *a_node) {
  if (!track_parse_trkpt(&wpt->pt, doc, a_node))
    return FALSE;

  wpt->pt.h_acc = G_MAXFLOAT;
  wpt->name = NULL;
  wpt->comment = NULL;
  wpt->description = NULL;

  /* scan for children */
  xmlNode *cur_node = NULL;
  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {

      /* name */
      if(strcasecmp((char*)cur_node->name, "name") == 0) {
        char *str = (char*)xmlNodeGetContent(cur_node);
        if (str) wpt->name = g_strdup(str);
        xmlFree(str);
      }

      /* comment */
      if(strcasecmp((char*)cur_node->name, "cmt") == 0) {
        char *str = (char*)xmlNodeGetContent(cur_node);
        if (str) wpt->comment = g_strdup(str);
        xmlFree(str);
      }

      /* description */
      if(strcasecmp((char*)cur_node->name, "desc") == 0) {
        char *str = (char*)xmlNodeGetContent(cur_node);
        if (str) wpt->description = g_strdup(str);
        xmlFree(str);
      }
    }
  }

  return TRUE;
}

static MaepGeodata *track_parse_gpx(xmlDocPtr doc, xmlNode *a_node) {
  MaepGeodata *track_state = maep_geodata_new();
  track_t **track = &(track_state->priv->track);
  xmlNode *cur_node = NULL;
  way_point_t wpt;

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
      } else if(strcasecmp((char*)cur_node->name, "wpt") == 0) {
        if (track_parse_wpt(&wpt, doc, cur_node))
          g_array_append_val(track_state->priv->way_points, wpt);
      } else
	g_message("found unhandled gpx/%s", cur_node->name);      
    }
  }
  return track_state;
}

/* parse root element and search for "track" */
static MaepGeodata *track_parse_root(xmlDocPtr doc, xmlNode *a_node) {
  MaepGeodata *track_state = NULL;
  xmlNode *cur_node = NULL;

  for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      /* parse track file ... */
      if(strcasecmp((char*)cur_node->name, "gpx") == 0) 
      	track_state = track_parse_gpx(doc, cur_node);
      else 
	g_message("found unhandled %s", cur_node->name);
    }
  }
  return track_state;
}

static MaepGeodata *track_parse_doc(xmlDocPtr doc) {
  MaepGeodata *track_state;

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
  MaepGeodata *track_state = MAEP_GEODATA(data);
  GError *error;

  if (!track_state->priv->dirty)
    return TRUE;

  g_message("TRACK: autosave to '%s'.", track_state->priv->path);

  /* make sure directory exists */
  gchar *dirname = g_path_get_dirname(track_state->priv->path);
  g_mkdir_with_parents(dirname, 0700);
  g_free(dirname);

  error = (GError*)0;
  maep_geodata_to_file(track_state, track_state->priv->path, &error);
  if (error)
    {
      g_warning("%s", error->message);
      g_error_free(error);
    }
  
  return TRUE;
}

gboolean maep_geodata_set_autosave_period(MaepGeodata *track_state, guint elaps)
{
  g_return_val_if_fail(MAEP_IS_GEODATA(track_state), FALSE);

  if (track_state->priv->timer_handler) {
    g_source_remove(track_state->priv->timer_handler);
    track_state->priv->timer_handler = 0;
  }

  if (elaps > 0) {
    g_message("Track: adding timeout every %ds.", elaps);
    if (!track_state->priv->path)
      track_state->priv->path = build_path();
#if GLIB_MINOR_VERSION > 13
    track_state->priv->timer_handler = g_timeout_add_seconds(elaps, track_autosave, track_state);
#else
    track_state->priv->timer_handler = g_timeout_add(elaps*1000, track_autosave, track_state);
#endif
  }

  return TRUE;
}
const gchar* maep_geodata_get_autosave_path(const MaepGeodata *track_state)
{
  g_return_val_if_fail(MAEP_IS_GEODATA(track_state), NULL);

  return track_state->priv->path;
}
gboolean maep_geodata_set_autosave_path(MaepGeodata *track_state, const gchar *path)
{
  g_return_val_if_fail(MAEP_IS_GEODATA(track_state), FALSE);

  if (track_state->priv->path)
    g_free(track_state->priv->path);
  track_state->priv->path = NULL;

  if (path && path[0])
    track_state->priv->path = g_strdup(path);

  if (track_state->priv->timer_handler && !track_state->priv->path)
    track_state->priv->path = build_path();

  return TRUE;
}

MaepGeodata *maep_geodata_new_from_file(const char *filename, GError **error) {
  xmlDoc *doc = NULL;

  LIBXML_TEST_VERSION;

  g_return_val_if_fail(error == NULL || *error == NULL, FALSE);
  
  /* parse the file and get the DOM */
  if((doc = xmlReadFile(filename, NULL, 0)) == NULL) {
    xmlErrorPtr	errP = xmlGetLastError();
    g_set_error(error, MAEP_GEODATA_ERROR, MAEP_GEODATA_ERROR_XML,
                "Wrong track file:\n%s", g_strstrip(errP->message));
    return NULL;
  }

  MaepGeodata *track_state = track_parse_doc(doc);

  if(!track_state || !track_state->priv->track) {
    g_set_error(error, MAEP_GEODATA_ERROR, MAEP_GEODATA_ERROR_EMPTY,
                "%s", "Track was empty or invalid");
    if (track_state)
      g_object_unref(G_OBJECT(track_state));
    return NULL;
  }
  track_state_update_bb(track_state);
  track_state_update_length(track_state);

  return track_state;
}

/* ----------------------  saving track --------------------------- */
static void track_save_point(track_point_t *point, xmlNodePtr node) {
  char str[G_ASCII_DTOSTR_BUF_SIZE];
  struct tm t;

  g_ascii_formatd(str, sizeof(str), "%.07f", rad2deg(point->coord.rlat));
  xmlNewProp(node, BAD_CAST "lat", BAD_CAST str);
    
  g_ascii_formatd(str, sizeof(str), "%.07f", rad2deg(point->coord.rlon));
  xmlNewProp(node, BAD_CAST "lon", BAD_CAST str);

  if(!isnan(point->altitude)) {
    g_ascii_formatd(str, sizeof(str), "%.02f", point->altitude);
    xmlNewTextChild(node, NULL, BAD_CAST "ele", BAD_CAST str);
  }

  if(!isnan(point->hr) || !isnan(point->cad) || point->h_acc != G_MAXFLOAT) {
    xmlNodePtr ext = 
      xmlNewChild(node, NULL, BAD_CAST "extensions", NULL);

    if(point->h_acc != G_MAXFLOAT) {
      g_ascii_dtostr(str, sizeof(str), point->h_acc);
      xmlNewTextChild(ext, NULL, BAD_CAST "h_acc", BAD_CAST str);
    }

    if (!isnan(point->hr) || !isnan(point->cad)) {
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
  }

  if(point->time) {
    strftime(str, sizeof(str), DATE_FORMAT, gmtime_r(&point->time, &t));
    xmlNewTextChild(node, NULL, BAD_CAST "time", BAD_CAST str);
  }
}

static void track_save_segs(track_seg_t *seg, xmlNodePtr node) {
  guint i;
  while(seg) {
    xmlNodePtr node_seg = xmlNewChild(node, NULL, BAD_CAST "trkseg", NULL);
    for (i = 0; i < seg->track_points->len; i++)
      {
        xmlNodePtr node_point = xmlNewChild(node_seg, NULL, BAD_CAST "trkpt", NULL);
        track_save_point(&g_array_index(seg->track_points, track_point_t, i), node_point);
      }
    seg = seg->next;
  }
}

static void track_save_tracks(track_t *track, xmlNodePtr node) {
  while(track) {
    xmlNodePtr trk_node = xmlNewChild(node, NULL, BAD_CAST "trk", NULL);
    if(track->name) 
      xmlNewTextChild(trk_node, NULL, BAD_CAST "name", BAD_CAST track->name);

    track_save_segs(track->track_seg, trk_node);
    track = track->next;
  }
}

static void track_save_waypoints(GArray *array, xmlNodePtr node) {
  guint i;
  way_point_t *wpt;

  for (i = 0; i < array->len; i++) {
    xmlNodePtr trk_node = xmlNewChild(node, NULL, BAD_CAST "wpt", NULL);
    wpt = &g_array_index(array, way_point_t, i);
    track_save_point(&wpt->pt, trk_node);
    if(wpt->name && wpt->name[0]) 
      xmlNewTextChild(trk_node, NULL, BAD_CAST "name", BAD_CAST wpt->name);
    if(wpt->comment && wpt->comment[0]) 
      xmlNewTextChild(trk_node, NULL, BAD_CAST "cmt", BAD_CAST wpt->comment);
    if(wpt->description && wpt->description[0]) 
      xmlNewTextChild(trk_node, NULL, BAD_CAST "desc", BAD_CAST wpt->description);
  }
}

gboolean maep_geodata_to_file(MaepGeodata *track_state,
                              const char *name, G_GNUC_UNUSED GError **error) {
  g_return_val_if_fail(MAEP_IS_GEODATA(track_state), FALSE);

  LIBXML_TEST_VERSION;
 
  xmlDocPtr doc = xmlNewDoc(BAD_CAST "1.0");
  xmlNodePtr root_node = xmlNewNode(NULL, BAD_CAST "gpx");
  xmlNewProp(root_node, BAD_CAST "version", BAD_CAST "1.1");
  xmlNewProp(root_node, BAD_CAST "creator", BAD_CAST PACKAGE " v" VERSION);
  xmlNewProp(root_node, BAD_CAST "xmlns", BAD_CAST 
	     "http://www.topografix.com/GPX/1/1");

  /* add TrackPointExtension only if it will actually be used */
  if(maep_geodata_track_get_contents(track_state) & (TRACK_HR | TRACK_CADENCE))
    xmlNewProp(root_node, BAD_CAST "xmlns:gpxtpx", BAD_CAST 
	       "http://www.garmin.com/xmlschemas/TrackPointExtension/v1");

  xmlDocSetRootElement(doc, root_node);

  track_save_tracks(track_state->priv->track, root_node);

  track_save_waypoints(track_state->priv->way_points, root_node);

  g_message("Going to save track to '%s'.", name);
  xmlSaveFormatFileEnc(name, doc, "UTF-8", 1);
  xmlFreeDoc(doc);
  /*
   * Free the global variables that may
   * have been allocated by the parser.
   * This should not be called if parsing is to be used again.
   */
  /* xmlCleanupParser(); */
  track_state->priv->dirty = FALSE;

  return TRUE;
}

int maep_geodata_track_get_contents(const MaepGeodata *track_state) {
  guint i;
  int flags = 0;

  if(track_state) {
    g_return_val_if_fail(MAEP_IS_GEODATA(track_state), flags);
    track_t *track = track_state->priv->track;
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

guint maep_geodata_track_get_length(const MaepGeodata *track_state) {
  guint len = 0;

  if(track_state) {
    g_return_val_if_fail(MAEP_IS_GEODATA(track_state), len);
    track_t *track = track_state->priv->track;
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

gfloat maep_geodata_track_get_metric_length(const MaepGeodata *track_state) {
  g_return_val_if_fail(MAEP_IS_GEODATA(track_state), 0.f);
  
  return track_state->priv->metricLength;
}
gboolean maep_geodata_track_set_metric_accuracy(MaepGeodata *track_state,
                                                gfloat metricAccuracy) {
  g_return_val_if_fail(MAEP_IS_GEODATA(track_state), FALSE);

  if (metricAccuracy == track_state->priv->metricAccuracy)
    return FALSE;

  track_state->priv->metricAccuracy = metricAccuracy;
  track_state_update_length(track_state);

  return TRUE;
}
gfloat maep_geodata_track_get_metric_accuracy(const MaepGeodata *track_state) {
  g_return_val_if_fail(MAEP_IS_GEODATA(track_state), G_MAXFLOAT);

  return track_state->priv->metricAccuracy;
}

guint maep_geodata_track_get_duration(const MaepGeodata *track_state) {
  guint duration, i;
  track_t *track;
  track_seg_t *seg;
  track_point_t *start, *stop, *cur;

  g_return_val_if_fail(MAEP_IS_GEODATA(track_state), 0);

  /* Accumulate time for each segment, on valid points only. */
  duration = 0;
  for(track = track_state->priv->track; track; track = track->next)
    for(seg = track->track_seg; seg; seg = seg->next)
      if (seg->track_points->len > 1)
        {
          start = NULL;
          for (i = 0; i < seg->track_points->len; i++)
            {
              cur = &g_array_index(seg->track_points, track_point_t, i);
              if (cur->h_acc <= track_state->priv->metricAccuracy)
                {
                  start = cur;
                  break;
                }
            }
          stop = NULL;
          for (i = seg->track_points->len - 1; i > 0; i--)
            {
              cur = &g_array_index(seg->track_points, track_point_t, i);
              if (cur->h_acc <= track_state->priv->metricAccuracy)
                {
                  stop = cur;
                  break;
                }
            }
          if (start && stop)
            duration += stop->time - start->time;
        }

  /* g_message("Track: get duration %d %d.", start->time, stop->time); */
  return duration;
}

guint maep_geodata_track_get_start_timestamp(const MaepGeodata *track_state) {
  track_point_t *start;

  g_return_val_if_fail(MAEP_IS_GEODATA(track_state), 0);
  
  if (!track_state->priv->track ||
      !track_state->priv->track->track_seg ||
      track_state->priv->track->track_seg->track_points->len == 0)
    return 0;

  start = &g_array_index(track_state->priv->track->track_seg->track_points,
                         track_point_t, 0);

  return start->time;
}

gboolean maep_geodata_get_bounding_box(const MaepGeodata *track_state,
                                       coord_t *top_left, coord_t *bottom_right) {
  g_return_val_if_fail(MAEP_IS_GEODATA(track_state), FALSE);

  if (track_state->priv->bb_top_left.rlat == G_MAXFLOAT ||
      track_state->priv->bb_top_left.rlon == G_MAXFLOAT ||
      track_state->priv->bb_bottom_right.rlat == -G_MAXFLOAT ||
      track_state->priv->bb_bottom_right.rlon == -G_MAXFLOAT ||
      track_state->priv->bb_top_left.rlat == track_state->priv->bb_bottom_right.rlat ||
      track_state->priv->bb_top_left.rlon == track_state->priv->bb_bottom_right.rlon)
    return FALSE;

  g_message("Track, top left coord is %g x %g",
            track_state->priv->bb_top_left.rlat, track_state->priv->bb_top_left.rlon);
  if (top_left)
    *top_left = track_state->priv->bb_top_left;
  g_message("Track, bottom right coord is %g x %g",
            track_state->priv->bb_bottom_right.rlat, track_state->priv->bb_bottom_right.rlon);
  if (bottom_right)
    *bottom_right = track_state->priv->bb_bottom_right;

  return TRUE;
}


static void track_state_update_bb0(MaepGeodata *track_state,
                                   const track_point_t *point)
{
  if (point->coord.rlat < track_state->priv->bb_top_left.rlat)
    track_state->priv->bb_top_left.rlat = point->coord.rlat;
  if (point->coord.rlon < track_state->priv->bb_top_left.rlon)
    track_state->priv->bb_top_left.rlon = point->coord.rlon;

  if (point->coord.rlat > track_state->priv->bb_bottom_right.rlat)
    track_state->priv->bb_bottom_right.rlat = point->coord.rlat;
  if (point->coord.rlon > track_state->priv->bb_bottom_right.rlon)
    track_state->priv->bb_bottom_right.rlon = point->coord.rlon;
}

static void track_state_update_bb(MaepGeodata *track_state)
{
  guint i;
  if(track_state) {
    g_return_if_fail(MAEP_IS_GEODATA(track_state));
    track_t *track = track_state->priv->track;
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

static gfloat _seg_add_point(track_seg_t *seg, track_point_t *new_point,
                             gfloat metricAccuracy)
{
  track_point_t *prev;
  gint i;

  g_array_append_vals(seg->track_points, new_point, 1);
  /* Calculate distance between previous point and new one. */
  if (seg->track_points->len > 1 && new_point->h_acc <= metricAccuracy)
    {
      /* Get previous valid point for distance. */
      prev = NULL;
      for (i = seg->track_points->len - 2; i >= 0; i--)
        {
          prev = &g_array_index(seg->track_points, track_point_t, i);
          if (prev->h_acc <= metricAccuracy)
            break;
        }
      return (prev)?ABS(get_distance(prev->coord.rlat, prev->coord.rlon,
                                     new_point->coord.rlat, new_point->coord.rlon)):0.f;
    }
  else
    return 0.f;
}

static void track_state_update_length(MaepGeodata *track_state)
{
  guint i;
  track_point_t *prev, *cur;

  if(track_state) {
    g_return_if_fail(MAEP_IS_GEODATA(track_state));
    track_state->priv->metricLength = 0.f;
    track_t *track = track_state->priv->track;
    while(track) {
      track_seg_t *seg = track->track_seg;
      while(seg) {
        /* Use only valid point for distance. */
        prev = NULL;
        cur = NULL;
        for (i = 0; i < seg->track_points->len; i++)
          {
            cur = &g_array_index(seg->track_points, track_point_t, i);
            if (cur->h_acc <= track_state->priv->metricAccuracy)
              {
                track_state->priv->metricLength +=
                  (prev)?ABS(get_distance(prev->coord.rlat, prev->coord.rlon,
                                          cur->coord.rlat, cur->coord.rlon)):0.f;
                prev = cur;
              }
          }
	seg = seg->next;
      }
      track = track->next;
    }
  }
}

static track_t* _get_last_track(MaepGeodata *track_state)
{
  g_return_val_if_fail(MAEP_IS_GEODATA(track_state), NULL);

  /* get last track, create one if none present */
  track_t *track = track_state->priv->track;
  if(!track) {
    g_message("track: no track so far, creating new track");

    track = track_state->priv->track = track_new();

    time_t tval = time(NULL);
    struct tm *loctime = localtime(&tval);
    char str[64];
    strftime(str, sizeof(str), "Track started %x %X", loctime);
    track->name = g_strdup(str);
  }
  else {
    while (track->next) {
      track = track->next;
    }
  }

  return track;
}

static track_seg_t* _get_new_segment(MaepGeodata *track_state)
{
  track_t *track;

  track = _get_last_track(track_state);
  g_return_val_if_fail(track, NULL);

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
void maep_geodata_track_finalize_segment(MaepGeodata *track_state)
{
  g_return_if_fail(MAEP_IS_GEODATA(track_state));

  g_message("track: finalize segment.");
  track_state->priv->current_seg = NULL;
}

void maep_geodata_add_trackpoint(MaepGeodata *track_state,
                                  float latitude, float longitude,
                                  float h_acc,
                                  float altitude, float speed,
                                  float hr, float cad)
{
  g_return_if_fail(MAEP_IS_GEODATA(track_state));

  /* save gps position in track */
  track_point_t new_point;
  new_point.altitude = altitude;
  new_point.time = time(NULL);
  new_point.speed = speed;
  new_point.hr = hr;
  new_point.cad = cad;
  new_point.coord.rlat = deg2rad(latitude);
  new_point.coord.rlon = deg2rad(longitude);
  new_point.h_acc = h_acc;

  /* get current segment */
  track_seg_t *seg = track_state->priv->current_seg;
  if (!seg)
    seg = track_state->priv->current_seg = _get_new_segment(track_state);
  
  track_state->priv->dirty = TRUE;
  track_state->priv->metricLength +=
    _seg_add_point(seg, &new_point, track_state->priv->metricAccuracy);
  g_message("gps: creating new point %g (%g)",
            track_state->priv->metricLength, h_acc);

  /* Updating bounding box. */
  track_state_update_bb0(track_state, &new_point);

  g_signal_emit(track_state, signals[DIRTY_SIG], 0, NULL);
}

void maep_geodata_add_waypoint(MaepGeodata *track_state,
                               float latitude, float longitude,
                               const gchar *name, const gchar *comment,
                               const gchar *description)
{
  way_point_t new_point;

  /* get current track. */
  g_return_if_fail(MAEP_IS_GEODATA(track_state));

  track_point_reset(&new_point.pt);
  new_point.pt.h_acc = G_MAXFLOAT;
  new_point.pt.time = time(NULL);
  new_point.pt.coord.rlat = deg2rad(latitude);
  new_point.pt.coord.rlon = deg2rad(longitude);
  new_point.name = g_strdup(name);
  new_point.comment = g_strdup(comment);
  new_point.description = g_strdup(description);

  g_array_append_val(track_state->priv->way_points, new_point);
  g_object_notify_by_pspec(G_OBJECT(track_state), properties[N_WPT_PROP]);

  track_state->priv->dirty = TRUE;
}
gboolean maep_geodata_waypoint_set_field(MaepGeodata *track_state,
                                         guint iwpt, way_point_field field,
                                         const gchar *value)
{
  way_point_t *wpt;

  g_return_val_if_fail(MAEP_IS_GEODATA(track_state), FALSE);

  if (iwpt >= track_state->priv->way_points->len)
    return FALSE;
  track_state->priv->dirty = TRUE;

  wpt = &g_array_index(track_state->priv->way_points, way_point_t, iwpt);
  switch (field)
    {
    case WAY_POINT_NAME:
      g_free(wpt->name);
      wpt->name = g_strdup(value);
      return TRUE;
    case WAY_POINT_COMMENT:
      g_free(wpt->comment);
      wpt->comment = g_strdup(value);
      return TRUE;
    case WAY_POINT_DESCRIPTION:
      g_free(wpt->description);
      wpt->description = g_strdup(value);
      return TRUE;
    }
  return TRUE;
}
const gchar* maep_geodata_waypoint_get_field(const MaepGeodata *track_state,
                                             guint iwpt, way_point_field field)
{
  way_point_t *wpt;

  g_return_val_if_fail(MAEP_IS_GEODATA(track_state), NULL);

  if (iwpt >= track_state->priv->way_points->len)
    return NULL;

  wpt = &g_array_index(track_state->priv->way_points, way_point_t, iwpt);
  switch (field)
    {
    case WAY_POINT_NAME:
      return wpt->name;
    case WAY_POINT_COMMENT:
      return wpt->comment;
    case WAY_POINT_DESCRIPTION:
      return wpt->description;
    }
  return NULL;
}
const way_point_t* maep_geodata_waypoint_get(const MaepGeodata *track_state,
                                             guint iwpt)
{
  g_return_val_if_fail(MAEP_IS_GEODATA(track_state), NULL);

  return (iwpt < track_state->priv->way_points->len)?&g_array_index(track_state->priv->way_points, way_point_t, iwpt):NULL;
}
guint maep_geodata_waypoint_get_length(const MaepGeodata *track_state)
{
  g_return_val_if_fail(MAEP_IS_GEODATA(track_state), 0);

  return track_state->priv->way_points->len;
}
gboolean maep_geodata_waypoint_set_highlight(MaepGeodata *track_state, gint iwpt)
{
  g_return_val_if_fail(MAEP_IS_GEODATA(track_state), FALSE);
  
  if (track_state->priv->iwpt_highlight == iwpt)
    return FALSE;

  track_state->priv->iwpt_highlight = (iwpt >= (gint)track_state->priv->way_points->len) ? -1 : iwpt;
  g_object_notify_by_pspec(G_OBJECT(track_state), properties[I_WPT_HL_PROP]);
  return TRUE;
}
gint maep_geodata_waypoint_get_highlight(const MaepGeodata *track_state)
{
  g_return_val_if_fail(MAEP_IS_GEODATA(track_state), -1);

  return track_state->priv->iwpt_highlight;
}

/* Iterator on tracks. */
void maep_geodata_track_iter_new(MaepGeodataTrackIter *iter,
                                 MaepGeodata *track_state)
{
  g_return_if_fail(MAEP_IS_GEODATA(track_state) && iter);

  iter->parent = track_state;
  iter->track = track_state->priv->track;
  iter->seg = (iter->track)?iter->track->track_seg:NULL;
  iter->pt = 0;
}
gboolean maep_geodata_track_iter_next(MaepGeodataTrackIter *iter,
                                      int *status)
{
  track_point_t *pt;
  guint i;

  g_return_val_if_fail(iter, FALSE);

  if (!iter->seg)
    return FALSE;

  if (status)
    *status = (iter->pt == 0)?TRACK_POINT_START:0;

  /* We go to next valid point. */
  pt = NULL;
  for (; iter->pt < iter->seg->track_points->len; iter->pt++)
    {
      pt = &g_array_index(iter->seg->track_points, track_point_t, iter->pt);
      if (pt->h_acc <= iter->parent->priv->metricAccuracy)
        {
          iter->cur = pt;
          iter->pt += 1;
          if (status)
            {
              /* We inquire if this is the last valid point of this
               * segment. */
              for ( i = iter->pt; i < iter->seg->track_points->len; i++)
                {
                  pt = &g_array_index(iter->seg->track_points, track_point_t, i);
                  if (pt->h_acc <= iter->parent->priv->metricAccuracy)
                    break;
                }
              if (i == iter->seg->track_points->len)
                *status += TRACK_POINT_STOP;
            }
          return TRUE;
        }
    }

  /* No more valid point on this segment, go to next. */
  if (iter->seg->next)
    {
      iter->seg = iter->seg->next;
      iter->pt = 0;
      return maep_geodata_track_iter_next(iter, status);
    }

  /* No more segment, go to next track. */
  if (iter->track->next)
    {
      iter->track = iter->track->next;
      iter->seg = iter->track->track_seg;
      iter->pt = 0;
      return maep_geodata_track_iter_next(iter, status);
    }
  
  return FALSE;
}

#ifdef TEST_ME
int main(int argc, const char **argv)
{
  MaepGeodata *track_state;
  MaepGeodataTrackIter iter;
  const way_point_t *wpt;
  guint i;
  int st;

  /* Create a track for tests. */
  track_state = maep_geodata_new();
  /* First segment. */
  for (i = 0; i < 10; i++)
    {
      maep_geodata_add_trackpoint(track_state, 46. + (gfloat)i / 1000.f,
                                  6. + sin((gfloat)i) / 1000.,
                                  45.f / (gfloat)(i + 1), 200., NAN, NAN, NAN);
      g_array_index(track_state->priv->current_seg->track_points, track_point_t, track_state->priv->current_seg->track_points->len - 1).time += i;
    }
  /* Second segment. */
  track_state->priv->current_seg = NULL;
  for (i = 0; i < 15; i++)
    {
      maep_geodata_add_trackpoint(track_state, 46. - (gfloat)i / 200.f,
                                  6. + cos((gfloat)i) / 1000.,
                                  45.f / (gfloat)(i + 1), 200., NAN, NAN, NAN);
      g_array_index(track_state->priv->current_seg->track_points, track_point_t, track_state->priv->current_seg->track_points->len - 1).time += 3 * i;
      if (i % 5 == 2)
        maep_geodata_add_waypoint(track_state, 46. - (gfloat)i / 200.f,
                                  6. + cos((gfloat)i) / 1000.,
                                  "Great point", NULL, NULL);
    }

  st = 0;
  maep_geodata_track_iter_new(&iter, track_state);
  while (maep_geodata_track_iter_next(&iter, &st))
    {
      g_print("%g %g %d (%g) at %d\n", rad2deg(iter.cur->coord.rlat),
              rad2deg(iter.cur->coord.rlon), st, iter.cur->h_acc, (int)iter.cur->time);
    };
  g_print("%gm %ds\n", track_state->metricLength, maep_geodata_track_get_duration(track_state));

  maep_geodata_track_set_metric_accuracy(track_state, 14.);
  maep_geodata_track_iter_new(&iter, track_state);
  while (maep_geodata_track_iter_next(&iter, &st))
    {
      g_print("%g %g %d (%g) at %d\n", rad2deg(iter.cur->coord.rlat),
              rad2deg(iter.cur->coord.rlon), st, iter.cur->h_acc, (int)iter.cur->time);
    };
  g_print("%gm %ds\n", track_state->metricLength, maep_geodata_track_get_duration(track_state));

  maep_geodata_track_set_metric_accuracy(track_state, 3.1);
  maep_geodata_track_iter_new(&iter, track_state);
  while (maep_geodata_track_iter_next(&iter, &st))
    {
      g_print("%g %g %d (%g) at %d\n", rad2deg(iter.cur->coord.rlat),
              rad2deg(iter.cur->coord.rlon), st, iter.cur->h_acc, (int)iter.cur->time);
    };
  g_print("%gm %ds\n", track_state->priv->metricLength, maep_geodata_track_get_duration(track_state));
  for (i = 0, wpt = track_waypoint_get(track_state, i); wpt;
       wpt = track_waypoint_get(track_state, ++i))
    g_print("%d '%s' at %g %g\n", i, wpt->name, wpt->pt.coord.rlat, wpt->pt.coord.rlon);

  maep_geodata_to_file(track_state, "test.gpx", NULL);

  g_object_unref(G_OBJECT(track_state));

  track_state = maep_geodata_new_from_file("test.gpx", NULL);

  st = 0;
  maep_geodata_track_iter_new(&iter, track_state);
  while (maep_geodata_track_iter_next(&iter, &st))
    {
      g_print("%g %g %d (%g) at %d\n", rad2deg(iter.cur->coord.rlat),
              rad2deg(iter.cur->coord.rlon), st, iter.cur->h_acc, (int)iter.cur->time);
    };
  g_print("%gm %ds\n", track_state->priv->metricLength, maep_geodata_track_get_duration(track_state));
  for (i = 0, wpt = maep_geodata_waypoint_get(track_state, i); wpt;
       wpt = maep_geodata_waypoint_get(track_state, ++i))
    g_print("%d '%s' at %g %g\n", i, wpt->name, wpt->pt.coord.rlat, wpt->pt.coord.rlon);

  g_object_unref(G_OBJECT(track_state));

  return 0;
}
#endif
