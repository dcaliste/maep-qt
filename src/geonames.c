/*
 * Copyright (C) 2009 Till Harbaum <till@harbaum.org>.
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

/*
 * TODO:
 */

#include "config.h"
#include "net_io.h"
#include "geonames.h"
#include "misc.h"
#include "menu.h"
#include "icon.h"
#include "osm-gps-map.h"
#include "osm-gps-map-osd-classic.h"
#include "converter.h"

#include <math.h> // for isnan()
#include <libxml/parser.h>
#include <libxml/tree.h>

#ifdef MAEMO5
#include <hildon/hildon-pannable-area.h>
#define ICON_SIZE_STR "48"
#define ICON_SIZE      48
#else
#ifdef USE_MAEMO
#define ICON_SIZE_STR "32"
#define ICON_SIZE      32
#else
#define ICON_SIZE_STR "24"
#define ICON_SIZE      24
#endif
#endif

#ifdef USE_MAEMO
#include <hildon/hildon-banner.h>
#endif

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

#include <string.h>

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

enum {
  GEONAMES_PICKER_COL_NAME = 0,
  GEONAMES_PICKER_COL_COUNTRY,
  GEONAMES_PICKER_COL_DATA,
  GEONAMES_PICKER_NUM_COLS
};

#ifndef MAEMO5
static void on_geonames_picker_activated(GtkTreeView        *treeview,
					 GtkTreePath        *path,
					 GtkTreeViewColumn  *col,
					 gpointer            userdata) 
#else
static void on_geonames_picker_row_tapped(GtkTreeView *treeview,
					  GtkTreePath *path,
					  gpointer     userdata) 
#endif
{
  GtkTreeIter   iter;
  GtkTreeModel *model = gtk_tree_view_get_model(treeview);

  if(gtk_tree_model_get_iter(model, &iter, path)) {
    geonames_geoname_t *geoname = NULL;
    gtk_tree_model_get(model, &iter, 
		       GEONAMES_PICKER_COL_DATA, &geoname, 
		       -1);

    osm_gps_map_set_center(OSM_GPS_MAP(userdata), 
	   rad2deg(geoname->pos.rlat), rad2deg(geoname->pos.rlon));


    gtk_dialog_response(GTK_DIALOG(gtk_widget_get_toplevel(
		   GTK_WIDGET(treeview))), GTK_RESPONSE_ACCEPT);
  }
}

static GtkWidget *geonames_picker_create(GSList *list, gpointer user_data) {
  GtkCellRenderer *renderer;
  GtkListStore    *store;
  GtkTreeIter     iter;

  GtkWidget *view = gtk_tree_view_new();
  
  /* --- "Name" column --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL );
  GtkTreeViewColumn *column = gtk_tree_view_column_new_with_attributes(
		 "Name", renderer, "text", GEONAMES_PICKER_COL_NAME, NULL);
  gtk_tree_view_column_set_expand(column, TRUE);
  gtk_tree_view_insert_column(GTK_TREE_VIEW(view), column, -1);

  /* --- "Country" column --- */
  renderer = gtk_cell_renderer_text_new();
  g_object_set(renderer, "ellipsize", PANGO_ELLIPSIZE_END, NULL );
  column = gtk_tree_view_column_new_with_attributes(
	       "Country", renderer, "text", GEONAMES_PICKER_COL_COUNTRY, NULL);
  gtk_tree_view_column_set_expand(column, TRUE);
  gtk_tree_view_insert_column(GTK_TREE_VIEW(view), column, -1);

  store = gtk_list_store_new(GEONAMES_PICKER_NUM_COLS, 
			     G_TYPE_STRING,
			     G_TYPE_STRING,
			     G_TYPE_POINTER);
  
  while(list) {
    geonames_geoname_t *geoname = (geonames_geoname_t*)list->data;

    /* Append a row and fill in some data */
    gtk_list_store_append (store, &iter);
  
    gtk_list_store_set(store, &iter,
		       GEONAMES_PICKER_COL_NAME,    geoname->name,
		       GEONAMES_PICKER_COL_COUNTRY, geoname->country,
		       GEONAMES_PICKER_COL_DATA,    geoname,
		       -1);
    
    list = g_slist_next(list);
  }

  gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(store));
  g_object_unref(store);

  /* make list react on clicks */
#ifndef MAEMO5
  g_signal_connect(view, "row-activated", 
		   (GCallback)on_geonames_picker_activated, user_data);
