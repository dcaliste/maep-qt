/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
 *
 * Contributions by
 * Damien Caliste 2017 <dcaliste@free.fr>
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

#include "source.h"

#include "../config.h"

#include <string.h>
#include <libsoup/soup.h>
#include <glib/gstdio.h>

struct _MaepSource
{
    guint ref_count;

    guint id;
    gchar *name;
    gchar *repo_uri;
    gchar *image_suffix;
    gchar *copyright_notice;
    gchar *copyright_url;
    guint min_zoom, max_zoom;
    guint cache_period;
    gboolean cache_policy;
    gboolean active;
    int uri_format;
};

#define DEFAULT_PERIOD (60*60*24*7)

#define URI_MARKER_X    "#X"
#define URI_MARKER_Y    "#Y"
#define URI_MARKER_Z    "#Z"
#define URI_MARKER_S    "#S"
#define URI_MARKER_Q    "#Q"
#define URI_MARKER_Q0   "#W"
#define URI_MARKER_YS   "#U"
#define URI_MARKER_R    "#R"
#define URI_MARKER_T    "#T"

static int _inspect_map_uri(const gchar *repo_uri)
{
    int uri_format;

    uri_format = 0;

    if (g_strrstr(repo_uri, URI_MARKER_X))
        uri_format |= MAEP_SOURCE_HAS_X;

    if (g_strrstr(repo_uri, URI_MARKER_Y))
        uri_format |= MAEP_SOURCE_HAS_Y;

    if (g_strrstr(repo_uri, URI_MARKER_Z))
        uri_format |= MAEP_SOURCE_HAS_Z;

    if (g_strrstr(repo_uri, URI_MARKER_S))
        uri_format |= MAEP_SOURCE_HAS_S;

    if (g_strrstr(repo_uri, URI_MARKER_Q))
        uri_format |= MAEP_SOURCE_HAS_Q;

    if (g_strrstr(repo_uri, URI_MARKER_Q0))
        uri_format |= MAEP_SOURCE_HAS_Q0;

    if (g_strrstr(repo_uri, URI_MARKER_YS))
        uri_format |= MAEP_SOURCE_HAS_YS;

    if (g_strrstr(repo_uri, URI_MARKER_R))
        uri_format |= MAEP_SOURCE_HAS_R;

    if (g_strrstr(repo_uri, URI_MARKER_T))
        uri_format |= MAEP_SOURCE_HAS_TR;

    if (g_strrstr(repo_uri, "google.com"))
        uri_format |= MAEP_SOURCE_HAS_GOOGLE_DOMAIN;

    g_debug("URI Format: 0x%X (google: %X)", uri_format,
            uri_format & MAEP_SOURCE_HAS_GOOGLE_DOMAIN);

    return uri_format;
}

static MaepSource* _sourceRef(MaepSource *source)
{
    g_return_if_fail(source);

    source->ref_count += 1;
    return source;
}

static void _sourceFree(MaepSource *source)
{
    g_return_if_fail(source);

    source->ref_count -= 1;
    if (!source->ref_count) {
        g_free(source->name);
        g_free(source->repo_uri);
        g_free(source->image_suffix);
        g_free(source->copyright_notice);
        g_free(source->copyright_url);
        g_free(source);
    }
}

static MaepSource* _sourceNew(const gchar *name,
                              const gchar *repo_uri,
                              const gchar *image_suffix,
                              const gchar *copyright_notice,
                              const gchar *copyright_url,
                              guint min_zoom, guint max_zoom,
                              guint cache_period,
                              gboolean cache_policy)
{
    MaepSource *source;

    source = g_malloc(sizeof(MaepSource));
    source->ref_count = 1;
    source->name = g_strdup(name);
    source->repo_uri = g_strdup(repo_uri);
    source->image_suffix = g_strdup(image_suffix);
    source->copyright_notice = g_strdup(copyright_notice);
    source->copyright_url = g_strdup(copyright_url);
    source->min_zoom = min_zoom;
    source->max_zoom = max_zoom;
    source->cache_period = cache_period;
    source->cache_policy = cache_policy;
    source->active = TRUE;
    source->uri_format = _inspect_map_uri(repo_uri);

    return source;
}


#define USER_AGENT                  PACKAGE "-libsoup/" VERSION

struct _MaepSourceManagerPrivate
{
    gchar *tile_dir;
    GHashTable *sources;
    GHashTable *sourcesById;
    guint userId;

    //how we download tiles
    SoupSession *soup_session;
    char *proxy_uri;
    GHashTable *tile_queue;
    GHashTable *missing_tiles;

    const MaepSource *current;
    gchar *cache_dir;
};

enum
  {
    PROP_0,
    N_SOURCES_PROP,
    CACHE_DIR_PROP,
    PROXY_URI_PROP,
    TILES_QUEUED_PROP,
    N_PROP
  };
static GParamSpec *_properties[N_PROP];
enum {
    TILE_SAVED,
    TILE_RECEIVED,
    LAST_SIGNAL
};
static guint _signals[LAST_SIGNAL] = { 0 };


G_DEFINE_TYPE(MaepSourceManager, maep_source_manager, G_TYPE_OBJECT)

static void maep_source_manager_finalize(GObject *obj);
static void maep_source_manager_get_property(GObject* obj, guint property_id,
                                             GValue *value, GParamSpec *pspec);
static void maep_source_manager_set_property(GObject* obj, guint property_id,
                                             const GValue *value, GParamSpec *pspec);
