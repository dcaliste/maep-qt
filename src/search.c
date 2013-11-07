/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/* vim:set et sw=4 ts=4 cino=t0,(0: */
/*
 * search.c
 * Copyright (C) Till Harbaum 2009 <till@harbaum.org>
 * Copyright (C) Damien Caliste 2013 <dcaliste@free.fr>
 *
 * search.c is free software: you can redistribute it and/or modify it
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

#include "search.h"

struct _MaepSearchContextPrivate
{
  gboolean dispose_has_run;

  GSList *list_geonames_places;
  gboolean downloading_geonames;

  GSList *list_nominatim_places;
  gboolean downloading_nominatim;
};

enum {
  DOWNLOAD_ERROR_SIGNAL,
  PLACES_AVAILABLE_SIGNAL,
  LAST_SIGNAL
};
static guint _signals[LAST_SIGNAL] = { 0 };

static void maep_search_context_dispose(GObject* obj);
static void maep_search_context_finalize(GObject* obj);

G_DEFINE_TYPE(MaepSearchContext, maep_search_context, G_TYPE_OBJECT)

static void maep_search_context_class_init(MaepSearchContextClass *klass)
{
  g_message("Class init search context.");
  /* Connect the overloading methods. */
  G_OBJECT_CLASS(klass)->dispose      = maep_search_context_dispose;
  G_OBJECT_CLASS(klass)->finalize     = maep_search_context_finalize;

  _signals[DOWNLOAD_ERROR_SIGNAL] = 
    g_signal_new ("download-error", G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  0 , NULL, NULL, g_cclosure_marshal_VOID__UINT_POINTER,
                  G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_POINTER);

  _signals[PLACES_AVAILABLE_SIGNAL] = 
    g_signal_new ("places-available", G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  0 , NULL, NULL, g_cclosure_marshal_VOID__UINT_POINTER,
                  G_TYPE_NONE, 2, G_TYPE_UINT, G_TYPE_POINTER);

  g_type_class_add_private(klass, sizeof(MaepSearchContextPrivate));
}
static void maep_search_context_init(MaepSearchContext *obj)
{
  g_message("New search context %p.", (gpointer)obj);
  obj->priv = G_TYPE_INSTANCE_GET_PRIVATE(obj, MAEP_TYPE_SEARCH_CONTEXT,
                                          MaepSearchContextPrivate);
  obj->priv->dispose_has_run = FALSE;
  obj->priv->list_geonames_places = NULL;
  obj->priv->downloading_geonames = FALSE;
  obj->priv->list_nominatim_places = NULL;
  obj->priv->downloading_nominatim = FALSE;
}
static void maep_search_context_dispose(GObject* obj)
{
  MaepSearchContextPrivate *priv = MAEP_SEARCH_CONTEXT(obj)->priv;

  if (priv->dispose_has_run)
    return;
  priv->dispose_has_run = TRUE;

  /* Chain up to the parent class */
  G_OBJECT_CLASS(maep_search_context_parent_class)->dispose(obj);
}
static void maep_search_context_finalize(GObject* obj)
{
  MaepSearchContextPrivate *priv = MAEP_SEARCH_CONTEXT(obj)->priv;

  if (priv->list_geonames_places)
    maep_geonames_place_list_free(priv->list_geonames_places);
  if (priv->list_nominatim_places)
    maep_geonames_place_list_free(priv->list_nominatim_places);

  G_OBJECT_CLASS(maep_search_context_parent_class)->finalize(obj);
}

MaepSearchContext* maep_search_context_new()
{
  MaepSearchContext *search;

  search = MAEP_SEARCH_CONTEXT(g_object_new(MAEP_TYPE_SEARCH_CONTEXT, NULL));
  return search;
}

static void nominatim_search_cb(MaepSearchContext *context, GSList *list, GError *error) {
  if (!error)
    {
      /* remove any list that may already be preset */
      if(context->priv->list_nominatim_places) {
	maep_geonames_place_list_free(context->priv->list_nominatim_places);
	context->priv->list_nominatim_places = NULL;
      }
      
      /* render all icons */
      context->priv->list_nominatim_places = list;
      g_signal_emit(context, _signals[PLACES_AVAILABLE_SIGNAL], 0,
                    MaepSearchContextNominatim, list, NULL);
    }
  else
    {
      g_warning("%s", error->message);
      g_signal_emit(context, _signals[DOWNLOAD_ERROR_SIGNAL], 0,
                    MaepSearchContextNominatim, error, NULL);
    }

  context->priv->downloading_nominatim = FALSE;
  g_object_unref(context);
}

static void geonames_search_cb(MaepSearchContext *context, GSList *list, GError *error) {
  if (!error)
    {
      /* remove any list that may already be preset */
      if(context->priv->list_geonames_places) {
	maep_geonames_place_list_free(context->priv->list_geonames_places);
	context->priv->list_geonames_places = NULL;
      }
      
      /* render all icons */
      context->priv->list_geonames_places = list;
      g_signal_emit(context, _signals[PLACES_AVAILABLE_SIGNAL], 0,
                    MaepSearchContextGeonames, list, NULL);
    }
  else
    {
      g_warning("%s", error->message);
      g_signal_emit(context, _signals[DOWNLOAD_ERROR_SIGNAL], 0,
                    MaepSearchContextGeonames, error, NULL);
    }

  context->priv->downloading_geonames = FALSE;
  g_object_unref(context);
}

/* request geotagged wikipedia entries for current map view */
void maep_search_context_request(MaepSearchContext *context, const gchar *request,
                                 guint sources) {
  if (sources & MaepSearchContextGeonames && !context->priv->downloading_geonames)
    {
      context->priv->downloading_geonames = TRUE;

      g_object_ref(context);
      maep_geonames_place_request(request,
                                  (MaepGeonamesRequestCallback)geonames_search_cb,
                                  context);
    }
  if (sources & MaepSearchContextNominatim && !context->priv->downloading_nominatim)
    {
      context->priv->downloading_nominatim = TRUE;

      g_object_ref(context);
      maep_nominatim_address_request(request,
                                     (MaepGeonamesRequestCallback)nominatim_search_cb,
                                     context);
    }
}