#else
  g_signal_connect(view, "hildon-row-tapped", 
		   (GCallback)on_geonames_picker_row_tapped, user_data);
#endif

  /* put this inside a scrolled view */
#ifndef MAEMO5
  GtkWidget *scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), 
				 GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
  gtk_container_add(GTK_CONTAINER(scrolled_window), view);
  return scrolled_window;
#else
  GtkWidget *pannable_area = hildon_pannable_area_new();
  gtk_container_add(GTK_CONTAINER(pannable_area), view);
  return pannable_area;
#endif
}

static void 
geonames_display_search_results(GtkWidget *parent, 
				GtkWidget *map, GSList *list) {

  GtkWidget *dialog = 
    gtk_dialog_new_with_buttons(_("Search results"),
		GTK_WINDOW(parent), GTK_DIALOG_MODAL,
		GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, 
		NULL);

  gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 256);
  
  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox),
			      geonames_picker_create(list, map));

  gtk_widget_show_all(dialog);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
}

static void
geonames_cb(net_result_t *result, gpointer data) {
  GtkWidget *button = GTK_WIDGET(data);
  GtkWidget *toplevel = gtk_widget_get_toplevel(button);

  net_io_t io = g_object_get_data(G_OBJECT(button), "search_io");

  /* if io is gone, then the search has been cancelled */
  if(!io) return;

  /* stop animation */
  gulong handler_id = 
    (gulong)g_object_get_data(G_OBJECT(button), "handler_id");

  gtk_timeout_remove(handler_id);

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

      if(g_slist_length(list)) {
	GtkWidget *map = g_object_get_data(G_OBJECT(button), "map");
	geonames_display_search_results(toplevel, map, list);
      } else
	errorf(toplevel, _("No places matching the search term "
			   "could be found."));

      geonames_geoname_list_free(list);
    }
  }
#ifdef USE_MAEMO
  else {
    /* result code is != 0 -> error */
    hildon_banner_show_information(toplevel, NULL, 
	   _("Geonames download failed!"));
  }
#endif

  /* re-enable ui */
  gtk_widget_set_sensitive(button, TRUE);
  GtkWidget *entry = g_object_get_data(G_OBJECT(button), "search_entry");
  gtk_widget_set_sensitive(entry, TRUE);

  g_object_set_data(G_OBJECT(button), "search_io", NULL);
}

/* this can be used for some animation during search */
static gboolean search_busy(gpointer data) {
  return TRUE;
}

static void on_search_clicked(GtkButton *button, gpointer user) {
  GtkWidget *entry = g_object_get_data(G_OBJECT(button), "search_entry");
  g_assert(entry);

  char *phrase = (char*)gtk_entry_get_text(GTK_ENTRY(entry));
  gconf_set_string("search_text", phrase);

  /* disable ui while search is running */
  gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
  gtk_widget_set_sensitive(entry, FALSE);

  /* start animation */
  gulong handler_id = 
    gtk_timeout_add(100, search_busy, entry);  

  g_object_set_data(G_OBJECT(button), "handler_id", (gpointer)handler_id);

  gchar *locale, lang[3] = { 0,0,0 };
  locale = setlocale (LC_MESSAGES, NULL);
  g_utf8_strncpy (lang, locale, 2);

  /* build search request */
  char *encoded_phrase = url_encode(phrase);
  char *url = g_strdup_printf(
	      GEONAMES "search?q=%s&maxRows=%u&lang=%s"
	      "&isNameRequired=1&featureClass=P",

	      encoded_phrase, MAX_RESULT, lang);
  g_free(encoded_phrase);

  /* request search results asynchronously */
  net_io_t io = net_io_download_async(url, geonames_cb, button);
  g_free(url);

  g_object_set_data(G_OBJECT(button), "search_io", io);
}