static void _download_tile(MaepSourceManager *manager,
                           const MaepSource *source,
                           int zoom, int x, int y);
#if USE_LIBSOUP22
static void _tile_download_complete(SoupMessage *msg, gpointer user_data);
#else
static void _tile_download_complete(SoupSession *session, SoupMessage *msg, gpointer user_data);
#endif

static void maep_source_manager_class_init(MaepSourceManagerClass *klass)
{
  /* Connect the overloading methods. */
  G_OBJECT_CLASS(klass)->finalize = maep_source_manager_finalize;
  G_OBJECT_CLASS(klass)->get_property = maep_source_manager_get_property;
  G_OBJECT_CLASS(klass)->set_property = maep_source_manager_set_property;

  /**
   * MaepSourceManager::n-sources:
   *
   * Number of managed sources.
   */
  _properties[N_SOURCES_PROP] =
    g_param_spec_uint("n-sources", "Number of sources", "number of sources",
                      0, G_MAXUINT, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);
  /**
   * MaepSourceManager::cache-dir:
   *
   * Where to put the cache tiles.
   */
  _properties[CACHE_DIR_PROP] =
    g_param_spec_string("cache-dir", "Cache directory", "where to cache tiles",
                        NULL, G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS);
  _properties[PROXY_URI_PROP] =
      g_param_spec_string("proxy-uri", "proxy uri", "http proxy uri on NULL",
                          NULL, G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY);
  _properties[TILES_QUEUED_PROP] =
      g_param_spec_uint("tiles-queued", "tiles-queued", "number of tiles currently waiting to download",
                        0, G_MAXUINT, 0, G_PARAM_READABLE | G_PARAM_STATIC_STRINGS);

  g_object_class_install_properties(G_OBJECT_CLASS(klass), N_PROP, _properties);

  _signals[TILE_SAVED] =
    g_signal_new("tile-saved", G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                 0, NULL, NULL, g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING, NULL);
  _signals[TILE_RECEIVED] =
    g_signal_new("tile-received", G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                 0, NULL, NULL, g_cclosure_marshal_VOID__STRING,
                 G_TYPE_NONE, 1, G_TYPE_STRING, NULL);

  g_type_class_add_private(klass, sizeof(MaepSourceManagerPrivate));
}

static void maep_source_manager_init(MaepSourceManager *self)
{
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self, MAEP_TYPE_SOURCE_MANAGER,
                                           MaepSourceManagerPrivate);
  self->priv->tile_dir = NULL;
  self->priv->sources = g_hash_table_new_full(g_str_hash, g_str_equal,
                                              NULL, (GDestroyNotify)_sourceFree);
  self->priv->sourcesById = g_hash_table_new(g_direct_hash, g_direct_equal);

  self->priv->userId = 0;
  self->priv->current = NULL;
  self->priv->cache_dir = NULL;

#if USE_LIBSOUP22
    /* libsoup-2.2 has no special way to set the user agent, so we */
    /* set it seperately as an extra header field for each reuest */
  self->priv->soup_session = soup_session_async_new();
#else
#if SOUP_CHECK_VERSION(2,42,0)
  self->priv->soup_session =
      soup_session_new_with_options(SOUP_SESSION_USER_AGENT,
                                    USER_AGENT, NULL);
#else
    /* set the user agent */
  self->priv->soup_session =
      soup_session_async_new_with_options(SOUP_SESSION_USER_AGENT,
                                          USER_AGENT, NULL);
#endif
#endif
    //Hash table which maps tile d/l URIs to SoupMessage requests
  self->priv->tile_queue = g_hash_table_new (g_str_hash, g_str_equal);

    //Some mapping providers (Google) have varying degrees of tiles at multiple
    //zoom levels
  self->priv->missing_tiles = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, NULL);
}

static void maep_source_manager_finalize(GObject* obj)
{
  MaepSourceManager *self;

  self = MAEP_SOURCE_MANAGER(obj);

  g_free(self->priv->tile_dir);
  g_hash_table_destroy(self->priv->sources);
  g_free(self->priv->cache_dir);
  g_free(self->priv->proxy_uri);

  soup_session_abort(self->priv->soup_session);
  g_object_unref(self->priv->soup_session);

  g_hash_table_destroy(self->priv->tile_queue);
  g_hash_table_destroy(self->priv->missing_tiles);

  G_OBJECT_CLASS(maep_source_manager_parent_class)->finalize(obj);
}

static void maep_source_manager_get_property(GObject* obj, guint property_id,
                                             GValue *value, GParamSpec *pspec)
{
  MaepSourceManager *self = MAEP_SOURCE_MANAGER(obj);

  switch (property_id) {
  case N_SOURCES_PROP:
      g_value_set_uint(value, g_hash_table_size(self->priv->sources));
      break;
  case CACHE_DIR_PROP:
      g_value_set_string(value, self->priv->tile_dir);
      break;
  case PROXY_URI_PROP:
      g_value_set_string(value, self->priv->proxy_uri);
      break;
  case TILES_QUEUED_PROP:
      g_value_set_uint(value, g_hash_table_size(self->priv->tile_queue));
      break;
  default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
      break;
  }
}

