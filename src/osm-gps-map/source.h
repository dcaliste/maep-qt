/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/*
 * Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
 *
 * Contributions by
 * Damien Caliste 2015-2017 <dcaliste@free.fr>
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

#ifndef SOURCE_H
#define SOURCE_H

#include <glib-object.h>

G_BEGIN_DECLS

/* New tiles should be appended to avoid id breakage. */
typedef enum {
    MAEP_SOURCE_NULL,
    MAEP_SOURCE_OPENSTREETMAP,
    MAEP_SOURCE_OPENSTREETMAP_RENDERER,
    MAEP_SOURCE_OPENAERIALMAP,
    MAEP_SOURCE_MAPS_FOR_FREE,
    MAEP_SOURCE_OPENCYCLEMAP,
    MAEP_SOURCE_OSM_PUBLIC_TRANSPORT,
    MAEP_SOURCE_GOOGLE_STREET,
    MAEP_SOURCE_GOOGLE_SATELLITE,
    MAEP_SOURCE_GOOGLE_HYBRID,
    MAEP_SOURCE_VIRTUAL_EARTH_STREET,
    MAEP_SOURCE_VIRTUAL_EARTH_SATELLITE,
    MAEP_SOURCE_VIRTUAL_EARTH_HYBRID,
    MAEP_SOURCE_YAHOO_STREET,
    MAEP_SOURCE_YAHOO_SATELLITE,
    MAEP_SOURCE_YAHOO_HYBRID,
    MAEP_SOURCE_OSMC_TRAILS,
    MAEP_SOURCE_OPENSEAMAP,
    MAEP_SOURCE_GOOGLE_TRAFFIC,
    MAEP_SOURCE_MML_PERUSKARTTA,
    MAEP_SOURCE_MML_ORTOKUVA,
    MAEP_SOURCE_MML_TAUSTAKARTTA,
    MAEP_SOURCE_HIKE_AND_BIKE,
    MAEP_SOURCE_HILL_SHADING,
    MAEP_SOURCE_LAST,

    MAEP_SOURCE_USER_DEFINED = 100
} MaepSourceId;

#define MAEP_SOURCE_HAS_X   (1 << 0)
#define MAEP_SOURCE_HAS_Y   (1 << 1)
#define MAEP_SOURCE_HAS_Z   (1 << 2)
#define MAEP_SOURCE_HAS_S   (1 << 3)
#define MAEP_SOURCE_HAS_Q   (1 << 4)
#define MAEP_SOURCE_HAS_Q0  (1 << 5)
#define MAEP_SOURCE_HAS_YS  (1 << 6)
#define MAEP_SOURCE_HAS_R   (1 << 7)
#define MAEP_SOURCE_HAS_GOOGLE_DOMAIN (1 << 8)
#define MAEP_SOURCE_HAS_TR  (1 << 9)
//....
#define MAEP_SOURCE_FLAG_END (1 << 10)

typedef struct _MaepSource MaepSource;

#define MAEP_TYPE_SOURCE (maep_source_get_type())

GType maep_source_get_type(void);

guint       maep_source_get_id            (const MaepSource *source);
const char* maep_source_get_friendly_name (const MaepSource *source);
const char* maep_source_get_repo_uri      (const MaepSource *source);
const char* maep_source_get_image_suffix  (const MaepSource *source);
void        maep_source_get_repo_copyright(const MaepSource *source,
                                           const gchar **notice,
                                           const gchar **url);
int         maep_source_get_min_zoom      (const MaepSource *source);
int         maep_source_get_max_zoom      (const MaepSource *source);
int         maep_source_get_uri_format    (const MaepSource *source);
guint       maep_source_get_cache_period  (const MaepSource *source);
gboolean    maep_source_get_cache_policy  (const MaepSource *source);
gboolean    maep_source_is_valid          (const MaepSource *source);
gchar*      maep_source_get_tile_uri      (const MaepSource *source,
                                           int zoom, int x, int y);

#define MAEP_SOURCE_MANAGER_CACHE_NONE  "none://"
#define MAEP_SOURCE_MANAGER_CACHE_AUTO  "auto://"
#define MAEP_SOURCE_MANAGER_CACHE_FRIENDLY  "friendly://"

#define MAEP_TYPE_SOURCE_MANAGER	     (maep_source_manager_get_type ())
#define MAEP_SOURCE_MANAGER(obj)	     (G_TYPE_CHECK_INSTANCE_CAST(obj, MAEP_TYPE_SOURCE_MANAGER, MaepSourceManager))
#define MAEP_SOURCE_MANAGER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST(klass, MAEP_TYPE_SOURCE_MANAGER, MaepSourceManagerClass))
#define MAEP_IS_SOURCE_MANAGER(obj)    (G_TYPE_CHECK_INSTANCE_TYPE(obj, MAEP_TYPE_SOURCE_MANAGER))
#define MAEP_IS_SOURCE_MANAGER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE(klass, MAEP_TYPE_SOURCE_MANAGER))
#define MAEP_SOURCE_MANAGER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS(obj, MAEP_TYPE_SOURCE_MANAGER, MaepSourceManagerClass))

typedef struct _MaepSourceManager        MaepSourceManager;
typedef struct _MaepSourceManagerPrivate MaepSourceManagerPrivate;
typedef struct _MaepSourceManagerClass   MaepSourceManagerClass;

struct _MaepSourceManager
{
  GObject parent;

  MaepSourceManagerPrivate *priv;
};

struct _MaepSourceManagerClass
{
  GObjectClass parent;
};

GType maep_source_manager_get_type(void);

MaepSourceManager* maep_source_manager_get_instance(void);

const MaepSource*  maep_source_manager_add       (MaepSourceManager *manager,
                                                  const gchar *name,
                                                  const gchar *repo_uri,
                                                  const gchar *image_suffix,
                                                  const gchar *copyright_notice,
                                                  const gchar *copyright_url,
                                                  guint min_zoom, guint max_zoom,
                                                  guint cache_period,
                                                  gboolean cache_policy);
gboolean           maep_source_manager_remove    (MaepSourceManager *manager,
                                                  MaepSource* source);

const MaepSource*  maep_source_manager_get(const MaepSourceManager *manager,
                                           const gchar *label);
const MaepSource*  maep_source_manager_getById(MaepSourceManager *manager,
                                               guint id);

void               maep_source_manager_set_cache_dir(MaepSourceManager *manager,
                                                     const gchar *dir);
gchar*             maep_source_manager_get_cached_tile(const MaepSourceManager *manager,
                                                       const MaepSource *source,
                                                       int zoom, int x, int y);
gchar*             maep_source_manager_get_tile_async(MaepSourceManager *manager,
                                                      const MaepSource *source,
                                                      int zoom, int x, int y);

/* void               maep_source_manager_set_active(MaepSourceManager *manager, */
/*                                                   MaepSource *source); */

/* void               maep_source_manager_add_settings(MaepSourceManager *manager); */
/* void               maep_source_manager_dump_settings(MaepSourceManager *manager); */

G_END_DECLS

#endif