/* this cancels a search in progress and removes the box otherwise */
static void on_search_close_clicked(GtkButton *close_but, gpointer user) {
  GtkWidget *map = GTK_WIDGET(user);
  GtkWidget *toplevel = gtk_widget_get_toplevel(map);

  GtkWidget *hbox = g_object_get_data(G_OBJECT(toplevel), GEONAMES_SEARCH);
  g_assert(hbox);
  GtkWidget *button = g_object_get_data(G_OBJECT(close_but), "search_button");
  g_assert(button);

  /* cancel search basically means to ignore the result */
  net_io_t io = g_object_get_data(G_OBJECT(button), "search_io");

  /* if search button is disabled, then the search is running */
  if(io) {
    net_io_cancel_async(io);
    g_object_set_data(G_OBJECT(button), "search_io", NULL);

    /* stop animation */
    gulong handler_id = 
      (gulong)g_object_get_data(G_OBJECT(button), "handler_id");
    
    g_assert(handler_id);
    gtk_timeout_remove(handler_id);
    
    /* re-enable ui */
    gtk_widget_set_sensitive(button, TRUE);
    GtkWidget *entry = g_object_get_data(G_OBJECT(button), "search_entry");
    gtk_widget_set_sensitive(entry, TRUE);

  } else {
    menu_enable(toplevel, "Search", TRUE); 
    g_object_set_data(G_OBJECT(toplevel), GEONAMES_SEARCH, NULL);
    gtk_widget_destroy(hbox);

    /* give entry keyboard focus back to map */
    gtk_widget_grab_focus(map);
  }
}

/* enable "search" button once the user has entered something */
static void on_entry_changed(GtkWidget *widget, gpointer data) {
  char *phrase = (char*)gtk_entry_get_text(GTK_ENTRY(widget));
  GtkWidget *button = g_object_get_data(G_OBJECT(widget), "search_button");
  g_assert(button);

  gboolean sensitive = GTK_WIDGET_FLAGS(button) & GTK_SENSITIVE;
  gboolean phrase_ok = phrase && strlen(phrase);

  /* need to change button state? */
  if((phrase_ok && !sensitive) || (!phrase_ok && sensitive))
    gtk_widget_set_sensitive(GTK_WIDGET(button), phrase_ok);
}

static void on_entry_activate(GtkWidget *widget, gpointer data) {
  GtkWidget *button = g_object_get_data(G_OBJECT(widget), "search_button");
  char *phrase = (char*)gtk_entry_get_text(GTK_ENTRY(widget));
  if(phrase && strlen(phrase))
    on_search_clicked(GTK_BUTTON(button), data);
}

void geonames_enable_search(GtkWidget *map) {
  GtkWidget *toplevel = gtk_widget_get_toplevel(map);
  GtkWidget *hbox = g_object_get_data(G_OBJECT(toplevel), GEONAMES_SEARCH);
  g_assert(!hbox);

  menu_enable(toplevel, "Search", FALSE); 

  /* no widget present -> build and install one */
  hbox = gtk_hbox_new(FALSE, 0);

  /* create search entry field */
  GtkWidget *entry = entry_new();
  g_signal_connect(G_OBJECT(entry), "changed",
		   G_CALLBACK(on_entry_changed), NULL);
  g_signal_connect(G_OBJECT(entry), "activate",
		   G_CALLBACK(on_entry_activate), map);
  gtk_box_pack_start_defaults(GTK_BOX(hbox), entry);

  /* create search button */
  GtkWidget *button = button_new_with_label(_("Search"));
  gtk_widget_set_sensitive(GTK_WIDGET(button), FALSE);
  g_signal_connect_after(button, "clicked", 
			 G_CALLBACK(on_search_clicked), map);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);

  /* make button and entry know each other */
  g_object_set_data(G_OBJECT(entry), "search_button", button);
  g_object_set_data(G_OBJECT(button), "search_entry", entry);
  g_object_set_data(G_OBJECT(button), "map", map);
  
  /* create close button */
  GtkWidget *close_but = button_new();
  g_object_set_data(G_OBJECT(close_but), "search_button", button);
  g_signal_connect_after(close_but, "clicked", 
			 G_CALLBACK(on_search_close_clicked), map);
  GtkWidget *image = 
    gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image(GTK_BUTTON(close_but), image);
  gtk_box_pack_start(GTK_BOX(hbox), close_but, FALSE, FALSE, 0);
  
  /* the parent is a container with a vbox being its child */
  GList *children = gtk_container_get_children(GTK_CONTAINER(toplevel));
  g_assert(g_list_length(children) == 1);
  
  gtk_widget_show_all(hbox);
  gtk_box_pack_start(GTK_BOX(children->data), hbox, FALSE, FALSE, 0);

  /* try to load preset */
  char *preset = gconf_get_string("search_text");
  if(preset) gtk_entry_set_text(GTK_ENTRY(entry), preset);

  /* give entry keyboard focus */
  gtk_widget_grab_focus(entry);
  
  g_object_set_data(G_OBJECT(toplevel), GEONAMES_SEARCH, hbox);
}

