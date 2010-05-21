/*
 * Copyright (C) 2008-2009 Till Harbaum <till@harbaum.org>.
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

#ifndef CONFIG_H
#define CONFIG_H

#include <gtk/gtk.h>

#include <locale.h>
#include <libintl.h>

#ifdef USE_MAEMO
#include <hildon/hildon-program.h>
#include <libosso.h>
#if MAEMO_VERSION_MAJOR >= 5
#define MAEMO5
#endif
#endif

#define LOCALEDIR PREFIX "/share/locale"
#define PACKAGE   "maep"

#define _(String) gettext(String)
#define N_(String) (String)

/* map configuration: */
#define ENABLE_OSD
#define OSD_SCALE
#define OSD_CROSSHAIR
#define OSD_COORDINATES
#undef OSD_NAV

#ifdef USE_MAEMO
#define MAP_DRAG_LIMIT            (32)

#define OSD_FONT_SIZE             (28.0)
#define OSD_DIAMETER              (60)
#define OSD_SCALE_FONT_SIZE       (20.0)
#define OSD_COORDINATES_FONT_SIZE (20.0)

#include <hildon/hildon-defines.h>
/* only maemo devices up to version 4 have a fullscreen button */
#if (MAEMO_VERSION_MAJOR < 5)
#define MAP_KEY_FULLSCREEN  HILDON_HARDKEY_FULLSCREEN
#else
#define MAP_KEY_FULLSCREEN  'f'
#endif
#define MAP_KEY_ZOOMIN      HILDON_HARDKEY_INCREASE
#define MAP_KEY_ZOOMOUT     HILDON_HARDKEY_DECREASE
#else
#define MAP_DRAG_LIMIT      (10)
#define MAP_KEY_FULLSCREEN  GDK_F11
#define MAP_KEY_ZOOMIN      '+'
#define MAP_KEY_ZOOMOUT     '-'
#endif

#define MAP_KEY_UP          GDK_Up
#define MAP_KEY_DOWN        GDK_Down
#define MAP_KEY_LEFT        GDK_Left
#define MAP_KEY_RIGHT       GDK_Right

/* specify OSD colors explicitely. Otherwise gtk default */
/* colors are used. fremantle always uses gtk defaults */
#ifndef MAEMO5
#define OSD_COLOR_BG         1, 1, 1, 1      // white background
#define OSD_COLOR            0.5, 0.5, 1     // light blue border and controls
#define OSD_COLOR_DISABLED   0.8, 0.8, 0.8   // light grey disabled controls
#define OSD_SHADOW_ENABLE
#else
#define OSD_COLOR            1, 1, 1         // white
#define OSD_COLOR_BG         0, 0, 0, 0.5    // transparent dark background
#define OSD_COLOR_DISABLED   0.5, 0.5, 0.5   // grey disabled controls
/* fremantle has controls at botton (for fringer friendlyness) */
#define OSD_Y  -10
#define OSD_HR_Y 60    // HR is always at screens top
#endif

#define OSD_DOUBLE_BUFFER    // render osd/map together offscreen
#define OSD_GPS_BUTTON       // display a GPS button
#define OSD_NO_DPAD          // no direction arrows (map is panned)
#define OSD_SOURCE_SEL       // display source selection tab
#define OSD_BALLOON
#define OSD_DOUBLEPIXEL      // allow pixel doubling from OSD
#define OSD_HEARTRATE

#endif // CONFIG_H
