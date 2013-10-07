typedef struct {
  char *name, *country;
  coord_t pos;
} geonames_geoname_t;

typedef struct {
  char *title, *summary;
  char *url, *thumbnail_url;
  coord_t pos;
} geonames_entry_t;

typedef struct {
  OsmGpsMap *map;
  GSList *list;
  gulong destroy_handler_id, press_handler_id, release_handler_id;
  gulong changed_handler_id, motion_handler_id;
  gulong timer_id;
  gboolean downloading;
} wiki_context_t;

#define MAX_RESULT 25
#define GEONAMES  "http://ws.geonames.org/"
#define GEONAMES_SEARCH "geonames_search"

/* -------------- begin of xml parser ---------------- */

static gboolean string_get(xmlNode *node, char *name, char **dst) {
  if(*dst) return FALSE;   /* don't overwrite anything */

  if(strcasecmp((char*)node->name, name) != 0) 
    return FALSE;

  char *str = (char*)xmlNodeGetContent(node);
  *dst = g_strdup(str);
  xmlFree(str);

  return TRUE;
}

static geonames_geoname_t *geonames_parse_geoname(xmlDocPtr doc, xmlNode *a_node) {
  xmlNode *cur_node = NULL;
  geonames_geoname_t *geoname = g_new0(geonames_geoname_t, 1);
  geoname->pos.rlat = geoname->pos.rlon = OSM_GPS_MAP_INVALID;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {

      string_get(cur_node, "name", &geoname->name);
      string_get(cur_node, "countryName", &geoname->country);

      if(strcasecmp((char*)cur_node->name, "lat") == 0) {
	char *str = (char*)xmlNodeGetContent(cur_node);
	geoname->pos.rlat = deg2rad(g_ascii_strtod(str, NULL));
 	xmlFree(str);
      } else if(strcasecmp((char*)cur_node->name, "lng") == 0) {
	char *str = (char*)xmlNodeGetContent(cur_node);
	geoname->pos.rlon = deg2rad(g_ascii_strtod(str, NULL));
 	xmlFree(str);
      }
    }
  }

  return geoname;
}

static geonames_entry_t *geonames_parse_entry(xmlDocPtr doc, xmlNode *a_node) {
  xmlNode *cur_node = NULL;
  geonames_entry_t *entry = g_new0(geonames_entry_t, 1);
  entry->pos.rlat = entry->pos.rlon = OSM_GPS_MAP_INVALID;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {

      string_get(cur_node, "title", &entry->title);
      string_get(cur_node, "summary", &entry->summary);
      string_get(cur_node, "thumbnailImg", &entry->thumbnail_url);
      string_get(cur_node, "wikipediaUrl", &entry->url);

      if(strcasecmp((char*)cur_node->name, "lat") == 0) {
	char *str = (char*)xmlNodeGetContent(cur_node);
	entry->pos.rlat = deg2rad(g_ascii_strtod(str, NULL));
	xmlFree(str);
      } else if(strcasecmp((char*)cur_node->name, "lng") == 0) {
	char *str = (char*)xmlNodeGetContent(cur_node);
	entry->pos.rlon = deg2rad(g_ascii_strtod(str, NULL));
	xmlFree(str);
      }
    }
  }

  if(entry->thumbnail_url && strlen(entry->thumbnail_url) == 0) {
    g_free(entry->thumbnail_url);
    entry->thumbnail_url = NULL;
  }

  return entry;
}

static GSList *geonames_parse_geonames(xmlDocPtr doc, xmlNode *a_node) {
  GSList *list = NULL;
  xmlNode *cur_node = NULL;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "geoname") == 0) {
	list = g_slist_append(list, geonames_parse_geoname(doc, cur_node));
      } else if(strcasecmp((char*)cur_node->name, "entry") == 0) {
	list = g_slist_append(list, geonames_parse_entry(doc, cur_node));
      }
    }
  }
  return list;
}

/* parse root element and search for "track" */
static GSList *geonames_parse_root(xmlDocPtr doc, xmlNode *a_node) {
  GSList *list = NULL;
  xmlNode *cur_node = NULL;

  for (cur_node = a_node; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(!list && strcasecmp((char*)cur_node->name, "geonames") == 0) 
	list = geonames_parse_geonames(doc, cur_node);
    }
  }
  return list;
}

static GSList *geonames_parse_doc(xmlDocPtr doc) {
  /* Get the root element node */
  xmlNode *root_element = xmlDocGetRootElement(doc);

  GSList *list = geonames_parse_root(doc, root_element);  

  xmlFreeDoc(doc);

  xmlCleanupParser();

  return list;
}

