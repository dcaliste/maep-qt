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

#ifndef TRACK_H
#define TRACK_H

#include "converter.h"
#include <glib.h>
#include <glib-object.h>

G_BEGIN_DECLS

/**
 * MAEP_TYPE_GEODATA:
 *
 * return the type of #MaepGeodata.
 *
 * Since: 1.4
 */
#define MAEP_TYPE_GEODATA	     (maep_geodata_get_type ())
/**
 * MAEP_GEODATA:
 * @obj: a #GObject to cast.
 *
 * Cast the given @obj into #MaepGeodata type.
 *
 * Since: 1.4
 */
#define MAEP_GEODATA(obj)	     (G_TYPE_CHECK_INSTANCE_CAST(obj, MAEP_TYPE_GEODATA, MaepGeodata))
/**
 * MAEP_GEODATA_CLASS:
 * @klass: a #GObjectClass to cast.
 *
 * Cast the given @klass into #MaepGeodataClass.
 *
 * Since: 1.4
 */
#define MAEP_GEODATA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST(klass, MAEP_TYPE_GEODATA, MaepGeodataClass))
/**
 * MAEP_IS_GEODATA:
 * @obj: a #GObject to test.
 *
 * Test if the given @ogj is of the type of #MaepGeodata object.
 *
 * Since: 1.4
 */
#define MAEP_IS_GEODATA(obj)    (G_TYPE_CHECK_INSTANCE_TYPE(obj, MAEP_TYPE_GEODATA))
/**
 * MAEP_IS_GEODATA_CLASS:
 * @klass: a #GObjectClass to test.
 *
 * Test if the given @klass is of the type of #MaepGeodataClass class.
 *
 * Since: 1.4
 */
#define MAEP_IS_GEODATA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE(klass, MAEP_TYPE_GEODATA))
/**
 * MAEP_GEODATA_GET_CLASS:
 * @obj: a #GObject to get the class of.
 *
 * It returns the class of the given @obj.
 *
 * Since: 1.4
 */
#define MAEP_GEODATA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS(obj, MAEP_TYPE_GEODATA, MaepGeodataClass))

typedef struct _MaepGeodata        MaepGeodata;
typedef struct _MaepGeodataPrivate MaepGeodataPrivate;
typedef struct _MaepGeodataClass   MaepGeodataClass;

struct _MaepGeodata
{
  GObject parent;

  MaepGeodataPrivate *priv;
};

struct _MaepGeodataClass
{
  GObjectClass parent;
};

/**
 * maep_geodata_get_type:
 *
 * This method returns the type of #MaepGeodata, use
 * MAEP_TYPE_GEODATA instead.
 *
 * Since: 1.4
 *
 * Returns: the type of #MaepGeodata.
 */
GType maep_geodata_get_type(void);

/* parts of a track */
#define TRACK_SPEED    (1<<0)
#define TRACK_ALTITUDE (1<<1)
#define TRACK_HR       (1<<2)
#define TRACK_CADENCE  (1<<3)

/* a point is just that */
typedef struct track_point_s {
  coord_t coord;
  float h_acc;
  float altitude;
  float speed;
  float hr;          // heart rate
  float cad;         // cadence
  time_t time;
  /* struct track_point_s *next; */
} track_point_t;

typedef struct way_point_s {
  track_point_t pt;
  gchar *name, *comment, *description;
} way_point_t;

/* a segment is a series of points */
typedef struct track_seg_s {
  GArray *track_points;
  /* track_point_t *track_point; */
  struct track_seg_s *next;
} track_seg_t;

/* a track is a series of segments */
typedef struct track_s {
  char *name;
  track_seg_t *track_seg;
  struct track_s *next;
} track_t;


#define MAEP_GEODATA_ERROR track_get_quark()
GQuark track_get_quark();

enum {
  MAEP_GEODATA_ERROR_XML,
  MAEP_GEODATA_ERROR_EMPTY
};

typedef enum {
  WAY_POINT_NAME,
  WAY_POINT_COMMENT,
  WAY_POINT_DESCRIPTION
} way_point_field;

MaepGeodata *maep_geodata_new();
MaepGeodata *maep_geodata_new_from_file(const char *filename, GError **error);

gchar* maep_geodata_get_default_autosave_path(void);

gboolean maep_geodata_to_file(MaepGeodata *track_state,
                              const char *name, GError **error);
gboolean maep_geodata_set_autosave_period(MaepGeodata *track_state, guint elaps);
const gchar* maep_geodata_get_autosave_path(const MaepGeodata *track_state);
gboolean maep_geodata_set_autosave_path(MaepGeodata *track_state, const gchar *path);

gboolean maep_geodata_get_bounding_box(const MaepGeodata *track_state,
                                       coord_t *top_left, coord_t *bottom_right);


void maep_geodata_add_trackpoint(MaepGeodata *track_state,
                                  float latitude, float longitude,
                                  float h_acc,
                                  float altitude, float speed,
                                  float hr, float cad);
void maep_geodata_track_finalize_segment(MaepGeodata *track_state);
int maep_geodata_track_get_contents(const MaepGeodata *track_state);
guint maep_geodata_track_get_length(const MaepGeodata *track_state);
gfloat maep_geodata_track_get_metric_length(const MaepGeodata *track_state);
guint maep_geodata_track_get_duration
(const MaepGeodata *track_state);
guint maep_geodata_track_get_start_timestamp(const MaepGeodata *track_state);
gboolean maep_geodata_track_set_metric_accuracy(MaepGeodata *track_state,
                                                gfloat metricAccuracy);
gfloat maep_geodata_track_get_metric_accuracy(const MaepGeodata *track_state);



void maep_geodata_add_waypoint(MaepGeodata *track_state,
                               float latitude, float longitude,
                               const gchar *name, const gchar *comment,
                               const gchar *description);
gboolean maep_geodata_waypoint_set_field(MaepGeodata *track_state,
                                         guint iwpt, way_point_field field,
                                         const gchar *value);
const gchar* maep_geodata_waypoint_get_field(const MaepGeodata *track_state,
                                             guint iwpt, way_point_field field);
gboolean maep_geodata_waypoint_set_highlight(MaepGeodata *track_state, gint iwpt);
gint maep_geodata_waypoint_get_highlight(const MaepGeodata *track_state);

const way_point_t* maep_geodata_waypoint_get(const MaepGeodata *track_state,
                                             guint iwpt);
guint maep_geodata_waypoint_get_length(const MaepGeodata *track_state);

typedef struct {
  MaepGeodata *parent;
  
  track_t *track;
  track_seg_t *seg;
  guint pt;

  track_point_t *cur;
} MaepGeodataTrackIter;

#define TRACK_POINT_START ( 1 << 0)
#define TRACK_POINT_STOP  ( 1 << 1)

void maep_geodata_track_iter_new(MaepGeodataTrackIter *iter,
                                 MaepGeodata *track_state);
gboolean maep_geodata_track_iter_next(MaepGeodataTrackIter *iter,
                                      int *status);

G_END_DECLS

#endif // TRACK_H
