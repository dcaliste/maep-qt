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
#include "misc.h"

typedef struct {
  GtkWidget *graph;
  GtkWidget *tmin_w, *tmax_w;
  GtkWidget *vmin_w, *vmax_w;
  GtkAdjustment *adj;
  GdkPixmap *pixmap;
  track_state_t *track_state;
  int zoom;
  float tmin, tmax;
  float vmin, vmax;
} graph_priv_t;

static gboolean graph_min_reached(GtkWidget *graph) {
  graph_priv_t *priv = g_object_get_data(G_OBJECT(graph), "priv");
  g_assert(priv);

  return priv->zoom <= 1;
}

/* a pixmap may have 16384 pixels width at max, so make sure */
/* the zoom is limited not to exceed this */
static gboolean graph_max_reached(GtkWidget *graph) {
  graph_priv_t *priv = g_object_get_data(G_OBJECT(graph), "priv");
  g_assert(priv);

  int width, height;
  gdk_drawable_get_size(priv->pixmap, &width, &height);
  return width >= 8192;
}

static void set_buttons(GtkWidget *graph) {
  GtkWidget *but = g_object_get_data(G_OBJECT(graph), "zoom_out_button");
  gtk_widget_set_sensitive(but, !graph_min_reached(graph));
  but = g_object_get_data(G_OBJECT(graph), "zoom_in_button");
  gtk_widget_set_sensitive(but, !graph_max_reached(graph));
}

static void graph_zoom(GtkWidget *graph, int zoom_dir) {
  /* the graph itself is inside the scolled window/viewport pair */
  graph_priv_t *priv = g_object_get_data(G_OBJECT(graph), "priv");
  g_assert(priv);

  switch(zoom_dir) {
  case -1:              // zoom out
    priv->zoom /= 2;
    break;
  case 0:               // reset zoom
    priv->zoom = 1;
    break;
  case 1:               // zoom in
    priv->zoom *= 2;
    break;
  }

  if(priv->zoom < 1) priv->zoom = 1;

  gtk_drawing_area_size(GTK_DRAWING_AREA(priv->graph), 
	priv->zoom * priv->graph->parent->allocation.width, -1);
}

static void on_zoom_out_clicked(GtkWidget *button, gpointer data) {
  GtkWidget *graph = GTK_WIDGET(data);
  graph_zoom(graph, -1);
  set_buttons(graph);
}

static void on_zoom_in_clicked(GtkWidget *button, gpointer data) {
  GtkWidget *graph = GTK_WIDGET(data);
  graph_zoom(graph, +1);
  set_buttons(graph);
}

static void graph_time_set(GtkWidget *vbox, time_t time) {
  char str[32];

  GtkWidget *time_str = g_object_get_data(G_OBJECT(vbox), "time");
  strftime(str, sizeof(str), "%T", localtime(&time));
  gtk_label_set_text(GTK_LABEL(time_str), str);

  GtkWidget *date_str = g_object_get_data(G_OBJECT(vbox), "date");
  strftime(str, sizeof(str), "%F", localtime(&time));
  gtk_label_set_text(GTK_LABEL(date_str), str);
}

static void scroll_changed(GtkAdjustment *adjustment, gpointer user_data) {
  graph_priv_t *priv = (graph_priv_t*)user_data;
  g_assert(priv);

  int val = gtk_adjustment_get_value(adjustment);

  int width, height;
  gdk_drawable_get_size(priv->pixmap, &width, &height);

  time_t tmin = ((float)val / (float)width) * 
    (priv->tmax - priv->tmin) + priv->tmin;
  time_t tmax = ((float)(val + priv->graph->parent->allocation.width) / 
		 (float)width) * (priv->tmax - priv->tmin) + priv->tmin;

  graph_time_set(priv->tmin_w, tmin);
  graph_time_set(priv->tmax_w, tmax);
}
  
static GtkWidget *graph_time_new(time_t time, gboolean left) {
  float align = left?0.0:1.0;

  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
  
  GtkWidget *time_str = gtk_label_new("");
  gtk_misc_set_alignment(GTK_MISC(time_str), align, 0.5f);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), time_str);
  g_object_set_data(G_OBJECT(vbox), "time", time_str);

  GtkWidget *date_str = gtk_label_new("");
  gtk_misc_set_alignment(GTK_MISC(date_str), align, 0.5f);
  gtk_box_pack_start_defaults(GTK_BOX(vbox), date_str);
  g_object_set_data(G_OBJECT(vbox), "date", date_str);

  if(time) graph_time_set(vbox, time);

  return vbox;
}