static void maep_source_manager_set_property(GObject* obj, guint property_id,
                                             const GValue *value, GParamSpec *pspec)
{
  MaepSourceManager *self = MAEP_SOURCE_MANAGER(obj);

  switch (property_id) {
  case CACHE_DIR_PROP:
      g_free(self->priv->tile_dir);
      self->priv->tile_dir = g_value_dup_string(value);
      break;
  case PROXY_URI_PROP:
      if ( g_value_get_string(value) ) {
          self->priv->proxy_uri = g_value_dup_string (value);
          g_debug("Setting proxy server: %s", self->priv->proxy_uri);

#if USE_LIBSOUP22
          SoupUri* uri = soup_uri_new(self->priv->proxy_uri);
          g_object_set(G_OBJECT(self->priv->soup_session), SOUP_SESSION_PROXY_URI, uri, NULL);
#else
          GValue val = G_VALUE_INIT;
          SoupURI* uri = soup_uri_new(self->priv->proxy_uri);
          g_value_init(&val, SOUP_TYPE_URI);
          g_value_take_boxed(&val, uri);
          g_object_set_property(G_OBJECT(self->priv->soup_session),SOUP_SESSION_PROXY_URI,&val);
#endif
      } else
          self->priv->proxy_uri = NULL;
      break;
  default:
      /* We don't have any other property... */
      G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, property_id, pspec);
      break;
  }
}

static MaepSourceManager *_instance = NULL;
MaepSourceManager* maep_source_manager_get_instance(void)
{
    if (!_instance)
        _instance = g_object_new(MAEP_TYPE_SOURCE_MANAGER, NULL);
    return _instance;
}

