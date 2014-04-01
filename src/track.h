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

G_BEGIN_DECLS

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

/* the state of the track handler */
typedef struct {
  track_t *track;

  /* The currently edited segment, or NULL if not any. */
  track_seg_t *current_seg;

  /* Timer for autosaving. */
  gchar *path;
  gboolean dirty;
  guint timer_handler;

  /* Bounding box of the track. */
  coord_t bb_top_left, bb_bottom_right;

  /* Total length of the track in meters. */
  gfloat metricLength;
  gfloat metricAccuracy;

  /* Ref counted object. */
  guint ref_count;
} track_state_t;

typedef struct {
  track_state_t *parent;
  
  track_t *track;
  track_seg_t *seg;
  guint pt;

  track_point_t *cur;
} track_iter_t;

#define MAEP_TRACK_ERROR track_get_quark()
GQuark track_get_quark();

enum {
  MAEP_TRACK_ERROR_XML,
  MAEP_TRACK_ERROR_EMPTY
};

track_seg_t* track_seg_new();
void track_seg_free(track_seg_t *seg);

track_state_t *track_state_new();

track_state_t* track_state_ref(track_state_t *track_state);
void track_state_unref(track_state_t *track_state);

track_state_t *track_read(const char *filename, GError **error);
gboolean track_write(track_state_t *track_state, const char *name, GError **error);
gboolean track_set_autosave_period(track_state_t *track_state, guint elaps);
const gchar* track_get_autosave_path(track_state_t *track_state);
gboolean track_set_autosave_path(track_state_t *track_state, const gchar *path);

gboolean track_set_metric_accuracy(track_state_t *track_state, gfloat metricAccuracy);
gfloat track_get_metric_accuracy(track_state_t *track_state);

void track_point_new(track_state_t *track_state,
                     float latitude, float longitude,
                     float h_acc,
                     float altitude, float speed,
                     float hr, float cad);

int track_contents(track_state_t *track_state);
int track_length(track_state_t *track_state);
gboolean track_bounding_box(track_state_t *track_state,
                            coord_t *top_left, coord_t *bottom_right);
gfloat track_metric_length(track_state_t *track_state);
guint track_duration(track_state_t *track_state);
guint track_start_timestamp(track_state_t *track_state);

G_END_DECLS

#endif // TRACK_H
