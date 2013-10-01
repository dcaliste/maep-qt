/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/* vim:set et sw=4 ts=4 cino=t0,(0: */
/*
 * osm-gps-map.c
 * Copyright (C) Marcus Bauer 2008 <marcus.bauer@gmail.com>
 * Copyright (C) John Stowers 2009 <john.stowers@gmail.com>
 * Copyright (C) Till Harbaum 2009 <till@harbaum.org>
 * Copyright (C) Damien Caliste 2013 <dcaliste@free.fr>
 *
 * Contributions by
 * Everaldo Canuto 2009 <everaldo.canuto@gmail.com>
 *
 * osm-gps-map-gtkwidget.c is free software: you can redistribute it and/or modify it
 * under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 */

#include "config.h"

/* #include <fcntl.h> */
/* #include <math.h> */
/* #include <unistd.h> */
/* #include <stdio.h> */
/* #include <stdlib.h> */
/* #include <string.h> */

#include <gtk/gtk.h>
/* #include <glib.h> */
/* #include <glib/gstdio.h> */
/* #include <glib/gprintf.h> */
/* #include <libsoup/soup.h> */
/* #include <cairo.h> */

/* #include "converter.h" */
/* #include "osm-gps-map-types.h" */
#include "osm-gps-map-gtk.h"

#define OSM_GPS_MAP_SCROLL_STEP     (10)

struct _OsmGpsMapGtkPrivate
{
  OsmGpsMap *map;

    //For storing keybindings
    guint keybindings[OSM_GPS_MAP_GTK_KEY_MAX];

    //For tracking click and drag
    int drag_counter;
    int drag_start_mouse_x;
    int drag_start_mouse_y;
    int drag_start_map_x;
    int drag_start_map_y;
    int drag_mouse_dx;
    int drag_mouse_dy;
    int drag_limit;
    gulong drag_expose;

    guint fullscreen : 1;
    guint keybindings_enabled : 1;
    guint is_disposed : 1;
    guint dragging : 1;
    guint button_down : 1;
};

#define OSM_GPS_MAP_GTK_PRIVATE(o)  (OSM_GPS_MAP_GTK (o)->priv)

enum
{
    PROP_0,

    PROP_DRAG_LIMIT,
};

G_DEFINE_TYPE (OsmGpsMapGtk, osm_gps_map_gtk, GTK_TYPE_DRAWING_AREA);

static gboolean 
on_window_key_press(GtkWidget *widget, GdkEventKey *event, gpointer user_data) 
{
    int i;
    int step;
    gboolean handled;
    OsmGpsMapGtk *map = OSM_GPS_MAP_GTK(widget);
    OsmGpsMapGtkPrivate *priv = map->priv;

    //if no keybindings are set, let the app handle them...
    if (!priv->keybindings_enabled)
        return FALSE;

    handled = FALSE;
    step = widget->allocation.width/OSM_GPS_MAP_SCROLL_STEP;

    //the map handles some keys on its own
    for (i = 0; i < OSM_GPS_MAP_GTK_KEY_MAX; i++) {
        //not the key we have a binding for
        if (priv->keybindings[i] != event->keyval)
            continue;

        switch(i) {
            case OSM_GPS_MAP_GTK_KEY_FULLSCREEN: {
                GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(widget));
                if(!priv->fullscreen)
                    gtk_window_fullscreen(GTK_WINDOW(toplevel));
                else
                    gtk_window_unfullscreen(GTK_WINDOW(toplevel));

                priv->fullscreen = !priv->fullscreen;
                handled = TRUE;
                } break;
            case OSM_GPS_MAP_GTK_KEY_ZOOMIN:
                osm_gps_map_zoom_in(priv->map);
                handled = TRUE;
                break;
            case OSM_GPS_MAP_GTK_KEY_ZOOMOUT:
                osm_gps_map_zoom_out(priv->map);
                handled = TRUE;
                break;
            case OSM_GPS_MAP_GTK_KEY_UP:
                osm_gps_map_scroll(priv->map, 0, -step);
                /* priv->map_y -= step; */
                /* center_coord_update(map); */
                /* osm_gps_map_map_redraw_idle(map); */
                handled = TRUE;
                break;
            case OSM_GPS_MAP_GTK_KEY_DOWN:
                osm_gps_map_scroll(priv->map, 0, +step);
                /* priv->map_y += step; */
                /* center_coord_update(map); */
                /* osm_gps_map_map_redraw_idle(map); */
                handled = TRUE;
                break;
              case OSM_GPS_MAP_GTK_KEY_LEFT:
                  osm_gps_map_scroll(priv->map, -step, 0);
                /* priv->map_x -= step; */
                /* center_coord_update(map); */
                /* osm_gps_map_map_redraw_idle(map); */
                handled = TRUE;
                break;
            case OSM_GPS_MAP_GTK_KEY_RIGHT:
                osm_gps_map_scroll(priv->map, +step, 0);
                /* priv->map_x += step; */
                /* center_coord_update(map); */
                /* osm_gps_map_map_redraw_idle(map); */
                handled = TRUE;
                break;
            default:
                break;
        }
    }

    return handled;
}