static void wiki_context_free(GtkWidget *widget, gboolean destroy) {
  wiki_context_t *context = 
    (wiki_context_t *)g_object_get_data(G_OBJECT(widget), "wikipedia");

  g_assert(context);

  if(!destroy) {
    osm_gps_map_clear_images(OSM_GPS_MAP(widget));
    osm_gps_map_osd_clear_balloon (OSM_GPS_MAP(widget));
  }

  /* if a list of entries is present, then remove it */
  if(context->list) {
    g_object_set_data(G_OBJECT(widget), "balloon", NULL);
    g_object_set_data(G_OBJECT(widget), "nearest", NULL);
    geonames_entry_list_free(context->list);
    context->list = NULL;
  }

  /* also remove the destroy handler for the context */
  if(context->destroy_handler_id) {
    g_signal_handler_disconnect(G_OBJECT(widget), context->destroy_handler_id);
    context->destroy_handler_id = 0;
  }

  /* and the button press handler */
  if(context->press_handler_id) {
    g_signal_handler_disconnect(G_OBJECT(widget), context->press_handler_id);
    context->press_handler_id = 0;
  }

  /* and the button release handler */
  if(context->release_handler_id) {
    g_signal_handler_disconnect(G_OBJECT(widget), context->release_handler_id);
    context->release_handler_id = 0;
  }

  if(context->changed_handler_id) {
    g_signal_handler_disconnect(G_OBJECT(widget), context->changed_handler_id);
    context->changed_handler_id = 0;
  }

  if(context->motion_handler_id) {
    g_signal_handler_disconnect(G_OBJECT(widget), context->motion_handler_id);
    context->motion_handler_id = 0;
  }

  if(context->timer_id) {
    gtk_timeout_remove(context->timer_id);
    context->timer_id = 0;
  }

  /* finally free the context itself */
  g_free(context);
  g_object_set_data(G_OBJECT(widget), "wikipedia", NULL);
}

static void on_map_destroy (GtkWidget *widget, gpointer data) {
  wiki_context_free(widget, TRUE);
}

static void geonames_entry_render(gpointer data, gpointer user_data) {
  geonames_entry_t *entry = (geonames_entry_t*)data;
  GtkWidget *map = GTK_WIDGET(user_data);

  GdkPixbuf *pix = icon_get_pixbuf(map, "wikipedia_w." ICON_SIZE_STR);

  if(entry->url && pix && !isnan(entry->pos.rlat) && !isnan(entry->pos.rlon)) 
    osm_gps_map_add_image_with_alignment(OSM_GPS_MAP(map), 
		 rad2deg(entry->pos.rlat), rad2deg(entry->pos.rlon), pix,
		 0.5, 0.5);
}

/* return distance between both points */
static float get_distance(coord_t *c0, coord_t *c1) {
  float aob = acos(cos(c0->rlat) * cos(c1->rlat) * cos(c1->rlon - c0->rlon) +
		   sin(c0->rlat) * sin(c1->rlat));

  return(aob * 6371000.0);     /* great circle radius in meters */
}

static int dist2pixel(GtkWidget *map, float m) {
  return m/osm_gps_map_get_scale(OSM_GPS_MAP(map));
}

void on_title_clicked(GtkButton *button, gpointer user_data) {
  GtkWidget *parent = gtk_widget_get_toplevel(GTK_WIDGET(button));
#ifdef MAEMO5
  GtkWidget *root = g_object_get_data(G_OBJECT(parent), "root");
#else
  GtkWidget *root = 
    GTK_WIDGET(gtk_window_get_transient_for(GTK_WINDOW(parent)));
#endif
  browser_url(root, user_data);
}