static const MaepSource* _preset(MaepSourceManager *manager, MaepSourceId id)
{
    MaepSource *source;

    g_return_val_if_fail(MAEP_IS_SOURCE_MANAGER(manager), NULL);
    g_return_val_if_fail(id < MAEP_SOURCE_LAST, NULL);

    if (id == MAEP_SOURCE_NULL)
        return NULL;

    switch (id) {
    case MAEP_SOURCE_OPENSTREETMAP:
        source = _sourceNew
            ("OpenStreetMap I",
             "http://tile.openstreetmap.org/#Z/#X/#Y.png", "png",
             "© OpenStreetMap contributors",
             "http://www.openstreetmap.org/copyright", 1, 18, DEFAULT_PERIOD, TRUE);
        break;
    case MAEP_SOURCE_MML_PERUSKARTTA:
        source = _sourceNew
            ("Peruskartta",
             "http://tiles.kartat.kapsi.fi/peruskartta/#Z/#X/#Y.png", "png",
             "CC 4.0 licence (© Maanmittauslaitos)",
             "http://www.maanmittauslaitos.fi/avoimen-tietoaineiston-cc-40-lisenssi",
             1, 20, DEFAULT_PERIOD, TRUE);
        break;
    case MAEP_SOURCE_MML_ORTOKUVA:
        source = _sourceNew
            ("Ortoilmakuva",
             "http://tiles.kartat.kapsi.fi/ortokuva/#Z/#X/#Y.png", "png",
             "CC 4.0 licence (© Maanmittauslaitos)",
             "http://www.maanmittauslaitos.fi/avoimen-tietoaineiston-cc-40-lisenssi",
             1, 20, DEFAULT_PERIOD, TRUE);
        break;
    case MAEP_SOURCE_MML_TAUSTAKARTTA:
        source = _sourceNew
            ("Taustakartta",
             "http://tiles.kartat.kapsi.fi/taustakartta/#Z/#X/#Y.png", "png",
             "CC 4.0 licence (© Maanmittauslaitos)",
             "http://www.maanmittauslaitos.fi/avoimen-tietoaineiston-cc-40-lisenssi",
             1, 20, DEFAULT_PERIOD, TRUE);
        break;
    case MAEP_SOURCE_HIKE_AND_BIKE:
        source = _sourceNew
            ("Hike and bike",
             "http://#T.tiles.wmflabs.org/hikebike/#Z/#X/#Y.png", "png",
             "© OpenStreetMap contributors",
             "http://www.hikebikemap.org", 1, 18, DEFAULT_PERIOD, TRUE);
        source->active = FALSE;
        break;
    case MAEP_SOURCE_HILL_SHADING:
        source = _sourceNew
            ("Hill shading",
             "http://#T.tiles.wmflabs.org/hillshading/#Z/#X/#Y.png", "png",
             "© NASA (SRTM3 v2)",
             "http://www2.jpl.nasa.gov/srtm/", 1, 18, DEFAULT_PERIOD, TRUE);
        source->active = FALSE;
        break;
    case MAEP_SOURCE_OPENAERIALMAP:
        /* OpenAerialMap is down, offline till furthur notice
           http://openaerialmap.org/pipermail/talk_openaerialmap.org/2008-December/000055.html */
        source = _sourceNew("OpenAerialMap", NULL, "png", NULL, NULL,
                            1, 17, DEFAULT_PERIOD, TRUE);
        break;
    case MAEP_SOURCE_OPENSEAMAP:
        source = _sourceNew
            ("OpenSeaMap",
             "http://t1.openseamap.org/seamark/#Z/#X/#Y.png", "png",
             "© OpenStreetMap contributors",
             "http://openseamap.org/", 1, 18, DEFAULT_PERIOD, TRUE);
        break;
    case MAEP_SOURCE_OPENSTREETMAP_RENDERER:
        source = _sourceNew
            ("OpenStreetMap II",
             "http://otile1.mqcdn.com/tiles/1.0.0/osm/#Z/#X/#Y.png", "png",
             "Tiles Courtesy of MapQuest",
             "http://www.mapquest.com/",
             1, 17, DEFAULT_PERIOD, TRUE);
        break;
        /* "http://tah.openstreetmap.org/Tiles/tile/#Z/#X/#Y.png"; */
    case MAEP_SOURCE_OPENCYCLEMAP:
        source = _sourceNew
            ("OpenCycleMap",
             "https://tile.thunderforest.com/cycle/#Z/#X/#Y.png?apikey=20a6951bc9c2496ab5a7b1ef728e8d92", "png",
             "Map © Thunderforest, data © www.osm.org/copyright",
             "http://www.thunderforest.com",
             1, 18, DEFAULT_PERIOD, TRUE);
        break;
    case MAEP_SOURCE_OSM_PUBLIC_TRANSPORT:
        source = _sourceNew
            ("Public Transport",
             "http://tile.memomaps.de/tilegen/#Z/#X/#Y.png", "png",
             "CC-BY-SA license (© by MeMomaps)",
             "http://memomaps.de",
             1, 18, DEFAULT_PERIOD, TRUE);
        break;
    case MAEP_SOURCE_OSMC_TRAILS:
        source = _sourceNew
            ("OSMC Trails",
             "http://topo.geofabrik.de/trails/#Z/#X/#Y.png", "png",
             NULL, NULL, 1, 15, DEFAULT_PERIOD, TRUE);
        break;
    case MAEP_SOURCE_MAPS_FOR_FREE:
        source = _sourceNew
            ("Maps-For-Free",
             "http://maps-for-free.com/layer/relief/z#Z/row#Y/#Z_#X-#Y.jpg", "jpg",
             NULL, NULL, 1, 11, DEFAULT_PERIOD, TRUE);
        break;
    case MAEP_SOURCE_GOOGLE_STREET:
        source = _sourceNew
            ("Google Maps",
             "http://mt#R.google.com/vt/v=w2.97&x=#X&y=#Y&z=#Z", "png",
             "©2017 Google",
             "http://www.google.com/intl/fr_fr/help/legalnotices_maps.html",
             1, 17, DEFAULT_PERIOD, TRUE);
        break;
        /* http://mt0.google.com/mapstt?zoom=13&x=1406&y=3272 */
    case MAEP_SOURCE_GOOGLE_HYBRID:
        /* No longer working
           "http://mt#R.google.com/mt?n=404&v=w2t.99&x=#X&y=#Y&zoom=#S" */
        source = _sourceNew("Google Hybrid", NULL, "png", NULL, NULL,
                            1, 17, DEFAULT_PERIOD, TRUE);
        break;
    case MAEP_SOURCE_GOOGLE_SATELLITE:
        source = _sourceNew
            ("Google Satellite",
             "http://khm#R.google.com/kh/v=51&x=#X&y=#Y&z=#Z", "jpg",
             "©2017 Google",
             "http://www.google.com/intl/fr_fr/help/legalnotices_maps.html",
             1, 18, DEFAULT_PERIOD, TRUE);
        break;
    case MAEP_SOURCE_GOOGLE_TRAFFIC:
        source = _sourceNew
            ("Google traffic",
             "http://mt#R.google.com/mapstt?zoom=#Z&x=#X&y=#Y", "png",
             "©2017 Google",
             "http://www.google.com/intl/fr_fr/help/legalnotices_maps.html",
             1, 17, 60 * 10, FALSE);
        break;
    case MAEP_SOURCE_VIRTUAL_EARTH_STREET:
        source = _sourceNew
            ("Virtual Earth",
             "http://a#R.ortho.tiles.virtualearth.net/tiles/r#W.jpeg?g=50", "png",
             "©2017 Microsoft Corporation",
             "http://windows.microsoft.com:80/en-gb/windows-live/microsoft-services-agreement",
             1, 17, DEFAULT_PERIOD, TRUE);
        break;
    case MAEP_SOURCE_VIRTUAL_EARTH_SATELLITE:
        source = _sourceNew
            ("Virtual Earth Satellite",
             "http://a#R.ortho.tiles.virtualearth.net/tiles/a#W.jpeg?g=50", "jpeg",
             "©2017 Microsoft Corporation",
             "http://windows.microsoft.com:80/en-gb/windows-live/microsoft-services-agreement",
             1, 17, DEFAULT_PERIOD, TRUE);
        break;
    case MAEP_SOURCE_VIRTUAL_EARTH_HYBRID:
        source = _sourceNew
            ("Virtual Earth Hybrid",
             "http://a#R.ortho.tiles.virtualearth.net/tiles/h#W.jpeg?g=50", "jpeg",
             "©2017 Microsoft Corporation",
             "http://windows.microsoft.com:80/en-gb/windows-live/microsoft-services-agreement",
             1, 17, DEFAULT_PERIOD, TRUE);
        break;
    case MAEP_SOURCE_YAHOO_STREET:
        source = _sourceNew("Yahoo Maps", NULL, "png", NULL, NULL,
                            1, 17, DEFAULT_PERIOD, TRUE);
        break;
    case MAEP_SOURCE_YAHOO_SATELLITE:
        source = _sourceNew("Yahoo Satellite", NULL, "png", NULL, NULL,
                            1, 17, DEFAULT_PERIOD, TRUE);
        break;
    case MAEP_SOURCE_YAHOO_HYBRID:
        /* TODO: Implement signed Y, aka U
         * http://us.maps3.yimg.com/aerial.maps.yimg.com/ximg?v=1.7&t=a&s=256&x=%d&y=%-d&z=%d
         *  x = tilex,
         *  y = (1 << (MAX_ZOOM - zoom)) - tiley - 1,
         *  z = zoom - (MAX_ZOOM - 17));
         */
        source = _sourceNew("Yahoo Hybrid", NULL, "png", NULL, NULL,
                            1, 17, DEFAULT_PERIOD, TRUE);
        break;
    default:
        g_warning("Unknown source id %d.", id);
        return NULL;
    }
    source->id = id;
    g_hash_table_insert(manager->priv->sources, source->name, source);
    g_hash_table_insert(manager->priv->sourcesById, GINT_TO_POINTER(source->id), source);
    g_object_notify_by_pspec(G_OBJECT(manager), _properties[N_SOURCES_PROP]);
    return source;
}

