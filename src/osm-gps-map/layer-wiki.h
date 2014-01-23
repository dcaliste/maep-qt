/*
 * Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
 *
 * Contributions by
 * Damien Caliste 2013-2014 <dcaliste@free.fr>
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

#ifndef LAYER_WIKI_H
#define LAYER_WIKI_H

#include <glib-object.h>
#include "osm-gps-map/osm-gps-map.h"
#include "geonames.h"

G_BEGIN_DECLS

#define MAEP_TYPE_WIKI_CONTEXT	     (maep_wiki_context_get_type ())
#define MAEP_WIKI_CONTEXT(obj)	     (G_TYPE_CHECK_INSTANCE_CAST(obj, MAEP_TYPE_WIKI_CONTEXT, MaepWikiContext))
#define MAEP_WIKI_CONTEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST(klass, MAEP_TYPE_WIKI_CONTEXT, MaepWikiContextClass))
#define MAEP_IS_WIKI_CONTEXT(obj)    (G_TYPE_CHECK_INSTANCE_TYPE(obj, MAEP_TYPE_WIKI_CONTEXT))
#define MAEP_IS_WIKI_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE(klass, MAEP_TYPE_WIKI_CONTEXT))
#define MAEP_WIKI_CONTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS(obj, MAEP_TYPE_WIKI_CONTEXT, MaepWikiContextClass))

typedef struct _MaepWikiContext        MaepWikiContext;
typedef struct _MaepWikiContextPrivate MaepWikiContextPrivate;
typedef struct _MaepWikiContextClass   MaepWikiContextClass;

struct _MaepWikiContext
{
  GObject parent;

  MaepWikiContextPrivate *priv;
};

struct _MaepWikiContextClass
{
  GObjectClass parent;
};

GType maep_wiki_context_get_type(void);

MaepWikiContext* maep_wiki_context_new(void);
void maep_wiki_context_enable(MaepWikiContext *context, OsmGpsMap *map);

G_END_DECLS

#endif