void thumbnail_cb(net_result_t *result, gpointer data) {
  if(!result->code) {
    GtkWidget *eventbox = GTK_WIDGET(data);

    GdkPixbufLoader *loader;
    if(!(loader = gdk_pixbuf_loader_new())) {
      g_warning("Error: Unable to create loader");
      return;
    }

    if (!gdk_pixbuf_loader_write(loader, (guchar*)result->data.ptr, 
				 result->data.len, NULL)) {
      g_warning("Error: Decoding of icon failed");
      g_object_unref(loader);
      return;
    }
    
    gdk_pixbuf_loader_close(loader, NULL);
    
    GdkPixbuf *pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
    
    /* give up loader but keep the pixbuf */
    g_object_ref(pixbuf);
    g_object_unref(loader);

    /* make sure pixbif gets freed if box is destroyed */
    icon_register_pixbuf(eventbox, NULL, pixbuf);

    GtkWidget *icon = gtk_bin_get_child(GTK_BIN(eventbox));
    gtk_widget_destroy(icon);

    /* make widget from pixbuf */
    icon = gtk_image_new_from_pixbuf(pixbuf);
    gtk_container_add(GTK_CONTAINER(eventbox), icon);
    gtk_widget_show_all(icon);
  }
}

/* dialog/window to display wiki entry details */
static void wikipedia_details(GtkWidget *map, geonames_entry_t *entry) {
  GtkWidget *parent = gtk_widget_get_toplevel(map);
  GtkWidget *vbox = gtk_vbox_new(FALSE, 10);

#ifdef MAEMO5
  GtkWidget *window = hildon_stackable_window_new();
  g_object_set_data(G_OBJECT(window), "root", parent);
#else
  GtkWidget *dialog = 
    gtk_dialog_new_with_buttons(_("Wikipedia"),
		GTK_WINDOW(parent), GTK_DIALOG_MODAL,
		GTK_STOCK_CANCEL, GTK_RESPONSE_REJECT, 
		NULL);

  gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 400);
#endif

  /* */
  gtk_box_pack_start(GTK_BOX(vbox), label_big_new(entry->title), 
		     FALSE, FALSE, 0);

  if(entry->url) {
    GtkWidget *hbox = gtk_hbox_new(FALSE, 0);
    GtkWidget *button = button_new_with_label(_("Open in browser"));
    g_signal_connect(button, "clicked", 
	     G_CALLBACK(on_title_clicked), entry->url);

    gtk_box_pack_start(GTK_BOX(hbox), button, TRUE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  }

  if(entry->thumbnail_url) {
    GtkWidget *eventbox = gtk_event_box_new();
    GtkWidget *icon = icon_get_widget(eventbox, "wikipedia.64");

    /* start download of real icon */
    net_io_download_async(entry->thumbnail_url, thumbnail_cb, eventbox);

    gtk_container_add(GTK_CONTAINER(eventbox), icon);
    gtk_box_pack_start(GTK_BOX(vbox), eventbox, FALSE, FALSE, 0);
  }

  if(entry->summary) {
    GtkWidget *label = label_wrap_new(entry->summary);

    /* scrolling text window */
    /* put this inside a scrolled view */
    GtkWidget *scrolled_window = 
      scrolled_window_new(GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    scrolled_window_add_with_viewport(scrolled_window, label);
    gtk_box_pack_start_defaults(GTK_BOX(vbox), scrolled_window);
  }

#ifdef MAEMO5
  gtk_container_add(GTK_CONTAINER(window), vbox);
  gtk_widget_show_all(window);
#else
  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), vbox);
  gtk_widget_show_all(dialog);
  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);
#endif
}

#ifndef USE_MAEMO
#define BALLOON_FONT 12.0
#else
#ifndef MAEMO5
#define BALLOON_FONT 20.0
#else
#define BALLOON_FONT 32.0
#endif
#endif

#define BORDER (BALLOON_FONT/4)