const MaepSource* maep_source_manager_add(MaepSourceManager *manager,
                                          const gchar *name,
                                          const gchar *repo_uri,
                                          const gchar *image_suffix,
                                          const gchar *copyright_notice,
                                          const gchar *copyright_url,
                                          guint min_zoom, guint max_zoom,
                                          guint cache_period,
                                          gboolean cache_policy)
{
    MaepSource *source;

    g_return_val_if_fail(MAEP_IS_SOURCE_MANAGER(manager), NULL);

    source = _sourceNew(name, repo_uri, image_suffix, copyright_notice,
                        copyright_url, min_zoom, max_zoom, cache_period, cache_policy);
    source->id = MAEP_SOURCE_USER_DEFINED + manager->priv->userId;
    manager->priv->userId += 1;
    g_hash_table_insert(manager->priv->sources, source->name, source);
    g_hash_table_insert(manager->priv->sourcesById, GINT_TO_POINTER(source->id), source);
    g_object_notify_by_pspec(G_OBJECT(manager), _properties[N_SOURCES_PROP]);
    return source;
}

gboolean maep_source_manager_remove(MaepSourceManager *manager,
                                    MaepSource* source)
{
    g_return_val_if_fail(MAEP_IS_SOURCE_MANAGER(manager), FALSE);
    g_return_val_if_fail(source, FALSE);

    if (manager->priv->current == source)
        manager->priv->current = NULL;

    return g_hash_table_remove(manager->priv->sources, source->name);
}

const MaepSource* maep_source_manager_get(const MaepSourceManager *manager,
                                          const gchar *label)
{
    g_return_val_if_fail(MAEP_IS_SOURCE_MANAGER(manager), NULL);

    return g_hash_table_lookup(manager->priv->sources, label);
}

const MaepSource*  maep_source_manager_getById(MaepSourceManager *manager,
                                               guint id)
{
    const MaepSource *source;

    g_return_val_if_fail(MAEP_IS_SOURCE_MANAGER(manager), NULL);

    source = g_hash_table_lookup(manager->priv->sourcesById, GINT_TO_POINTER(id));
    if (!source && id > MAEP_SOURCE_NULL && id < MAEP_SOURCE_LAST)
        source = _preset(manager, id);

    return source;
}

void maep_source_manager_set_cache_dir(MaepSourceManager *manager, const gchar *dir)
{
    g_return_if_fail(MAEP_IS_SOURCE_MANAGER(manager));

    g_free(manager->priv->tile_dir);
    manager->priv->tile_dir = g_strdup(dir);
    g_object_notify_by_pspec(G_OBJECT(manager), _properties[CACHE_DIR_PROP]);
}

static void _update_current(const MaepSourceManager *manager, const MaepSource *source)
{
    g_return_val_if_fail(MAEP_IS_SOURCE_MANAGER(manager), NULL);
    g_return_val_if_fail(source, NULL);

    if (manager->priv->current == source)
        return;

    g_free(manager->priv->cache_dir);
    if (!manager->priv->tile_dir
        || g_str_has_prefix(manager->priv->tile_dir, MAEP_SOURCE_MANAGER_CACHE_NONE)) {
        manager->priv->cache_dir = NULL;
    } else if (g_str_has_prefix(manager->priv->tile_dir, MAEP_SOURCE_MANAGER_CACHE_AUTO)) {
#if GLIB_CHECK_VERSION (2, 16, 0)
        char *md5 = g_compute_checksum_for_string
            (G_CHECKSUM_MD5, maep_source_get_repo_uri(source), -1);
#else
        char *md5 = g_strdup(maep_source_get_friendly_name(source));
#endif
        manager->priv->cache_dir = g_build_filename(manager->priv->tile_dir + 7, md5, NULL);
        g_free(md5);
    } else if (g_str_has_prefix(manager->priv->tile_dir, MAEP_SOURCE_MANAGER_CACHE_FRIENDLY)) {
        manager->priv->cache_dir = g_build_filename(manager->priv->tile_dir + 11,
                                                    maep_source_get_friendly_name(source),
                                                    NULL);
    } else {
        manager->priv->cache_dir = g_build_filename(manager->priv->tile_dir,
                                                    maep_source_get_friendly_name(source),
                                                    NULL);
    }

    manager->priv->current = source;
    return;
}

static gchar* _get_tile_id(const MaepSourceManager *manager,
                           const MaepSource *source,
                           int zoom, int x, int y)
{
    g_return_val_if_fail(MAEP_IS_SOURCE_MANAGER(manager), NULL);
    g_return_val_if_fail(source, NULL);

    if (manager->priv->current != source)
        _update_current(manager, source);

    return g_strdup_printf("%s%c%d%c%d%c%d.%s",
                           manager->priv->cache_dir ? manager->priv->cache_dir : maep_source_get_friendly_name(source), G_DIR_SEPARATOR,
                           zoom, G_DIR_SEPARATOR,
                           x, G_DIR_SEPARATOR,
                           y,
                           source->image_suffix);
}