static void
osm_gps_map_gtk_init (OsmGpsMapGtk *object)
{
    int i;
    OsmGpsMapGtkPrivate *priv;

    priv = G_TYPE_INSTANCE_GET_PRIVATE (object, OSM_TYPE_GPS_MAP_GTK,
                                        OsmGpsMapGtkPrivate);
    object->priv = priv;

    priv->map = NULL;

    priv->drag_counter = 0;
    priv->drag_start_mouse_x = 0;
    priv->drag_start_mouse_y = 0;
    priv->drag_mouse_dx = 0;
    priv->drag_mouse_dy = 0;

    priv->keybindings_enabled = FALSE;
    for (i = 0; i < OSM_GPS_MAP_GTK_KEY_MAX; i++)
        priv->keybindings[i] = 0;

    gtk_widget_add_events (GTK_WIDGET (object),
                           GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK |
                           GDK_POINTER_MOTION_MASK |
                           GDK_KEY_PRESS_MASK | GDK_KEY_RELEASE_MASK);
    GTK_WIDGET_SET_FLAGS (object, GTK_CAN_FOCUS);

    //Setup signal handlers
    g_signal_connect(G_OBJECT(object), "key_press_event",
                     G_CALLBACK(on_window_key_press), (gpointer)0);
}

static void
osm_gps_map_gtk_dispose (GObject *object)
{
    OsmGpsMapGtk *map = OSM_GPS_MAP_GTK(object);
    OsmGpsMapGtkPrivate *priv = map->priv;

    if (priv->is_disposed)
        return;

    g_message("disposing.");
    priv->is_disposed = TRUE;

    if (priv->map)
      g_object_unref(priv->map);

    if (priv->drag_expose != 0)
        g_source_remove (priv->drag_expose);

    G_OBJECT_CLASS (osm_gps_map_gtk_parent_class)->dispose (object);
}

static void
osm_gps_map_gtk_finalize (GObject *object)
{
    /* OsmGpsMapGtk *map = OSM_GPS_MAP_GTK(object); */
    /* OsmGpsMapGtkPrivate *priv = map->priv; */

    G_OBJECT_CLASS (osm_gps_map_gtk_parent_class)->finalize (object);
}

static void
osm_gps_map_gtk_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (OSM_IS_GPS_MAP_GTK (object));
    OsmGpsMapGtk *map = OSM_GPS_MAP_GTK(object);
    OsmGpsMapGtkPrivate *priv = map->priv;

    switch (prop_id)
    {
        case PROP_DRAG_LIMIT:
            priv->drag_limit = g_value_get_int (value);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static void
osm_gps_map_gtk_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
    g_return_if_fail (OSM_IS_GPS_MAP_GTK (object));
    OsmGpsMapGtk *map = OSM_GPS_MAP_GTK(object);
    OsmGpsMapGtkPrivate *priv = map->priv;

    switch (prop_id)
    {
        case PROP_DRAG_LIMIT:
            g_value_set_int(value, priv->drag_limit);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
            break;
    }
}

static gboolean
osm_gps_map_gtk_scroll_event (GtkWidget *widget, GdkEventScroll  *event)
{
    OsmGpsMapGtk *map = OSM_GPS_MAP_GTK(widget);

    if (event->direction == GDK_SCROLL_UP)
        osm_gps_map_zoom_in(map->priv->map);
    else if (event->direction == GDK_SCROLL_DOWN)
        osm_gps_map_zoom_out(map->priv->map);

    return FALSE;
}

