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

/* a point is just that */
typedef struct track_point_s {
  coord_t coord;
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
  track_seg_t *track_seg;
  gboolean dirty;
  track_seg_t *cur_seg;
} track_t;

track_t *track_import(char *filename);

#endif // TRACK_H
