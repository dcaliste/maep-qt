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

#include <assert.h>
#include <stdint.h>

#include "graph.h"
#include "track.h"

void graph_time(GtkWidget *widget, int x, time_t val) {
  GdkPixmap *pixmap = g_object_get_data(G_OBJECT(widget), "pixmap");
  gint width = widget->allocation.width;
  gint height = widget->allocation.height;

  /* draw time below graph */
  PangoLayout *layout;
  char str[32];
  int tw, th, th2, c;

  strftime(str, sizeof(str), "%F", localtime(&val));
  layout = gtk_widget_create_pango_layout(widget, str);
  pango_layout_get_pixel_size(layout, &tw, &th);  
  c = x - tw/2;
  if(c < 0)          c = 0;
  if(c + tw > width) c = width-tw;
  gdk_draw_layout(pixmap, widget->style->fg_gc[GTK_STATE_NORMAL], 
		  c, height-th, layout);
  g_object_unref(layout);

  strftime(str, sizeof(str), "%T", localtime(&val));
  layout = gtk_widget_create_pango_layout(widget, str);
  pango_layout_get_pixel_size(layout, &tw, &th2);  
  c = x - tw/2;
  if(c < 0)          c = 0;
  if(c + tw > width) c = width-tw;
  
  gdk_draw_layout(pixmap, widget->style->fg_gc[GTK_STATE_NORMAL], 
		  c, height-th-th2, layout);
  g_object_unref(layout);
  
}

static void graph_draw(GtkWidget *widget) {
  GdkPixmap *pixmap = g_object_get_data(G_OBJECT(widget), "pixmap");

  gint width = widget->allocation.width;
  gint height = widget->allocation.height;
  /* erase background */
  gdk_draw_rectangle(pixmap, widget->style->bg_gc[GTK_STATE_NORMAL], TRUE,
		     0, 0, width, height);

  printf("graph widget size: %d x %d\n", width, height);

  /* do some basic track analysis */
  track_t *track = g_object_get_data(G_OBJECT(widget), "track");
  if(!track_length(track)) {
    printf("no valid track!\n");
  } else {
    printf("track is %d points long\n", track_length(track));   

    time_t tmin = INT32_MAX, tmax = 0;
    assert(tmin > time(NULL));  // make sure out tests will work at all

    // get smallest and biggest time
    track_seg_t *seg = track->track_seg;
    while(seg) {
      track_point_t *point = seg->track_point;
      while(point) {
	if(point->time > tmax) tmax = point->time;
	if(point->time < tmin) tmin = point->time;

	point = point->next;
      }
      seg = seg->next;
    }

#if 0
    GdkGC *circle_gc = widget->style->white_gc;
    if(widget->style->bg[GTK_STATE_NORMAL].red + 
       widget->style->bg[GTK_STATE_NORMAL].green +
       widget->style->bg[GTK_STATE_NORMAL].blue > 3*60000) {
      circle_gc = gdk_gc_new(pixmap);
      gdk_gc_copy(circle_gc, widget->style->black_gc);
      GdkColor lgrey_color;
      gdk_color_parse("#DDDDDD", &lgrey_color);
      gdk_gc_set_rgb_fg_color(circle_gc,  &lgrey_color);
    }
#endif

    graph_time(widget, 0,       tmin);
    graph_time(widget, width-1, tmax);

    /* finally draw the graph itself */


  }
}

/* Create a new backing pixmap of the appropriate size */
static gint graph_configure_event(GtkWidget *widget, GdkEventConfigure *event,
				  gpointer data) {
  GdkPixmap *pixmap = g_object_get_data(G_OBJECT(widget), "pixmap");
  if(pixmap) gdk_pixmap_unref(pixmap);
  
  g_object_set_data(G_OBJECT(widget), "pixmap", 
		    gdk_pixmap_new(widget->window,
				   widget->allocation.width,
				   widget->allocation.height,
				   -1));

  graph_draw(widget);

  return TRUE;
}

/* Redraw the screen from the backing pixmap */
static gint graph_expose_event(GtkWidget *widget, GdkEventExpose *event, 
			       gpointer data) {

  GdkPixmap *pixmap = g_object_get_data(G_OBJECT(widget), "pixmap");
  
  gdk_draw_pixmap(widget->window,
                  widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
                  pixmap,
                  event->area.x, event->area.y,
                  event->area.x, event->area.y,
                  event->area.width, event->area.height);

  return FALSE;
}

gint graph_destroy_event(GtkWidget *widget, gpointer data ) {

  return FALSE;
}

GtkWidget *graph_new(track_t *track) {
  GtkWidget *graph = gtk_drawing_area_new();
  g_object_set_data(G_OBJECT(graph), "track", track);

  gtk_signal_connect(GTK_OBJECT(graph), "expose_event",
		     G_CALLBACK(graph_expose_event), NULL);
  gtk_signal_connect(GTK_OBJECT(graph),"configure_event",
		     G_CALLBACK(graph_configure_event), NULL);
  g_signal_connect(G_OBJECT(graph), "destroy", 
		   G_CALLBACK(graph_destroy_event), NULL);

  return graph;
}