static gboolean
osm_gps_map_gtk_button_press (GtkWidget *widget, GdkEventButton *event)
{
    OsmGpsMapGtk *map = OSM_GPS_MAP_GTK(widget);
    OsmGpsMapGtkPrivate *priv = map->priv;

#ifdef ENABLE_OSD
    osm_gps_map_osd_t *osd;

    osd = osm_gps_map_osd_get(priv->map);
    if(osd) {
        /* pressed inside OSD control? */
        osd_button_t but = 
            osd->check(osd, TRUE, event->x, event->y);

        if(but != OSD_NONE)
            {
                int step = 
                    widget->allocation.width/OSM_GPS_MAP_SCROLL_STEP;
                priv->drag_counter = -1;
            
                switch(but) {
                case OSD_UP:
                    osm_gps_map_scroll(priv->map, 0, -step);
                    g_object_set(G_OBJECT(priv->map), "auto-center", FALSE, NULL);
                    break;

                case OSD_DOWN:
                    osm_gps_map_scroll(priv->map, 0, +step);
                    g_object_set(G_OBJECT(priv->map), "auto-center", FALSE, NULL);
                    break;

                case OSD_LEFT:
                    osm_gps_map_scroll(priv->map, -step, 0);
                    g_object_set(G_OBJECT(priv->map), "auto-center", FALSE, NULL);
                    break;
                
                case OSD_RIGHT:
                    osm_gps_map_scroll(priv->map, +step, 0);
                    g_object_set(G_OBJECT(priv->map), "auto-center", FALSE, NULL);
                    break;
                
                case OSD_IN:
                    osm_gps_map_zoom_in(priv->map);
                    break;
                
                case OSD_OUT:
                    osm_gps_map_zoom_out(priv->map);
                    break;
                
                default:
                    /* all custom buttons are forwarded to the application */
                    if(osd->cb)
                        osd->cb(but, osd->data);
                    break;
                }
            
                return FALSE;
            }
    }
#endif

    priv->button_down = TRUE;
    priv->drag_counter = 0;
    priv->drag_start_mouse_x = (int) event->x;
    priv->drag_start_mouse_y = (int) event->y;
    g_object_get(G_OBJECT(priv->map), "map-x", &priv->drag_start_map_x,
                 "map-y", &priv->drag_start_map_y, NULL);
    g_message("press at %dx%d.", priv->drag_start_map_x, priv->drag_start_map_y);
    g_message("mouse at %dx%d.", priv->drag_start_mouse_x, priv->drag_start_mouse_y);

    return FALSE;
}

static gboolean
osm_gps_map_gtk_button_release (GtkWidget *widget, GdkEventButton *event)
{
    OsmGpsMapGtk *map = OSM_GPS_MAP_GTK(widget);
    OsmGpsMapGtkPrivate *priv = map->priv;
#ifdef ENABLE_OSD
    osm_gps_map_osd_t *osd;
#endif

    if(!priv->button_down)
        return FALSE;

    if (priv->dragging)
    {
        priv->dragging = FALSE;
        priv->drag_mouse_dx = 0;
        priv->drag_mouse_dy = 0;

        osm_gps_map_scroll(priv->map, priv->drag_start_mouse_x - (int) event->x,
                           priv->drag_start_mouse_y - (int) event->y);
    }
#ifdef ENABLE_OSD
    osd = osm_gps_map_osd_get(priv->map);
    if(osd)
        osd->check(osd, FALSE, event->x, event->y);
#endif

#ifdef DRAG_DEBUG
    printf("dragging done\n");
#endif

    priv->drag_counter = -1;
    priv->button_down = 0;

    return FALSE;
}

static gboolean
osm_gps_map_gtk_expose (GtkWidget *widget, GdkEventExpose  *event);

static gboolean
osm_gps_map_gtk_idle_expose (GtkWidget *widget)
{
    OsmGpsMapGtkPrivate *priv = OSM_GPS_MAP_GTK(widget)->priv;

    priv->drag_expose = 0;
    osm_gps_map_gtk_expose (widget, NULL);
    return FALSE;
}

