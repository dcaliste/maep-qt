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
#include <math.h>

#include "graph.h"
#include "track.h"

typedef struct {
  track_t *track;
  float zoom;
} graph_priv_t;

int graph_time(GtkWidget *widget, int x, time_t val) {
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

  return height - th - th2;
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
  graph_priv_t *priv = g_object_get_data(G_OBJECT(widget), "priv");
  if(!track_length(priv->track)) {
    printf("no valid track!\n");
  } else {
    printf("track is %d points long\n", track_length(priv->track));   

    time_t tmin = INT32_MAX, tmax = 0;
    assert(tmin > time(NULL));  // make sure out tests will work at all

    // get smallest and biggest time
    track_seg_t *seg = priv->track->track_seg;
    while(seg) {
      track_point_t *point = seg->track_point;
      while(point) {
	if(point->time > tmax) tmax = point->time;
	if(point->time < tmin) tmin = point->time;

	point = point->next;
      }
      seg = seg->next;
    }

    // default: altitude
    float min, max;
    track_get_min_max(priv->track, TRACK_ALTITUDE, &min, &max);
    
    GdkGC *graph_gc = widget->style->fg_gc[GTK_STATE_NORMAL];

    int gheight = graph_time(widget, 0, tmin);
    graph_time(widget, width-1, tmax);

    /* finally draw the graph itself */
    seg = priv->track->track_seg;
    while(seg) {
      track_point_t *point = seg->track_point;
      while(point) {
	if(!isnan(point->altitude)) {
	  int x = width*(point->time-tmin)/(tmax-tmin);
	  int y = gheight*(point->altitude-min)/(max-min);

	  gdk_draw_line(pixmap, graph_gc, x, gheight, x, gheight-y);
	}
	
	point = point->next;
      }
      seg = seg->next;
    }
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
  graph_priv_t *priv = g_object_get_data(G_OBJECT(widget), "priv");
  g_free(priv);

  return FALSE;
}

GtkWidget *graph_new(track_t *track) {
  graph_priv_t *priv = g_new0(graph_priv_t, 1);

  GtkWidget *graph = gtk_drawing_area_new();
  g_object_set_data(G_OBJECT(graph), "priv", priv);
  priv->track = track;
  priv->zoom = 1.0;

  int flags = track_contents(track);
  
  printf("flags = %x\n", flags);

  gtk_signal_connect(GTK_OBJECT(graph), "expose_event",
		     G_CALLBACK(graph_expose_event), NULL);
  gtk_signal_connect(GTK_OBJECT(graph),"configure_event",
		     G_CALLBACK(graph_configure_event), NULL);
  g_signal_connect(G_OBJECT(graph), "destroy", 
		   G_CALLBACK(graph_destroy_event), NULL);

  return graph;
}

void graph_zoom(GtkWidget *graph, int zoom_dir) {
  graph_priv_t *priv = g_object_get_data(G_OBJECT(graph), "priv");

  switch(zoom_dir) {
  case -1: 
    priv->zoom /= 2.0;
    break;
  case 0: 
    priv->zoom = 1.0;
    break;
  case 1: 
    priv->zoom *= 2.0;
    break;
  }
  printf("zoom to %f\n", priv->zoom);
}