/* -------------- end of xml parser ---------------- */

/* ------------- begin of freeing ------------------ */

static void geonames_geoname_free(gpointer data, gpointer user_data) {
  geonames_geoname_t *geoname = (geonames_geoname_t*)data;
  if(geoname->name) g_free(geoname->name);
  if(geoname->country) g_free(geoname->country);
}

static void geonames_geoname_list_free(GSList *list) {
  g_slist_foreach(list, geonames_geoname_free, NULL);
  g_slist_free(list);
}

static void geonames_entry_free(gpointer data, gpointer user_data) {
  geonames_entry_t *entry = (geonames_entry_t*)data;
  if(entry->title) g_free(entry->title);
  if(entry->summary) g_free(entry->summary);
  if(entry->thumbnail_url) g_free(entry->thumbnail_url);
  if(entry->url) g_free(entry->url);
}

static void geonames_entry_list_free(GSList *list) {
  g_slist_foreach(list, geonames_entry_free, NULL);
  g_slist_free(list);
}

/* ------------- end of freeing ------------------ */

static void wiki_context_free(wiki_context_t *context, gboolean destroy) {
  g_assert(context);

  g_return_if_fail(!context->downloading);

  if(!destroy) {
    osm_gps_map_clear_images (context->map);
    osm_gps_map_osd_clear_balloon (context->osd);
  }

  /* if a list of entries is present, then remove it */
  if(context->list) {
    g_object_set_data(G_OBJECT(context->map), "balloon", NULL);
    g_object_set_data(G_OBJECT(context->map), "nearest", NULL);
    geonames_entry_list_free(context->list);
    context->list = NULL;
  }

  /* also remove the destroy handler for the context */
  if(context->destroy_handler_id) {
    g_signal_handler_disconnect(G_OBJECT(context->widget), context->destroy_handler_id);
    context->destroy_handler_id = 0;
  }

  /* and the button press handler */
  if(context->press_handler_id) {
    g_signal_handler_disconnect(G_OBJECT(context->widget), context->press_handler_id);
    context->press_handler_id = 0;
  }

  /* and the button release handler */
  if(context->release_handler_id) {
    g_signal_handler_disconnect(G_OBJECT(context->widget), context->release_handler_id);
    context->release_handler_id = 0;
  }

  if(context->changed_handler_id) {
    g_signal_handler_disconnect(G_OBJECT(context->map), context->changed_handler_id);
    context->changed_handler_id = 0;
  }

  if(context->motion_handler_id) {
    g_signal_handler_disconnect(G_OBJECT(context->widget), context->motion_handler_id);
    context->motion_handler_id = 0;
  }

  if(context->timer_id) {
    gtk_timeout_remove(context->timer_id);
    context->timer_id = 0;
  }

  g_object_set_data(G_OBJECT(context->map), "wikipedia", NULL);

  /* finally free the context itself */
  g_free(context);
}

static void geonames_entry_render(gpointer data, gpointer user_data) {
  geonames_entry_t *entry = (geonames_entry_t*)data;
  OsmGpsMap *map = OSM_GPS_MAP(user_data);

  cairo_surface_t *cr_surf = icon_get_surface(G_OBJECT(map),
                                              "wikipedia_w." ICON_SIZE_STR);

  /* g_error("to be implemented"); */
  if(entry->url && cr_surf && !isnan(entry->pos.rlat) && !isnan(entry->pos.rlon))
    osm_gps_map_add_image_with_alignment(OSM_GPS_MAP(map),
        	 rad2deg(entry->pos.rlat), rad2deg(entry->pos.rlon), cr_surf,
        	 0.5, 0.5);
}

void geonames_wiki_cb(net_result_t *result, gpointer data) {
  wiki_context_t *context = (wiki_context_t*)g_object_get_data(G_OBJECT(data), "wikipedia");

  g_message("asynchronous callback.");
  /* context may be gone by now as the user may have disabled */
  /* the wiki service in the meantime */
  if(!context) return;

  if(!result->code) {
    /* feed this into the xml parser */
    xmlDoc *doc = NULL;

    LIBXML_TEST_VERSION;
  
    /* parse the file and get the DOM */
    if((doc = xmlReadMemory(result->data.ptr, result->data.len, 
			    NULL, NULL, 0)) == NULL) {
      xmlErrorPtr errP = xmlGetLastError();
      printf("While parsing:\n\n%s\n", errP->message);
    } else {
      GSList *list = geonames_parse_doc(doc); 
      
      g_message("got %d results", g_slist_length(list));

      /* remove any list that may already be preset */
      if(context->list) {
	osm_gps_map_osd_clear_balloon (context->osd);
	g_object_set_data(G_OBJECT(context->map), "balloon", NULL);
	g_object_set_data(G_OBJECT(context->map), "nearest", NULL);
	
	geonames_entry_list_free(context->list);
	context->list = NULL;
	osm_gps_map_clear_images(context->map);
      }
      
      /* render all icons */
      context->list = list;
      g_slist_foreach(list, geonames_entry_render, context->map);
    }
  }

  context->downloading = FALSE;
}