static gboolean
osm_gps_map_gtk_motion_notify (GtkWidget *widget, GdkEventMotion  *event)
{
    int x, y;
    GdkModifierType state;
    OsmGpsMapGtk *map = OSM_GPS_MAP_GTK(widget);
    OsmGpsMapGtkPrivate *priv = map->priv;

    if(!priv->button_down)
        return FALSE;

    if (event->is_hint)
        gdk_window_get_pointer (event->window, &x, &y, &state);
    else
    {
        x = event->x;
        y = event->y;
        state = event->state;
    }

    // are we being dragged
    if (!(state & GDK_BUTTON1_MASK))
        return FALSE;

    if (priv->drag_counter < 0) 
        return FALSE;

    /* not yet dragged far enough? */
    if(!priv->drag_counter &&
       ( (x - priv->drag_start_mouse_x) * (x - priv->drag_start_mouse_x) + 
         (y - priv->drag_start_mouse_y) * (y - priv->drag_start_mouse_y) <
         priv->drag_limit*priv->drag_limit))
        return FALSE;

    priv->drag_counter++;

    priv->dragging = TRUE;

    g_object_set(G_OBJECT(priv->map), "auto-center", FALSE, NULL);

    priv->drag_mouse_dx = x - priv->drag_start_mouse_x;
    priv->drag_mouse_dy = y - priv->drag_start_mouse_y;

    /* instead of redrawing directly just add an idle function */
    if (!priv->drag_expose)
        priv->drag_expose = 
            g_idle_add ((GSourceFunc)osm_gps_map_gtk_idle_expose, widget);

    return FALSE;
}

static gboolean
osm_gps_map_gtk_configure (GtkWidget *widget, GdkEventConfigure *event)
{
    /* Set viewport. */
    osm_gps_map_set_viewport(OSM_GPS_MAP_GTK_PRIVATE(widget)->map,
                             widget->allocation.width, widget->allocation.height);

    return FALSE;
}

static gboolean
osm_gps_map_gtk_expose (GtkWidget *widget, GdkEventExpose  *event)
{
    OsmGpsMapGtk *map = OSM_GPS_MAP_GTK(widget);
    OsmGpsMapGtkPrivate *priv = map->priv;
#ifdef ENABLE_OSD
    osm_gps_map_osd_t *osd;
#endif
    cairo_surface_t *source;
    cairo_t *cr;

#ifdef DRAG_DEBUG
    printf("expose, map %d/%d\n", priv->map_x, priv->map_y);
#endif
    g_message("expose, map.");
    
    source = osm_gps_map_get_surface(priv->map);
    cr = gdk_cairo_create(widget->window);
    if (!priv->drag_mouse_dx && !priv->drag_mouse_dy && event)
    {
#ifdef DRAG_DEBUG
        printf("  dragging = %d, event = %p\n", priv->dragging, event);
#endif
        g_message("transfering at %dx%d.",
                  event->area.x + EXTRA_BORDER, event->area.y + EXTRA_BORDER);
        cairo_set_source_surface(cr, source,
                                 -(event->area.x + EXTRA_BORDER),
                                 -(event->area.y + EXTRA_BORDER));
        cairo_rectangle(cr, event->area.x, event->area.y,
                        event->area.width, event->area.height);
        g_message("rect %dx%d over %dx%d", event->area.x, event->area.y,
                        event->area.width, event->area.height);
        cairo_fill(cr);
    }
    else
    {
#ifdef DRAG_DEBUG
        printf("  drag_mouse %d/%d\n", 
               priv->drag_mouse_dx - EXTRA_BORDER,
               priv->drag_mouse_dy - EXTRA_BORDER);
#endif
        cairo_set_source_surface(cr, source,
                                 priv->drag_mouse_dx - EXTRA_BORDER,
                                 priv->drag_mouse_dy - EXTRA_BORDER);
        cairo_paint(cr);
        
        //Paint white outside of the map if dragging. Its less
        //ugly than painting the corrupted map
        if(priv->drag_mouse_dx>EXTRA_BORDER) {
            cairo_rectangle(cr, 0, 0, priv->drag_mouse_dx - EXTRA_BORDER,
                            widget->allocation.width);
            cairo_set_source_rgb(cr, 1., 1., 1.);
            cairo_fill(cr);
        }
        else if (-priv->drag_mouse_dx > EXTRA_BORDER)
        {
            cairo_rectangle(cr, priv->drag_mouse_dx + widget->allocation.width + EXTRA_BORDER, 0,
                            -priv->drag_mouse_dx - EXTRA_BORDER,
                            widget->allocation.height);
            cairo_set_source_rgb(cr, 1., 1., 1.);
            cairo_fill(cr);
        }
        
        if (priv->drag_mouse_dy>EXTRA_BORDER) {
            cairo_rectangle(cr, 0, 0, widget->allocation.width,
                            priv->drag_mouse_dy - EXTRA_BORDER);
            cairo_set_source_rgb(cr, 1., 1., 1.);
            cairo_fill(cr);
        }
        else if (-priv->drag_mouse_dy > EXTRA_BORDER)
        {
            cairo_rectangle(cr, 0, priv->drag_mouse_dy + widget->allocation.height + EXTRA_BORDER,
                            widget->allocation.width,
                            -priv->drag_mouse_dy - EXTRA_BORDER);
            cairo_set_source_rgb(cr, 1., 1., 1.);
            cairo_fill(cr);
        }
    }
    cairo_surface_destroy(source);

#ifdef ENABLE_OSD
    /* draw new OSD */
    osd = osm_gps_map_osd_get(priv->map);
    if(osd) 
        osd->draw (osd, cr);
#endif
    cairo_destroy(cr);
        
    return FALSE;
}

