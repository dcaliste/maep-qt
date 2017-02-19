/*
 * Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
 *
 * Contributions by
 * Damien Caliste 2014 <dcaliste@free.fr>
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

#include <math.h>
#include "layer-gps.h"
#include "osm-gps-map-layer.h"

struct _MaepLayerGpsPrivate
{
  gboolean dispose_has_run;

  coord_t gps;
  float gps_heading;
  float compass_azimuth;
  MaepLayerCompassMode compass_mode;
  gboolean gps_valid;

  guint ui_gps_point_inner_radius;
  guint ui_gps_point_outer_radius;

  cairo_surface_t *surf;
};

enum
{
    PROP_0,

    PROP_GPS_POINT_R1,
    PROP_GPS_POINT_R2,

    PROP_LAST
};
static GParamSpec *properties[PROP_LAST];
enum {
  DIRTY_SIGNAL,
  LAST_SIGNAL
};
static guint _signals[LAST_SIGNAL] = { 0 };

static void maep_layer_gps_dispose(GObject* obj);
static void maep_layer_gps_finalize(GObject* obj);
static void maep_layer_gps_set_property (GObject *object, guint prop_id,
                                         const GValue *value, GParamSpec *pspec);
static void maep_layer_gps_get_property (GObject *object, guint prop_id,
                                         GValue *value, GParamSpec *pspec);
static void osm_gps_map_layer_interface_init(OsmGpsMapLayerIface *iface);
static void maep_layer_gps_draw(OsmGpsMapLayer *self, cairo_t *cr,
                                OsmGpsMap *map);

G_DEFINE_TYPE_WITH_CODE(MaepLayerGps, maep_layer_gps,
                        G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(OSM_TYPE_GPS_MAP_LAYER,
                                              osm_gps_map_layer_interface_init))

static void maep_layer_gps_class_init(MaepLayerGpsClass *klass)
{
  GObjectClass *oclass = G_OBJECT_CLASS(klass);

  g_message("Class init gps layer context.");
  /* Connect the overloading methods. */
  oclass->dispose      = maep_layer_gps_dispose;
  oclass->finalize     = maep_layer_gps_finalize;
  oclass->set_property = maep_layer_gps_set_property;
  oclass->get_property = maep_layer_gps_get_property;

  _signals[DIRTY_SIGNAL] = 
    g_signal_new ("dirty", G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  0 , NULL, NULL, g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  properties[PROP_GPS_POINT_R1] =
    g_param_spec_uint ("gps-point-radius", "gps-point-radius",
                       "radius of the gps point inner circle",
                       0, G_MAXUINT, 10, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_class_install_property (oclass, PROP_GPS_POINT_R1,
                                   properties[PROP_GPS_POINT_R1]);

  properties[PROP_GPS_POINT_R2] =
    g_param_spec_uint ("gps-highlight-radius", "gps-highlight-radius",
                       "radius of the gps point highlight circle",
                       0, G_MAXUINT, 0, G_PARAM_READWRITE | G_PARAM_CONSTRUCT);
  g_object_class_install_property (oclass, PROP_GPS_POINT_R2,
                                   properties[PROP_GPS_POINT_R2]);

  g_type_class_add_private(klass, sizeof(MaepLayerGpsPrivate));
}
static void osm_gps_map_layer_interface_init(OsmGpsMapLayerIface *iface)
{
  g_message("setup layer interface for gps layer context.");
  iface->render = NULL;
  iface->draw = maep_layer_gps_draw;
  iface->busy = NULL;
  iface->button = NULL;
}
static void maep_layer_gps_init(MaepLayerGps *obj)
{
  g_message("New layer gps %p.", (gpointer)obj);
  obj->priv = G_TYPE_INSTANCE_GET_PRIVATE(obj, MAEP_TYPE_LAYER_GPS,
                                          MaepLayerGpsPrivate);
  obj->priv->dispose_has_run = FALSE;

  obj->priv->gps_valid = FALSE;

  obj->priv->surf = NULL;
}
static void maep_layer_gps_dispose(GObject* obj)
{
  MaepLayerGpsPrivate *priv = MAEP_LAYER_GPS(obj)->priv;

  if (priv->dispose_has_run)
    return;
  priv->dispose_has_run = TRUE;

  /* Chain up to the parent class */
  G_OBJECT_CLASS(maep_layer_gps_parent_class)->dispose(obj);
}
static void maep_layer_gps_finalize(GObject* obj)
{
  MaepLayerGpsPrivate *priv = MAEP_LAYER_GPS(obj)->priv;

  if (priv->surf)
    cairo_surface_destroy(priv->surf);

  G_OBJECT_CLASS(maep_layer_gps_parent_class)->finalize(obj);
}
static void maep_layer_gps_set_property (GObject *object, guint prop_id, const GValue *value, GParamSpec *pspec)
{
  cairo_t *cr;
  cairo_pattern_t *pat;
  double r;

  g_return_if_fail (MAEP_IS_LAYER_GPS (object));
  MaepLayerGpsPrivate *priv = MAEP_LAYER_GPS(object)->priv;

  switch (prop_id)
    {
    case PROP_GPS_POINT_R1:
      priv->ui_gps_point_inner_radius = g_value_get_uint (value);
      if (priv->surf)
        cairo_surface_destroy(priv->surf);
      priv->surf = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, 
                                              priv->ui_gps_point_inner_radius * 2 + 2,
                                              priv->ui_gps_point_inner_radius * 2 + 2);
      cr = cairo_create(priv->surf);
      cairo_translate(cr, priv->ui_gps_point_inner_radius + 1, priv->ui_gps_point_inner_radius + 1);
      r = (double)(priv->ui_gps_point_inner_radius/5);
      pat = cairo_pattern_create_radial(-r, -r, r, 0., 0., 5 * r);
      cairo_pattern_add_color_stop_rgba (pat, 0, 1, 1, 1, 1.0);
      cairo_pattern_add_color_stop_rgba (pat, 1, 0, 0, 1, 1.0);
      cairo_set_source (cr, pat);
      /* cairo_set_source_rgba (cr, 0.0, 0.0, 1.0, 1.0); */
      cairo_arc (cr, 0., 0., priv->ui_gps_point_inner_radius, 0, 2 * M_PI);
      cairo_fill_preserve (cr);
      // draw ball border
      cairo_set_line_width (cr, 1.0);
      cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 1.0);
      cairo_stroke(cr);
      cairo_pattern_destroy(pat);
      cairo_destroy(cr);
      break;
    case PROP_GPS_POINT_R2:
      priv->ui_gps_point_outer_radius = g_value_get_uint (value);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void maep_layer_gps_get_property (GObject *object, guint prop_id, GValue *value, GParamSpec *pspec)
{
  g_return_if_fail (MAEP_IS_LAYER_GPS (object));
  MaepLayerGpsPrivate *priv = MAEP_LAYER_GPS(object)->priv;

  switch (prop_id)
    {
    case PROP_GPS_POINT_R1:
      g_value_set_uint(value, priv->ui_gps_point_inner_radius);
      break;
    case PROP_GPS_POINT_R2:
      g_value_set_uint(value, priv->ui_gps_point_outer_radius);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void _draw(MaepLayerGpsPrivate *priv, cairo_t *cr, OsmGpsMap *map)
{
  int r = priv->ui_gps_point_inner_radius;
  double r2 = (double)priv->ui_gps_point_outer_radius;

  // draw transparent area
  if (r2 > 0.) {
    /* Transform meters to pixels. */
    r2 /= osm_gps_map_get_scale(map);
    cairo_set_source_rgba (cr, 0.75, 0.75, 0.75, 0.4);
    cairo_arc (cr, 0., 0., r2, 0, 2 * M_PI);
    cairo_fill_preserve (cr);
    // draw transparent area border
    cairo_set_line_width (cr, 1.5);
    cairo_set_source_rgba (cr, 0.55, 0.55, 0.55, 0.4);
    cairo_stroke(cr);
  }

  // draw ball gradient
  if (r > 0) {
    // draw magnetic compass
    switch ((isnan(priv->compass_azimuth) ? COMPASS_MODE_OFF : priv->compass_mode))
      {
      case COMPASS_MODE_DEVICE:
        cairo_move_to (cr, -r*cos(priv->compass_azimuth), -r*sin(priv->compass_azimuth));
        cairo_line_to (cr, 4.0*r*sin(priv->compass_azimuth), -4.0*r*cos(priv->compass_azimuth));
        cairo_line_to (cr, r*cos(priv->compass_azimuth), r*sin(priv->compass_azimuth));
        cairo_close_path (cr);

        cairo_set_source_rgba (cr, 0.0, 1.0, 1.0, 0.5);
        cairo_fill_preserve (cr);

        cairo_set_line_width (cr, 1.0);
        cairo_set_source_rgba (cr, 0.0, 0.5, 0.5, 0.5);
        cairo_stroke(cr);
        break;
      case COMPASS_MODE_NORTH:
        cairo_move_to (cr, -r*cos(priv->compass_azimuth), r*sin(priv->compass_azimuth));
        cairo_line_to (cr, -4.0*r*sin(priv->compass_azimuth), -4.0*r*cos(priv->compass_azimuth));
        cairo_line_to (cr, r*cos(priv->compass_azimuth), -r*sin(priv->compass_azimuth));
        cairo_close_path (cr);

        cairo_set_source_rgba (cr, 0.3, 1.0, 0.3, 0.5);
        cairo_fill_preserve (cr);

        cairo_set_line_width (cr, 1.0);
        cairo_set_source_rgba (cr, 0.0, 0.5, 0.0, 0.5);
        cairo_stroke(cr);


        cairo_move_to (cr, r*cos(priv->compass_azimuth), -r*sin(priv->compass_azimuth));
        cairo_line_to (cr, 4.0*r*sin(priv->compass_azimuth), 4.0*r*cos(priv->compass_azimuth));
        cairo_line_to (cr, -r*cos(priv->compass_azimuth), r*sin(priv->compass_azimuth));
        cairo_close_path (cr);

        cairo_set_source_rgba (cr, 1.0, 0.3, 0.3, 0.5);
        cairo_fill_preserve (cr);

        cairo_set_line_width (cr, 1.0);
        cairo_set_source_rgba (cr, 0.5, 0.0, 0.0, 0.5);
        cairo_stroke(cr);
        break;
      default:
        break;
      }
    // draw direction arrow
    if(!isnan(priv->gps_heading)) 
      {
        cairo_move_to (cr, -r*cos(priv->gps_heading), -r*sin(priv->gps_heading));
        cairo_line_to (cr, 3*r*sin(priv->gps_heading), -3*r*cos(priv->gps_heading));
        cairo_line_to (cr, r*cos(priv->gps_heading), r*sin(priv->gps_heading));
        cairo_close_path (cr);

        cairo_set_source_rgba (cr, 0.3, 0.3, 1.0, 0.5);
        cairo_fill_preserve (cr);

        cairo_set_line_width (cr, 1.0);
        cairo_set_source_rgba (cr, 0.0, 0.0, 0.0, 0.5);
        cairo_stroke(cr);
      }

    cairo_set_source_surface (cr, priv->surf, -r - 1, -r - 1);
    cairo_paint(cr);
  }
}

static void maep_layer_gps_draw(OsmGpsMapLayer *self, cairo_t *cr,
                                OsmGpsMap *map)
{
  int pixel_x,pixel_y;
  MaepLayerGpsPrivate *priv = MAEP_LAYER_GPS(self)->priv;

  if (!priv->gps_valid)
    return;

  osm_gps_map_from_co_ordinates(map, &priv->gps, &pixel_x, &pixel_y);
  
  cairo_save(cr);
  cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

  cairo_translate(cr, pixel_x, pixel_y);
  _draw(priv, cr, map);

  cairo_restore(cr);
}

MaepLayerGps* maep_layer_gps_new(void)
{
  return g_object_new(MAEP_TYPE_LAYER_GPS, NULL);
}

gboolean maep_layer_gps_set_coordinates(MaepLayerGps *gps, gfloat lat, gfloat lon,
                                        gfloat hprec, gfloat heading)
{
  gboolean changed;
  g_return_val_if_fail(MAEP_IS_LAYER_GPS(gps), FALSE);

  changed = FALSE;

  hprec = MAX(hprec, 0.f);
  if (gps->priv->ui_gps_point_outer_radius != hprec)
    {
      changed = TRUE;
      gps->priv->ui_gps_point_outer_radius = hprec;
      g_object_notify_by_pspec(G_OBJECT(gps), properties[PROP_GPS_POINT_R2]);
    }

  heading = deg2rad(heading);
  if (gps->priv->gps_heading != heading)
    {
      changed = TRUE;
      gps->priv->gps_heading = heading;
    }

  lat = deg2rad(lat);
  lon = deg2rad(lon);
  if (gps->priv->gps.rlat != lat || gps->priv->gps.rlon != lon)
    {
      changed = TRUE;
      gps->priv->gps.rlat = lat;
      gps->priv->gps.rlon = lon;
    }

  if (changed)
    g_signal_emit(gps, _signals[DIRTY_SIGNAL], 0, NULL);

  return changed;
}

gboolean maep_layer_gps_set_azimuth(MaepLayerGps *gps, gfloat azimuth)
{
  g_return_val_if_fail(MAEP_IS_LAYER_GPS(gps), FALSE);

  azimuth = deg2rad(azimuth);
  if (gps->priv->compass_azimuth != azimuth)
    {
      gps->priv->compass_azimuth = azimuth;
      g_signal_emit(gps, _signals[DIRTY_SIGNAL], 0, NULL);
      return TRUE;
    }
  return FALSE;
}

gboolean maep_layer_gps_set_compass_mode(MaepLayerGps *gps, MaepLayerCompassMode mode)
{
  g_return_val_if_fail(MAEP_IS_LAYER_GPS(gps), FALSE);
  if (gps->priv->compass_mode != mode)
    {
      gps->priv->compass_mode = mode;
      g_signal_emit(gps, _signals[DIRTY_SIGNAL], 0, NULL);
      return TRUE;
    }
  return FALSE;
}

gboolean maep_layer_gps_set_active(MaepLayerGps *gps, gboolean status)
{
  g_return_val_if_fail(MAEP_IS_LAYER_GPS(gps), FALSE);

  if (gps->priv->gps_valid == status)
    return FALSE;

  gps->priv->gps_valid = status;

  g_signal_emit(gps, _signals[DIRTY_SIGNAL], 0, NULL);
  return TRUE;
}