static void balloon_cb(osm_gps_map_balloon_event_t *event, gpointer data) {
  GtkWidget *map = GTK_WIDGET(data);
  geonames_entry_t *entry = 
    g_object_get_data(G_OBJECT(map), "balloon");

  if(event->type == OSM_GPS_MAP_BALLOON_EVENT_TYPE_SIZE_REQUEST) {
    cairo_text_extents_t extents;

    /* we need to figure out the extents in order to determine */
    /* the surface size this in turn requires a valid surface ... */
    cairo_select_font_face (event->data.draw.cr, "Sans",
			    CAIRO_FONT_SLANT_NORMAL,
			    CAIRO_FONT_WEIGHT_BOLD);
      
    cairo_set_font_size (event->data.draw.cr, BALLOON_FONT);
    cairo_text_extents (event->data.draw.cr, entry->title, &extents);

    event->data.draw.rect->w = extents.width + 2*BORDER;
    event->data.draw.rect->h = extents.height + 2*BORDER;

  } else if(event->type == OSM_GPS_MAP_BALLOON_EVENT_TYPE_DRAW) {
    cairo_text_extents_t extents;

    cairo_select_font_face (event->data.draw.cr, "Sans",
			    CAIRO_FONT_SLANT_NORMAL,
			    CAIRO_FONT_WEIGHT_BOLD);
      
    cairo_set_font_size (event->data.draw.cr, BALLOON_FONT);
    cairo_text_extents (event->data.draw.cr, entry->title, &extents);

    cairo_move_to (event->data.draw.cr, 
		   event->data.draw.rect->x + BORDER - extents.x_bearing, 
		   event->data.draw.rect->y + BORDER - extents.y_bearing);

    cairo_set_source_rgb (event->data.draw.cr, 0, 0, 0);
    cairo_show_text (event->data.draw.cr, entry->title);
    cairo_stroke (event->data.draw.cr);
  } else if(event->type == OSM_GPS_MAP_BALLOON_EVENT_TYPE_CLICK) {
    g_object_set_data(G_OBJECT(map), "balloon", NULL);
    wikipedia_details(map, entry);
    osm_gps_map_osd_clear_balloon (OSM_GPS_MAP(map));
  }
}

static gboolean
on_map_button_press_event(GtkWidget *widget, 
			  GdkEventButton *event, gpointer user_data) {
  static geonames_entry_t *nearest = NULL;
  
  wiki_context_t *context = 
    (wiki_context_t *)g_object_get_data(G_OBJECT(widget), "wikipedia");

  /* check if we actually clicked parts of the OSD */
  if(osm_gps_map_osd_check(OSM_GPS_MAP(widget), event->x, event->y) 
     != OSD_NONE) 
    return FALSE;

  if(event->type == GDK_BUTTON_PRESS) {

    /* the center of the click is in the middle of the icon which in turn */
    /* is ICON_SIZE/2+ICON_SIZE/4 (+4 because of the pin at the icon bottom) */
    /* above the actual object position. */
    coord_t coord = 
      osm_gps_map_get_co_ordinates(OSM_GPS_MAP(widget), event->x, event->y);

    /* find closest wikipoint */
    GSList *list = context->list;
    float nearest_distance = 1000000000.0; 
    nearest = NULL;
    while(list) {
      geonames_entry_t *entry = (geonames_entry_t*)list->data;

      float dst = get_distance(&entry->pos, &coord);
      if(dst < nearest_distance) {
	nearest = entry;
	nearest_distance = dst;
      }

      list = g_slist_next(list);
    }

    /* check if click was close enough */
    if(nearest && dist2pixel(widget, nearest_distance) >= ICON_SIZE/2)
      nearest = NULL;
    
    g_object_set_data(G_OBJECT(widget), "nearest", nearest);

  } else if(event->type == GDK_BUTTON_RELEASE) {
    /* a "nearest" is still valid from the click */
    nearest = g_object_get_data(G_OBJECT(widget), "nearest");
    if(nearest) {
      g_object_set_data(G_OBJECT(widget), "balloon", nearest);
      osm_gps_map_osd_draw_balloon (OSM_GPS_MAP(widget), 
		    rad2deg(nearest->pos.rlat), rad2deg(nearest->pos.rlon),
		    balloon_cb, widget);
    }
  }

  return FALSE;
}

void geonames_wiki_cb(net_result_t *result, gpointer data) {
  GtkWidget *map = GTK_WIDGET(data);
  wiki_context_t *context = 
    (wiki_context_t *)g_object_get_data(G_OBJECT(map), "wikipedia");

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
      
      printf("got %d results\n", g_slist_length(list));

      /* remove any list that may already be preset */
      if(context->list) {
	osm_gps_map_osd_clear_balloon (OSM_GPS_MAP(map));
	g_object_set_data(G_OBJECT(map), "balloon", NULL);
	g_object_set_data(G_OBJECT(map), "nearest", NULL);
	
	geonames_entry_list_free(context->list);
	context->list = NULL;
	osm_gps_map_clear_images(OSM_GPS_MAP(map));
      }
      
      /* render all icons */
      context->list = list;
      g_slist_foreach(list, geonames_entry_render, map);
    }
  }

  context->downloading = FALSE;
}