static void osm_gps_map_gtk_repaint(OsmGpsMapGtk *widget, OsmGpsMap *map)
{
    OsmGpsMapGtkPrivate *priv = widget->priv;

    if (!priv->drag_expose)
        priv->drag_expose = g_idle_add((GSourceFunc)osm_gps_map_gtk_idle_expose, widget);
}

static void
osm_gps_map_gtk_class_init (OsmGpsMapGtkClass *klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);
    GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

    g_type_class_add_private (klass, sizeof (OsmGpsMapGtkPrivate));

    object_class->dispose = osm_gps_map_gtk_dispose;
    object_class->finalize = osm_gps_map_gtk_finalize;
    object_class->set_property = osm_gps_map_gtk_set_property;
    object_class->get_property = osm_gps_map_gtk_get_property;

    widget_class->expose_event = osm_gps_map_gtk_expose;
    widget_class->configure_event = osm_gps_map_gtk_configure;
    widget_class->button_press_event = osm_gps_map_gtk_button_press;
    widget_class->button_release_event = osm_gps_map_gtk_button_release;
    widget_class->motion_notify_event = osm_gps_map_gtk_motion_notify;
    widget_class->scroll_event = osm_gps_map_gtk_scroll_event;

    g_object_class_install_property (object_class,
                                     PROP_DRAG_LIMIT,
                                     g_param_spec_int ("drag-limit",
                                                       "drag limit",
                                                       "the number of pixels the user has to move the pointer in order to start dragging",
                                                       0,           /* minimum property value */
                                                       G_MAXINT,    /* maximum property value */
                                                       10,
                                                       G_PARAM_READABLE | G_PARAM_WRITABLE));
}

GtkWidget *
osm_gps_map_gtk_new (OsmGpsMap *map)
{
    GObject *obj;

    obj = g_object_new (OSM_TYPE_GPS_MAP_GTK, NULL);
    OSM_GPS_MAP_GTK_PRIVATE(obj)->map = map;
    g_object_ref(map);
    g_signal_connect_swapped(G_OBJECT(map), "dirty",
                             G_CALLBACK(osm_gps_map_gtk_repaint), obj);

    return GTK_WIDGET(obj);
}

void osm_gps_map_gtk_set_keyboard_shortcut (OsmGpsMapGtk *map,
                                            OsmGpsMapGtkKey_t key, guint keyval)
{
    g_return_if_fail (OSM_IS_GPS_MAP_GTK (map));
    g_return_if_fail(key < OSM_GPS_MAP_GTK_KEY_MAX);

    map->priv->keybindings[key] = keyval;
    map->priv->keybindings_enabled = TRUE;
}

OsmGpsMap* osm_gps_map_gtk_get_map (OsmGpsMapGtk *widget)
{
    g_return_val_if_fail (OSM_IS_GPS_MAP_GTK (widget), (OsmGpsMap*)0);
    
    return widget->priv->map;
}