static void setup_time_bounds(graph_priv_t *priv) {
  priv->tmin = INT32_MAX, priv->tmax = 0;
  assert(priv->tmin > time(NULL));  // make sure out tests will work at all
  
  // get smallest and biggest time
  track_t *track = priv->track_state->track;
  while(track) {
    track_seg_t *seg = track->track_seg;
    while(seg) {
      track_point_t *point = seg->track_point;
      while(point) {
	if(point->time > priv->tmax) priv->tmax = point->time;
	if(point->time < priv->tmin) priv->tmin = point->time;
	
	point = point->next;
      }
      seg = seg->next;
    }
    track = track->next;
  }
}

static void graph_draw(GtkWidget *widget) {
  graph_priv_t *priv = g_object_get_data(G_OBJECT(widget), "priv");
  g_assert(priv);

  int width, height;
  gdk_drawable_get_size(priv->pixmap, &width, &height);
  printf("draw, pixmap size = %d x %d\n", width, height);

  /* erase background */
  gdk_draw_rectangle(priv->pixmap, 
		     priv->graph->style->bg_gc[GTK_STATE_NORMAL], TRUE,
  		     0, 0, width, height);

  /* do some basic track analysis */
  if(track_length(priv->track_state) <= 1) {
    printf("no valid track!\n");
  } else {
    printf("track is %d points long\n", track_length(priv->track_state));   

    setup_time_bounds(priv);
    scroll_changed(priv->adj, priv);

    // default: altitude
    track_get_min_max(priv->track_state, TRACK_ALTITUDE, &priv->vmin, &priv->vmax);

    // fill range widgets
    char *str = g_strdup_printf("%.1f", priv->vmax);
    gtk_label_set_text(GTK_LABEL(priv->vmax_w), str);
    g_free(str);
    str = g_strdup_printf("%.1f", priv->vmin);
    gtk_label_set_text(GTK_LABEL(priv->vmin_w), str);
    g_free(str);
    
    GdkGC *graph_gc = widget->style->fg_gc[GTK_STATE_NORMAL];

    /* finally draw the graph itself */
    track_t *track = priv->track_state->track;
    while(track) {
      track_seg_t *seg = track->track_seg;
      while(seg) {
	track_point_t *point = seg->track_point;
	while(point) {
	  if(!isnan(point->altitude)) {
	    int x = width*(point->time-priv->tmin)/(priv->tmax-priv->tmin);
	    int y = height*(point->altitude-priv->vmin)/(priv->vmax-priv->vmin);
	    
	    gdk_draw_line(priv->pixmap, graph_gc, x, height, x, height-y);
	  }
	  
	  point = point->next;
	}
	seg = seg->next;
      }
      track = track->next;
    }
  }
}

// whenever the scrolled window is resized, the bitmap inside
// will also be resized
static gint swin_expose_event(GtkWidget *widget, GdkEventExpose *event, 
			      GtkWidget *vbox) {
  graph_priv_t *priv = g_object_get_data(G_OBJECT(vbox), "priv");
  g_assert(priv);

  // resize embedded graph if size of scrolled window changed
  if(priv->graph->allocation.width != 
     priv->zoom * priv->graph->parent->allocation.width) 
    gtk_drawing_area_size(GTK_DRAWING_AREA(priv->graph), 
	  priv->zoom * priv->graph->parent->allocation.width, -1);

  return FALSE;
}

/* Create a new backing pixmap of the appropriate size */
static gint graph_configure_event(GtkWidget *widget, GdkEventConfigure *event,
				  gpointer data) {
  graph_priv_t *priv = g_object_get_data(G_OBJECT(data), "priv");
  g_assert(priv);

  if(priv->pixmap) {
    int width, height;
    gdk_drawable_get_size(priv->pixmap, &width, &height);

    if((width != widget->allocation.width) ||
       (height != widget->allocation.height)) {
      gdk_pixmap_unref(priv->pixmap);
      priv->pixmap = NULL;
    }
  }
  
  if(!priv->pixmap) {
    priv->pixmap = gdk_pixmap_new(widget->window,
				  widget->allocation.width,
				  widget->allocation.height,
				  -1);

    graph_draw(GTK_WIDGET(data));
  }

  return TRUE;
}

/* Redraw the screen from the backing pixmap */
static gint graph_expose_event(GtkWidget *widget, GdkEventExpose *event, 
			       gpointer data) {

  graph_priv_t *priv = g_object_get_data(G_OBJECT(data), "priv");
  g_assert(priv);
  
  gdk_draw_pixmap(widget->window,
                  widget->style->fg_gc[GTK_WIDGET_STATE(widget)],
                  priv->pixmap,
                  event->area.x, event->area.y,
                  event->area.x, event->area.y,
                  event->area.width, event->area.height);

  return FALSE;
}

