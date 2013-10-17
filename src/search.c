#include "search.h"

struct _MaepSearchContextPrivate
{
  gboolean dispose_has_run;

  GSList *list;
  gboolean downloading;
};

enum {
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

  _signals[PLACES_AVAILABLE_SIGNAL] = 
    g_signal_new ("places-available", G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                  0 , NULL, NULL, g_cclosure_marshal_VOID__POINTER,
                  G_TYPE_NONE, 1, G_TYPE_POINTER);

  g_type_class_add_private(klass, sizeof(MaepSearchContextPrivate));
}
static void maep_search_context_init(MaepSearchContext *obj)
{
  g_message("New search context %p.", (gpointer)obj);
  obj->priv = G_TYPE_INSTANCE_GET_PRIVATE(obj, MAEP_TYPE_SEARCH_CONTEXT,
                                          MaepSearchContextPrivate);
  obj->priv->dispose_has_run = FALSE;
  obj->priv->downloading = FALSE;
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

  maep_geonames_place_list_free(priv->list);

  G_OBJECT_CLASS(maep_search_context_parent_class)->finalize(obj);
}

MaepSearchContext* maep_search_context_new()
{
  MaepSearchContext *search;

  search = MAEP_SEARCH_CONTEXT(g_object_new(MAEP_TYPE_SEARCH_CONTEXT, NULL));
  return search;
}

static void geonames_search_cb(MaepSearchContext *context, GSList *list, GError *error) {
  if (!error)
    {
      /* remove any list that may already be preset */
      if(context->priv->list) {
	maep_geonames_place_list_free(context->priv->list);
	context->priv->list = NULL;
      }
      
      /* render all icons */
      context->priv->list = list;
      g_signal_emit(context, _signals[PLACES_AVAILABLE_SIGNAL], 0, list, NULL);
    }
  else
    {
      g_warning("%s", error->message);
    }

  context->priv->downloading = FALSE;
  g_object_unref(context);
}

/* request geotagged wikipedia entries for current map view */
void maep_search_context_request(MaepSearchContext *context, const gchar *request) {
  /* don't have more than one pending request */
  if(context->priv->downloading)
    return;

  context->priv->downloading = TRUE;

  g_object_ref(context);
  maep_geonames_place_request(request,
                              (MaepGeonamesRequestCallback)geonames_search_cb,
                              context);
}