static gboolean _tile_age_exceeded(char *filename, guint period) 
{
    struct stat buf;
    
    if(!g_stat(filename, &buf)) 
        return(time(NULL) - buf.st_mtime > (gint)period);

    return FALSE;
}

static gchar* _get_cached_tile(const MaepSourceManager *manager,
                               const MaepSource *source,
                               int zoom, int x, int y)
{
    gchar *filename;

    g_return_val_if_fail(MAEP_IS_SOURCE_MANAGER(manager), NULL);
    g_return_val_if_fail(source, NULL);

    if (manager->priv->current != source)
        _update_current(manager, source);

    if (!manager->priv->cache_dir)
        return NULL;

    filename = g_strdup_printf("%s%c%d%c%d%c%d.%s",
                               manager->priv->cache_dir, G_DIR_SEPARATOR,
                               zoom, G_DIR_SEPARATOR,
                               x, G_DIR_SEPARATOR,
                               y,
                               source->image_suffix);
    if (!g_file_test(filename, G_FILE_TEST_EXISTS)) {
        g_free(filename);
        return NULL;
    }

    g_debug("Found file %s", filename);
    return filename;
}

gchar* maep_source_manager_get_cached_tile(const MaepSourceManager *manager,
                                           const MaepSource *source,
                                           int zoom, int x, int y)
{
    gchar *filename;

    g_return_val_if_fail(source, NULL);

    filename = _get_cached_tile(manager, source, zoom, x, y);
    if (!filename)
        return NULL;

    /* Allow outdated cached tiles. */
    if (source->cache_policy)
        return filename;

    if (!_tile_age_exceeded(filename, source->cache_period))
        return filename;

    g_free(filename);

    return NULL;
}

gchar* maep_source_manager_get_tile_async(MaepSourceManager *manager,
                                          const MaepSource *source,
                                          int zoom, int x, int y)
{
    gchar *filename;
    gboolean queueing;

    g_return_val_if_fail(source, NULL);

    filename = _get_cached_tile(manager, source, zoom, x, y);

    queueing = TRUE;
    if (!filename || (queueing = _tile_age_exceeded(filename, source->cache_period)))
        _download_tile(manager, source, zoom, x, y);

    if (source->cache_policy || !queueing)
        return filename;

    g_free(filename);

    return NULL;
}

GType maep_source_get_type(void)
{
    static GType g_define_type_id = 0;

    if (g_define_type_id == 0)
        g_define_type_id = g_boxed_type_register_static("MaepSource",
                                                        (GBoxedCopyFunc)_sourceRef,
                                                        (GBoxedFreeFunc)_sourceFree);
    return g_define_type_id;
}

guint maep_source_get_id(const MaepSource *source)
{
    g_return_val_if_fail(source, 0);

    return source->id;
}

const char* maep_source_get_friendly_name(const MaepSource *source)
{
    g_return_val_if_fail(source, NULL);

    return source->name;
}

const char* maep_source_get_repo_uri(const MaepSource *source)
{
    g_return_val_if_fail(source, NULL);

    return source->repo_uri;
}

const char* maep_source_get_image_suffix(const MaepSource *source)
{
    g_return_val_if_fail(source, NULL);

    return source->image_suffix;
}

void maep_source_get_repo_copyright(const MaepSource *source,
                                    const gchar **notice,
                                    const gchar **url)
{
    g_return_if_fail(source);

    if (notice)
        *notice = source->copyright_notice;
    if (url)
        *url = source->copyright_url;
}

int maep_source_get_min_zoom(const MaepSource *source)
{
    g_return_val_if_fail(source, 1);

    return source->min_zoom;
}

int maep_source_get_max_zoom(const MaepSource *source)
{
    g_return_val_if_fail(source, 1);

    return source->max_zoom;
}

gboolean maep_source_is_valid(const MaepSource *source)
{
    g_return_val_if_fail(source, FALSE);

    return source->active;
}

int maep_source_get_uri_format(const MaepSource *source)
{
    g_return_val_if_fail(source, 0);

    return source->uri_format;
}

guint maep_source_get_cache_period(const MaepSource *source)
{
    g_return_val_if_fail(source, 0);

    return source->cache_period;
}

gboolean maep_source_get_cache_policy(const MaepSource *source)
{
    g_return_val_if_fail(source, FALSE);

    return source->cache_policy;
}

static void map_convert_coords_to_quadtree_string(gint x, gint y, gint zoomlevel,
                                                  gchar *buffer, const gchar initial,
                                                  const gchar *const quadrant)
{
    gchar *ptr = buffer;
    gint n;

    if (initial)
        *ptr++ = initial;

    for(n = zoomlevel-1; n >= 0; n--)
    {
        gint xbit = (x >> n) & 1;
        gint ybit = (y >> n) & 1;
        *ptr++ = quadrant[xbit + 2 * ybit];
    }

    *ptr++ = '\0';
}

/*
 * Description:
 *   Find and replace text within a string.
 *
 * Parameters:
 *   src  (in) - pointer to source string
 *   from (in) - pointer to search text
 *   to   (in) - pointer to replacement text
 *
 * Returns:
 *   Returns a pointer to dynamically-allocated memory containing string
 *   with occurences of the text pointed to by 'from' replaced by with the
 *   text pointed to by 'to'.
 */
