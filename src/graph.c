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

#include "graph.h"

#define WIDTH 150
#define HEIGHT 150

static void graph_draw(GtkWidget *widget) {
  GdkPixmap *pixmap = g_object_get_data(G_OBJECT(widget), "pixmap");

  gint width = widget->allocation.width;
  gint height = widget->allocation.height;
  gint diameter = (height < width)?height:width;

  gint xcenter = width/2;
  gint ycenter = height/2;

  /* erase background */
  gdk_draw_rectangle(pixmap, widget->style->bg_gc[GTK_STATE_NORMAL], TRUE,
		     0, 0, width, height);

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

  gdk_draw_arc(pixmap, circle_gc, TRUE,
	       xcenter - (diameter/2), 
	       ycenter - (diameter/2), 
	       diameter, diameter,
	       0, 360*64);
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

GtkWidget *graph_new(void) {
  GtkWidget *graph = gtk_drawing_area_new();
  gtk_drawing_area_size(GTK_DRAWING_AREA(graph), WIDTH, HEIGHT);

  gtk_signal_connect(GTK_OBJECT(graph), "expose_event",
		     G_CALLBACK(graph_expose_event), NULL);
  gtk_signal_connect(GTK_OBJECT(graph),"configure_event",
		     G_CALLBACK(graph_configure_event), NULL);
  g_signal_connect(G_OBJECT(graph), "destroy", 
		   G_CALLBACK(graph_destroy_event), NULL);

  return graph;
}