/* request geotagged wikipedia entries for current map view */
static void geonames_wiki_request(wiki_context_t *context) {
  /* don't have more than one pending request */
  if(context->downloading)
    return;

  context->downloading = TRUE;

  /* request area from map */
  coord_t pt1, pt2;
  osm_gps_map_get_bbox (OSM_GPS_MAP(context->map), &pt1, &pt2);

  /* create ascii (dot decimal point) strings */
  char str[4][16];
  g_ascii_formatd(str[0], sizeof(str[0]), "%.07f", rad2deg(pt1.rlat));
  g_ascii_formatd(str[1], sizeof(str[1]), "%.07f", rad2deg(pt2.rlat));
  g_ascii_formatd(str[2], sizeof(str[2]), "%.07f", rad2deg(pt1.rlon));
  g_ascii_formatd(str[3], sizeof(str[3]), "%.07f", rad2deg(pt2.rlon));

  gchar *locale, lang[3] = { 0,0,0 };
  locale = setlocale (LC_MESSAGES, NULL);
  g_utf8_strncpy (lang, locale, 2);

  /* currently only "de" and "en" are supported by geonames.org */
  /* force to "en" in any other case */
  if(strcasecmp(lang, "de") && strcasecmp(lang, "en")) 
    strncpy(lang, "en", 2);
      
  /* build complete url for request */
  char *url = g_strdup_printf(
      GEONAMES "wikipediaBoundingBox?"
      "north=%s&south=%s&west=%s&east=%s&lang=%s&maxRows=%u", 
      str[0], str[1], str[2], str[3], lang, MAX_RESULT);

  /* start download in background */
  g_message("start asynchronous download.");
  net_io_download_async(url, geonames_wiki_cb, context->map);

  g_free(url);
}


/* the wikimedia icons are automagically updated after one second */
/* of idle time */
wiki_context_t* geonames_enable_wikipedia(GtkWidget *widget, OsmGpsMap *map, gboolean enable)
{
  wiki_context_t *context = NULL;

  context = (wiki_context_t *)
    g_object_get_data(G_OBJECT(map), "wikipedia");

  if(enable) {    
    /* there must be no context yet */
    g_assert(!context);
    
    /* create and register a context */
    context = g_new0(wiki_context_t, 1);
    g_object_set_data(G_OBJECT(map), "wikipedia", context);
    context->widget = OSM_GPS_MAP_GTK(widget);
    context->map = map;
    context->osd = osm_gps_map_gtk_get_osd(context->widget);

    /* attach to maps changed event, so we can update the wiki */
    /* entries whenever the map changes */

    /* trigger wiki data update either by "changed" events ... */
    context->changed_handler_id =
      g_signal_connect(G_OBJECT(map), "changed",
		       G_CALLBACK(on_map_changed), context);

    /* ... or by the user dragging the map */
    context->motion_handler_id =
      g_signal_connect(G_OBJECT(widget), "motion-notify-event",
		       G_CALLBACK(on_map_motion), context);

    /* clean up when map is destroyed */
    context->destroy_handler_id = 
      g_signal_connect(G_OBJECT(widget), "destroy", 
		       G_CALLBACK(on_map_destroy), context);

    /* finally install handlers to handle clicks onto the wiki labels */
    context->press_handler_id =
      g_signal_connect(G_OBJECT(widget), "button-press-event",
		       G_CALLBACK(on_map_button_press_event), context);

    context->release_handler_id =
      g_signal_connect(G_OBJECT(widget), "button-release-event",
		       G_CALLBACK(on_map_button_press_event), context);

    /* only do request if map has a reasonable size. otherwise map may */
    /* still be in setup phase */
    if(widget->allocation.width > 1 && widget->allocation.height > 1)
      wiki_update(map);

  } else if (context)
    wiki_context_free(context, FALSE);

  gconf_set_bool("wikipedia", enable);
}
