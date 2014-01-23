/* -*- Mode: C; indent-tabs-mode: nil; c-basic-offset: 4; tab-width: 4 -*- */
/* vim:set et sw=4 ts=4 cino=t0,(0: */
/*
 * search.c
 * Copyright (C) Till Harbaum 2009 <till@harbaum.org>
 * Copyright (C) Damien Caliste 2013-2014 <dcaliste@free.fr>
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

#ifndef SEARCH_H
#define SEARCH_H

#include <glib-object.h>

#include "geonames.h"

G_BEGIN_DECLS

#define MAEP_TYPE_SEARCH_CONTEXT	     (maep_search_context_get_type ())
#define MAEP_SEARCH_CONTEXT(obj)	     (G_TYPE_CHECK_INSTANCE_CAST(obj, MAEP_TYPE_SEARCH_CONTEXT, MaepSearchContext))
#define MAEP_SEARCH_CONTEXT_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST(klass, MAEP_TYPE_SEARCH_CONTEXT, MaepSearchContextClass))
#define MAEP_IS_SEARCH_CONTEXT(obj)    (G_TYPE_CHECK_INSTANCE_TYPE(obj, MAEP_TYPE_SEARCH_CONTEXT))
#define MAEP_IS_SEARCH_CONTEXT_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE(klass, MAEP_TYPE_SEARCH_CONTEXT))
#define MAEP_SEARCH_CONTEXT_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS(obj, MAEP_TYPE_SEARCH_CONTEXT, MaepSearchContextClass))

typedef struct _MaepSearchContext        MaepSearchContext;
typedef struct _MaepSearchContextPrivate MaepSearchContextPrivate;
typedef struct _MaepSearchContextClass   MaepSearchContextClass;

typedef enum {
  MaepSearchContextGeonames  = 1,
  MaepSearchContextNominatim = 2
} MaepSearchContextSource;

struct _MaepSearchContext
{
  GObject parent;

  MaepSearchContextPrivate *priv;
};

struct _MaepSearchContextClass
{
  GObjectClass parent;
};

GType maep_search_context_get_type(void);

MaepSearchContext* maep_search_context_new();
void maep_search_context_request(MaepSearchContext *context, const gchar *request,
                                 guint sources);

G_END_DECLS

#endif
