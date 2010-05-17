/*
 * Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
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

#include "osm-gps-map.h"

/* parts of a track */
#define TRACK_SPEED    (1<<0)
#define TRACK_ALTITUDE (1<<1)
#define TRACK_HR       (1<<2)
#define TRACK_CADENCE  (1<<3)

/* a point is just that */
typedef struct track_point_s {
  coord_t coord;
  float altitude;
  float speed;
  float hr;          // heart rate
  float cad;         // cadence
  time_t time;
  struct track_point_s *next;
} track_point_t;

/* a segment is a series of points */
typedef struct track_seg_s {
  track_point_t *track_point;
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
  gboolean dirty;
  guint timer_handler;
} track_state_t;

void track_import(GtkWidget *map);
void track_export(GtkWidget *map);
void track_clear(GtkWidget *map);
void track_capture_enable(GtkWidget *map, gboolean enable); 
void track_graph(GtkWidget *map);
void track_hr_enable(GtkWidget *map, gboolean enable); 

void track_restore(GtkWidget *map);
void track_save(GtkWidget *map);
int track_length(track_state_t *);

int track_contents(track_state_t *);
void track_get_min_max(track_state_t *, int flag, float *min, float *max);

#endif // TRACK_H