gint graph_destroy_event(GtkWidget *widget, gpointer data ) {
  graph_priv_t *priv = g_object_get_data(G_OBJECT(data), "priv");
  g_assert(priv);

  if(priv->pixmap) gdk_pixmap_unref(priv->pixmap);
  g_free(priv);

  return FALSE;
}

static GtkWidget *graph_bottom_box(GtkWidget *vbox) {
  graph_priv_t *priv = g_object_get_data(G_OBJECT(vbox), "priv");
  g_assert(priv);

  /* zoom button/hrow box below */
  GtkWidget *but, *hbox = gtk_hbox_new(FALSE, 0);
  
  priv->tmin_w = graph_time_new(0, TRUE);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), priv->tmin_w);

  but = gtk_button_new_from_stock(GTK_STOCK_ZOOM_OUT);
  //  but = gtk_button_new_with_label(_("Zoom out"));
  gtk_widget_set_sensitive(but, FALSE);
  g_object_set_data(G_OBJECT(vbox), "zoom_out_button", but);
  gtk_signal_connect(GTK_OBJECT(but), "clicked",
	     GTK_SIGNAL_FUNC(on_zoom_out_clicked), vbox);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), but);
  but = gtk_button_new_from_stock(GTK_STOCK_ZOOM_IN);
  //  but = gtk_button_new_with_label(_("Zoom in"));
  g_object_set_data(G_OBJECT(vbox), "zoom_in_button", but);
  gtk_signal_connect(GTK_OBJECT(but), "clicked",
	     GTK_SIGNAL_FUNC(on_zoom_in_clicked), vbox);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), but);

  priv->tmax_w = graph_time_new(0, FALSE);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), priv->tmax_w);

  return hbox;
}

static GtkWidget *graph_range_new(graph_priv_t *priv) {
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

  gtk_box_pack_start(GTK_BOX(vbox), gtk_hseparator_new(), FALSE, FALSE, 0);

  priv->vmax_w = gtk_label_new("---");
  gtk_box_pack_start(GTK_BOX(vbox), priv->vmax_w, FALSE, FALSE, 0);

  GtkWidget *ivbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start_defaults(GTK_BOX(ivbox), gtk_label_new("Altitude"));
  gtk_box_pack_start_defaults(GTK_BOX(ivbox), gtk_label_new("(m)"));

  gtk_box_pack_start(GTK_BOX(vbox), ivbox, TRUE, FALSE, 0);

  priv->vmin_w = gtk_label_new("---");
  gtk_box_pack_start(GTK_BOX(vbox), priv->vmin_w, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(vbox), gtk_hseparator_new(), FALSE, FALSE, 0);

  return vbox;
}

GtkWidget *graph_new(track_state_t *track_state) {
  graph_priv_t *priv = g_new0(graph_priv_t, 1);
  priv->track_state = track_state;
  priv->zoom = 1;

  /* outmost widget: a vbox */
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);
  g_object_set_data(G_OBJECT(vbox), "priv", priv);

  GtkWidget *win = scrolled_window_new(GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
  priv->graph = gtk_drawing_area_new();

  int flags = track_contents(track_state);
  
  printf("flags = %x\n", flags);

  gtk_signal_connect(GTK_OBJECT(priv->graph), "expose_event",
		     G_CALLBACK(graph_expose_event), vbox);
  gtk_signal_connect(GTK_OBJECT(priv->graph),"configure_event",
		     G_CALLBACK(graph_configure_event), vbox);
  g_signal_connect(G_OBJECT(priv->graph), "destroy", 
		   G_CALLBACK(graph_destroy_event), vbox);

  gtk_widget_add_events(win, GDK_STRUCTURE_MASK);
  gtk_signal_connect(GTK_OBJECT(win), "expose_event",
		     G_CALLBACK(swin_expose_event), vbox);

  scrolled_window_add_with_viewport(win, priv->graph);

  priv->adj = 
    gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(win));
  gtk_signal_connect(GTK_OBJECT(priv->adj), "value_changed",
		     G_CALLBACK(scroll_changed), priv);

  GtkWidget *hbox = gtk_hbox_new(FALSE, 5);

  gtk_box_pack_start(GTK_BOX(hbox), graph_range_new(priv), FALSE, FALSE, 0);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), win);

  gtk_box_pack_start_defaults(GTK_BOX(vbox), hbox);

  GtkWidget *bottom_box = graph_bottom_box(vbox);
  gtk_box_pack_start(GTK_BOX(vbox), bottom_box, FALSE, FALSE, 0);

  return vbox;
}