static gchar * replace_string(const gchar *src, const gchar *from, const gchar *to)
{
    size_t size    = strlen(src) + 1;
    size_t fromlen = strlen(from);
    size_t tolen   = strlen(to);

    /* Allocate the first chunk with enough for the original string. */
    gchar *value = g_malloc(size);


    /* We need to return 'value', so let's make a copy to mess around with. */
    gchar *dst = value;

    if ( value != NULL )
    {
        for ( ;; )
        {
            /* Try to find the search text. */
            const gchar *match = g_strstr_len(src, size, from);
            if ( match != NULL )
            {
                gchar *temp;
                /* Find out how many characters to copy up to the 'match'. */
                size_t count = match - src;


                /* Calculate the total size the string will be after the
                 * replacement is performed. */
                size += tolen - fromlen;

                temp = g_realloc(value, size);
                if ( temp == NULL )
                {
                    g_free(value);
                    return NULL;
                }

                /* we'll want to return 'value' eventually, so let's point it
                 * to the memory that we are now working with.
                 * And let's not forget to point to the right location in
                 * the destination as well. */
                dst = temp + (dst - value);
                value = temp;

                /*
                 * Copy from the source to the point where we matched. Then
                 * move the source pointer ahead by the amount we copied. And
                 * move the destination pointer ahead by the same amount.
                 */
                g_memmove(dst, src, count);
                src += count;
                dst += count;

                /* Now copy in the replacement text 'to' at the position of
                 * the match. Adjust the source pointer by the text we replaced.
                 * Adjust the destination pointer by the amount of replacement
                 * text. */
                g_memmove(dst, to, tolen);
                src += fromlen;
                dst += tolen;
            }
            else
            {
                /*
                 * Copy any remaining part of the string. This includes the null
                 * termination character.
                 */
                strcpy(dst, src);
                break;
            }
        }
    }
    return value;
}

gchar* maep_source_get_tile_uri(const MaepSource *source,
                                int zoom, int x, int y)
{
    char *url;
    unsigned int i;
    char location[22];
    const char letters[3] = {'a', 'b', 'c'};

    g_return_val_if_fail(source, NULL);

    i = 1;
    url = g_strdup(source->repo_uri);
    while (i < MAEP_SOURCE_FLAG_END) {
        char *s = NULL;
        char *old;

        old = url;
        switch(i & source->uri_format) {
        case MAEP_SOURCE_HAS_X:
            s = g_strdup_printf("%d", x);
            url = replace_string(url, URI_MARKER_X, s);
            //g_debug("FOUND " URI_MARKER_X);
            break;
        case MAEP_SOURCE_HAS_Y:
            s = g_strdup_printf("%d", y);
            url = replace_string(url, URI_MARKER_Y, s);
            //g_debug("FOUND " URI_MARKER_Y);
            break;
        case MAEP_SOURCE_HAS_Z:
            s = g_strdup_printf("%d", zoom);
            url = replace_string(url, URI_MARKER_Z, s);
            //g_debug("FOUND " URI_MARKER_Z);
            break;
        case MAEP_SOURCE_HAS_S:
            s = g_strdup_printf("%d", source->max_zoom-zoom);
            url = replace_string(url, URI_MARKER_S, s);
            //g_debug("FOUND " URI_MARKER_S);
            break;
        case MAEP_SOURCE_HAS_Q:
            map_convert_coords_to_quadtree_string(x,y,zoom,location,'t',"qrts");
            s = g_strdup_printf("%s", location);
            url = replace_string(url, URI_MARKER_Q, s);
            //g_debug("FOUND " URI_MARKER_Q);
            break;
        case MAEP_SOURCE_HAS_Q0:
            map_convert_coords_to_quadtree_string(x,y,zoom,location,'\0', "0123");
            s = g_strdup_printf("%s", location);
            url = replace_string(url, URI_MARKER_Q0, s);
            //g_debug("FOUND " URI_MARKER_Q0);
            break;
        case MAEP_SOURCE_HAS_YS:
            //              s = g_strdup_printf("%d", y);
            //              url = replace_string(url, URI_MARKER_YS, s);
            g_warning("FOUND " URI_MARKER_YS " NOT IMPLEMENTED");
            //            retval = g_strdup_printf(repo->url,
            //                    tilex,
            //                    (1 << (MAX_ZOOM - zoom)) - tiley - 1,
            //                    zoom - (MAX_ZOOM - 17));
            break;
        case MAEP_SOURCE_HAS_R:
            s = g_strdup_printf("%d", g_random_int_range(0,4));
            url = replace_string(url, URI_MARKER_R, s);
            //g_debug("FOUND " URI_MARKER_R);
            break;
        case MAEP_SOURCE_HAS_TR:
            s = g_strdup_printf("%c", letters[g_random_int_range(0,3)]);
            url = replace_string(url, URI_MARKER_T, s);
            //g_debug("FOUND " URI_MARKER_R);
            break;
        default:
            s = NULL;
            break;
        }

        if (s) {
            g_free(s);
            g_free(old);
        }

        i = (i << 1);
    }

    return url;
}