/* request geotagged wikipedia entries for current map view */
void geonames_wiki_request(GtkWidget *map) {
  wiki_context_t *context = 
    (wiki_context_t *)g_object_get_data(G_OBJECT(map), "wikipedia");

  /* don't have more than one pending request */
  if(context->downloading)
    return;

  context->downloading = TRUE;

  /* request area from map */
  coord_t pt1, pt2;
  osm_gps_map_get_bbox (OSM_GPS_MAP(map), &pt1, &pt2);

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
  net_io_download_async(url, geonames_wiki_cb, map);

  g_free(url);
}

// redo wiki entries after this time (ms) of map being idle
#define WIKI_UPDATE_TIMEOUT (1000)

static gboolean wiki_update(gpointer data) {
  wiki_context_t *context = 
    (wiki_context_t *)g_object_get_data(G_OBJECT(data), "wikipedia");

  geonames_wiki_request(GTK_WIDGET(data));

    context->timer_id = 0;
  return FALSE;
}

/* the map has changed, the wiki timer needs to be reset to update the */
/* wiki data after 2 seconds of idle time. */
static void 
wiki_timer_trigger(GtkWidget *widget) {
  wiki_context_t *context = 
    (wiki_context_t *)g_object_get_data(G_OBJECT(widget), "wikipedia");

  g_assert(context);

  /* if the timer is already running, then reset it, otherwise */
  /* install a new one*/
  if(context->timer_id) {
    gtk_timeout_remove(context->timer_id);
    context->timer_id = 0;
  }
  
  context->timer_id =
    gtk_timeout_add(WIKI_UPDATE_TIMEOUT, wiki_update, widget);  
}

static void on_map_changed(GtkWidget *widget, gpointer data ) {
  wiki_timer_trigger(widget);
}

static gboolean
on_map_motion(GtkWidget *widget, GdkEventMotion  *event, gpointer data)
{
  wiki_context_t *context = 
    (wiki_context_t *)g_object_get_data(G_OBJECT(widget), "wikipedia");

  g_assert(context);

  /* motion only delays an existing trigger, but doesn't trigger itself */
  if(context->timer_id && (event->state & GDK_BUTTON1_MASK)) 
    wiki_timer_trigger(widget);

  return FALSE;  /* allow further processing */
}

/* the wikimedia icons are automagically updated after one second */
/* of idle time */
void geonames_enable_wikipedia(GtkWidget *map, gboolean enable) {
  wiki_context_t *context = NULL;

  if(enable) {
    context = (wiki_context_t *)
      g_object_get_data(G_OBJECT(map), "wikipedia");
    
    /* there must be no context yet */
    g_assert(!context);
    
    /* create and register a context */
    context = g_new0(wiki_context_t, 1);
    g_object_set_data(G_OBJECT(map), "wikipedia", context);
    

    /* attach to maps changed event, so we can update the wiki */
    /* entries whenever the map changes */

    /* trigger wiki data update either by "changed" events ... */
    context->changed_handler_id =
      g_signal_connect(G_OBJECT(map), "changed",
		       G_CALLBACK(on_map_changed), context);

    /* ... or by the user dragging the map */
    context->motion_handler_id =
      g_signal_connect(G_OBJECT(map), "motion-notify-event",
		       G_CALLBACK(on_map_motion), context);

    /* clean up when map is destroyed */
    context->destroy_handler_id = 
      g_signal_connect(G_OBJECT(map), "destroy", 
		       G_CALLBACK(on_map_destroy), NULL);

    /* finally install handlers to handle clicks onto the wiki labels */
    context->press_handler_id =
      g_signal_connect(G_OBJECT(map), "button-press-event",
		       G_CALLBACK(on_map_button_press_event), NULL);

    context->release_handler_id =
      g_signal_connect(G_OBJECT(map), "button-release-event",
		       G_CALLBACK(on_map_button_press_event), NULL);

    /* only do request if map has a reasonable size. otherwise map may */
    /* still be in setup phase */
    if(map->allocation.width > 1 && map->allocation.height > 1)
      wiki_update(map);

  } else
    wiki_context_free(map, FALSE);

  gconf_set_bool("wikipedia", enable);
}

void geonames_wikipedia_restore(GtkWidget *map) {
  if(gconf_get_bool("wikipedia", FALSE)) {
    GtkWidget *toplevel = gtk_widget_get_toplevel(map);
    menu_check_set_active(toplevel, "Wikipedia", TRUE);
  }
}