/* libsoup-2.2 and libsoup-2.4 use different ways to store the body data */
#if USE_LIBSOUP22
#define soup_message_headers_append(a,b,c) soup_message_add_header(a,b,c)
#define MSG_RESPONSE_BODY(a)    ((a)->response.body)
#define MSG_RESPONSE_LEN(a)     ((a)->response.length)
#define MSG_RESPONSE_LEN_FORMAT "%u"
#else
#define MSG_RESPONSE_BODY(a)    ((a)->response_body->data)
#define MSG_RESPONSE_LEN(a)     ((a)->response_body->length)
#define MSG_RESPONSE_LEN_FORMAT G_GOFFSET_FORMAT
#endif

typedef struct {
    /* The details of the tile to download */
    char *uri;
    char *filename;
    MaepSourceManager *manager;
} tile_download_t;

#if USE_LIBSOUP22
static void _tile_download_complete(SoupMessage *msg, gpointer user_data)
#else
static void _tile_download_complete(SoupSession *session, SoupMessage *msg, gpointer user_data)
#endif
{
    FILE *file;
    tile_download_t *dl = (tile_download_t *)user_data;

    if (SOUP_STATUS_IS_SUCCESSFUL (msg->status_code)) {
        /* save tile into cachedir if one has been specified */
        if (g_path_is_absolute(dl->filename)) {
            gchar *folder;
            folder = g_path_get_dirname(dl->filename);
            if (g_mkdir_with_parents(folder,0700) == 0) {
                file = g_fopen(dl->filename, "wb");
                if (file != NULL) {
                    fwrite (MSG_RESPONSE_BODY(msg), 1, MSG_RESPONSE_LEN(msg), file);
                    g_debug("Wrote %"MSG_RESPONSE_LEN_FORMAT" bytes to %s", MSG_RESPONSE_LEN(msg), dl->filename);
                    fclose (file);
                    g_signal_emit(G_OBJECT(dl->manager), _signals[TILE_SAVED], 0, dl->filename);
                }
            } else {
                g_warning("Error creating tile download directory: %s", folder);
                perror("perror:");
            }
            g_free(folder);
        } else {
            g_signal_emit(G_OBJECT(dl->manager), _signals[TILE_RECEIVED], 0, dl->filename,
                          MSG_RESPONSE_BODY(msg), MSG_RESPONSE_LEN(msg));
        }

        g_hash_table_remove(dl->manager->priv->tile_queue, dl->uri);

        g_free(dl->uri);
        g_free(dl->filename);
        g_free(dl);
    }
    else
    {
        g_message("Error downloading tile: %d - %s (%s)",
                  msg->status_code, msg->reason_phrase, dl->uri);
        if (msg->status_code == SOUP_STATUS_NOT_FOUND)
        {
            g_hash_table_add(dl->manager->priv->missing_tiles, dl->uri);
            g_hash_table_remove(dl->manager->priv->tile_queue, dl->uri);
            g_free(dl->filename);
            g_free(dl);
        }
        else if (msg->status_code == SOUP_STATUS_CANCELLED)
        {
            ;//application exiting
        }
        else if (msg->status_code == SOUP_STATUS_CANT_RESOLVE)
        {
            ; /* No network... */
        }
        else
        {
            g_warning("Download status %d, requeueing.", msg->status_code);
#if USE_LIBSOUP22
            soup_session_requeue_message(dl->manager->priv->soup_session, msg);
#else
            soup_session_requeue_message(session, msg);
#endif
            return;
        }
    }
}

static void _download_tile(MaepSourceManager *manager,
                           const MaepSource *source,
                           int zoom, int x, int y)
{
    SoupMessage *msg;
    tile_download_t *dl = g_new0(tile_download_t,1);

    //calculate the uri to download
    dl->uri = maep_source_get_tile_uri(source, zoom, x, y);
    /* g_message("Downloading '%s'", dl->uri); */

#if USE_LIBSOUP22
    dl->session = priv->soup_session;
#endif

    //check the tile has not already been queued for download,
    //or has been attempted, and its missing
    if (g_hash_table_contains(manager->priv->tile_queue, dl->uri) ||
        g_hash_table_contains(manager->priv->missing_tiles, dl->uri)) {
        g_debug("Tile already downloading (or missing)");
        g_free(dl->uri);
        g_free(dl);
        return;
    }

    dl->filename = _get_tile_id(manager, source, zoom, x, y);
    dl->manager = manager;

    /* g_message("Download tile: %d,%d z:%d\n\t%s --> %s", x, y, zoom, dl->uri, dl->filename); */

    msg = soup_message_new (SOUP_METHOD_GET, dl->uri);
    if (!msg) {
        g_warning("Could not create soup message");
        g_free(dl->uri);
        g_free(dl->filename);
        g_free(dl);
    }

    if (maep_source_get_uri_format(source) & MAEP_SOURCE_HAS_GOOGLE_DOMAIN) {
        //Set maps.google.com as the referrer
        g_debug("Setting Google Referrer");
        soup_message_headers_append(msg->request_headers, "Referer", "http://maps.google.com/");
        //For google satelite also set the appropriate cookie value
        if (maep_source_get_uri_format(source) & MAEP_SOURCE_HAS_Q) {
            const char *cookie = g_getenv("GOOGLE_COOKIE");
            if (cookie) {
                g_debug("Adding Google Cookie");
                soup_message_headers_append(msg->request_headers, "Cookie", cookie);
            }
        }
    }

#if USE_LIBSOUP22
    soup_message_headers_append(msg->request_headers, "User-Agent", USER_AGENT);
#endif

    g_hash_table_insert (manager->priv->tile_queue, dl->uri, msg);
    soup_session_queue_message (manager->priv->soup_session, msg,
                                _tile_download_complete, dl);
}
