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

#include "config.h"
#include "net_io.h"
#include "geonames.h"
#include "misc.h"
#include "menu.h"
#include "osm-gps-map.h"
#include "converter.h"

#include <libxml/parser.h>
#include <libxml/tree.h>

#ifdef MAEMO5
#include <hildon/hildon-pannable-area.h>
#endif

#ifndef LIBXML_TREE_ENABLED
#error "Tree not enabled in libxml"
#endif

#include <string.h>

typedef struct {
  char *name, *admin;
  coord_t pos;
} geonames_code_t;

typedef struct {
  char *title;
  coord_t pos;
} geonames_entry_t;

#define MAX_RESULT 50
#define GEONAMES  "http://ws.geonames.org/"
#define GEONAMES_SEARCH "geonames_search"

/* -------------- begin of xml parser ---------------- */

static void string_get(xmlNode *node, char *name, char **dst) {
  if(*dst) return;   /* don't overwrite anything */

  if(strcasecmp((char*)node->name, name) != 0) 
    return;

  char *str = (char*)xmlNodeGetContent(node);

  *dst = g_strdup(str);
  xmlFree(str);
}

static char *code_append(char *src, char *append) {
  if(!append) return src;  /* nothing to append */

  if(src) {
    char *new_str = g_strdup_printf("%s, %s", src, append);
    g_free(append);
    g_free(src);
    return new_str;
  }

  return append;
}

static geonames_code_t *geonames_parse_code(xmlDocPtr doc, xmlNode *a_node) {
  xmlNode *cur_node = NULL;
  geonames_code_t *code = g_new0(geonames_code_t, 1);

  char *name = NULL, *country_code = NULL, *postal_code = NULL;
  char *admin_name1 = NULL, *admin_name2 = NULL, *admin_name3 = NULL;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {

      string_get(cur_node, "name", &name);
      string_get(cur_node, "postalcode", &postal_code);
      string_get(cur_node, "countryCode", &country_code);

      string_get(cur_node, "adminName1", &admin_name1);
      string_get(cur_node, "adminName2", &admin_name2);
      string_get(cur_node, "adminName3", &admin_name3);

      if(strcasecmp((char*)cur_node->name, "lat") == 0) {
	char *str = (char*)xmlNodeGetContent(cur_node);
	code->pos.rlat = deg2rad(g_ascii_strtod(str, NULL));
 	xmlFree(str);
      } else if(strcasecmp((char*)cur_node->name, "lng") == 0) {
	char *str = (char*)xmlNodeGetContent(cur_node);
	code->pos.rlon = deg2rad(g_ascii_strtod(str, NULL));
 	xmlFree(str);
      } else
	printf("found unhandled geonames/code/%s\n", cur_node->name);      
    }
  }

  /* build name and admin strings */
  code->name = code_append(code->name, name);
  code->name = code_append(code->name, postal_code);
  code->name = code_append(code->name, country_code);

  code->admin = code_append(code->admin, admin_name1);
  code->admin = code_append(code->admin, admin_name2);
  code->admin = code_append(code->admin, admin_name3);

  return code;
}

static geonames_entry_t *geonames_parse_entry(xmlDocPtr doc, xmlNode *a_node) {
  xmlNode *cur_node = NULL;
  geonames_entry_t *entry = g_new0(geonames_entry_t, 1);

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {

      string_get(cur_node, "title", &entry->title);

      if(strcasecmp((char*)cur_node->name, "lat") == 0) {
	char *str = (char*)xmlNodeGetContent(cur_node);
	entry->pos.rlat = deg2rad(g_ascii_strtod(str, NULL));
	xmlFree(str);
      } else if(strcasecmp((char*)cur_node->name, "lng") == 0) {
	char *str = (char*)xmlNodeGetContent(cur_node);
	entry->pos.rlon = deg2rad(g_ascii_strtod(str, NULL));
	xmlFree(str);
      } else
	printf("found unhandled geonames/code/%s\n", cur_node->name);      
    }
  }
  
  return entry;
}

static GSList *geonames_parse_geonames(xmlDocPtr doc, xmlNode *a_node) {
  GSList *list = NULL;
  xmlNode *cur_node = NULL;

  for (cur_node = a_node->children; cur_node; cur_node = cur_node->next) {
    if (cur_node->type == XML_ELEMENT_NODE) {
      if(strcasecmp((char*)cur_node->name, "code") == 0) {
	list = g_slist_append(list, geonames_parse_code(doc, cur_node));
      } else if(strcasecmp((char*)cur_node->name, "entry") == 0) {
	list = g_slist_append(list, geonames_parse_entry(doc, cur_node));
      } else
	printf("found unhandled geonames/%s\n", cur_node->name);      
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
      else 
	printf("found unhandled %s\n", cur_node->name);
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

static void geonames_code_free(gpointer data, gpointer user_data) {
  geonames_code_t *code = (geonames_code_t*)data;
  if(code->name) g_free(code->name);
  if(code->admin) g_free(code->admin);
}

static void geonames_code_list_free(GSList *list) {
  g_slist_foreach(list, geonames_code_free, NULL);
  g_slist_free(list);
}

static void geonames_entry_free(gpointer data, gpointer user_data) {
  geonames_entry_t *entry = (geonames_entry_t*)data;
  if(entry->title) g_free(entry->title);
}

static void geonames_entry_list_free(GSList *list) {
  g_slist_foreach(list, geonames_entry_free, NULL);
  g_slist_free(list);
}

/* ------------- end of freeing ------------------ */

enum {
  GEONAMES_PICKER_COL_NAME = 0,
  GEONAMES_PICKER_COL_ADMIN,
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
    geonames_code_t *code = NULL;
    gtk_tree_model_get(model, &iter, 
		       GEONAMES_PICKER_COL_DATA, &code, 
		       -1);

    osm_gps_map_set_center(OSM_GPS_MAP(userdata), 
	   rad2deg(code->pos.rlat), rad2deg(code->pos.rlon));


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

  store = gtk_list_store_new(GEONAMES_PICKER_NUM_COLS, 
			     G_TYPE_STRING,
			     G_TYPE_STRING,
			     G_TYPE_POINTER);
  
  while(list) {
    geonames_code_t *code = (geonames_code_t*)list->data;

    /* Append a row and fill in some data */
    gtk_list_store_append (store, &iter);
  
    gtk_list_store_set(store, &iter,
		       GEONAMES_PICKER_COL_NAME, code->name,
		       GEONAMES_PICKER_COL_ADMIN, code->admin,
		       GEONAMES_PICKER_COL_DATA, code,
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

static void on_search_clicked(GtkButton *button, gpointer user) {
  GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(button));
  GtkWidget *entry = g_object_get_data(G_OBJECT(button), "search_entry");
  GtkWidget *hbox = g_object_get_data(G_OBJECT(button), "search_widget");
  g_assert(entry);

  char *phrase = (char*)gtk_entry_get_text(GTK_ENTRY(entry));
  gconf_set_string("search_text", phrase);

  /* disable ui while search is running */
  gtk_widget_set_sensitive(hbox, FALSE);

  /* build search request */
  char *encoded_phrase = url_encode(phrase);
  char *url = g_strdup_printf(
	      GEONAMES "postalCodeSearch?placename=%s&maxRows=%u", 
	      encoded_phrase, MAX_RESULT);
  g_free(encoded_phrase);

  char *data = NULL;
  if(net_io_download(toplevel, url, &data)) {

    /* feed this into the xml parser */
    xmlDoc *doc = NULL;

    LIBXML_TEST_VERSION;
  
    /* parse the file and get the DOM */
    if((doc = xmlReadMemory(data, strlen(data), NULL, NULL, 0)) == NULL) {
      xmlErrorPtr errP = xmlGetLastError();
      printf("While parsing:\n\n%s\n", errP->message);
    } else {
      GSList *list = geonames_parse_doc(doc); 

      if(g_slist_length(list))
	geonames_display_search_results(toplevel, user, list);
      else
	errorf(toplevel, _("No places matching the search term "
			   "could be found."));

      geonames_code_list_free(list);
    }
  } else
    printf("download failed\n");

  g_free(url);

  /* re-enable ui */
  gtk_widget_set_sensitive(hbox, TRUE);
}

static void on_search_close_clicked(GtkButton *button, gpointer user) {
  GtkWidget *map = GTK_WIDGET(user);
  GtkWidget *toplevel = gtk_widget_get_toplevel(map);

  GtkWidget *hbox = g_object_get_data(G_OBJECT(toplevel), GEONAMES_SEARCH);
  g_assert(hbox);

  menu_enable(toplevel, "Search", TRUE); 
  g_object_set_data(G_OBJECT(toplevel), GEONAMES_SEARCH, NULL);
  gtk_widget_destroy(hbox);

  /* give entry keyboard focus back to map */
  gtk_widget_grab_focus(map);
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

void geonames_enable_search(GtkWidget *parent, GtkWidget *map) {
  GtkWidget *hbox = g_object_get_data(G_OBJECT(parent), GEONAMES_SEARCH);
  g_assert(!hbox);

  menu_enable(parent, "Search", FALSE); 

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
  g_object_set_data(G_OBJECT(button), "search_widget", hbox);
  
  /* create close button */
  button = button_new();
  g_signal_connect_after(button, "clicked", 
			 G_CALLBACK(on_search_close_clicked), map);
  GtkWidget *image = 
    gtk_image_new_from_stock(GTK_STOCK_CLOSE, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image(GTK_BUTTON(button), image);
  gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
  
  /* the parent is a container with a vbox being its child */
  GList *children = gtk_container_get_children(GTK_CONTAINER(parent));
  g_assert(g_list_length(children) == 1);
  
  gtk_widget_show_all(hbox);
  gtk_box_pack_start(GTK_BOX(children->data), hbox, FALSE, FALSE, 0);

  /* try to load preset */
  char *preset = gconf_get_string("search_text");
  if(preset) gtk_entry_set_text(GTK_ENTRY(entry), preset);

  /* give entry keyboard focus */
  gtk_widget_grab_focus(entry);
  
  g_object_set_data(G_OBJECT(parent), GEONAMES_SEARCH, hbox);
}

/* request geotagged wikipedia entries for current map view */
void geonames_wikipedia(GtkWidget *parent, GtkWidget *map) {
  /* request area from map */
  coord_t pt1, pt2;
  osm_gps_map_get_bbox (OSM_GPS_MAP(map), &pt1, &pt2);

  /* create ascii (dot decimal point) strings */
  char str[4][16];
  g_ascii_formatd(str[0], sizeof(str[0]), "%.07f", rad2deg(pt1.rlat));
  g_ascii_formatd(str[1], sizeof(str[1]), "%.07f", rad2deg(pt2.rlat));
  g_ascii_formatd(str[2], sizeof(str[2]), "%.07f", rad2deg(pt1.rlon));
  g_ascii_formatd(str[3], sizeof(str[3]), "%.07f", rad2deg(pt2.rlon));

  /* build complete url for request */
  char *url = g_strdup_printf(
	      GEONAMES "wikipediaBoundingBox?"
	      "north=%s&south=%s&west=%s&east=%s", 
	      str[0], str[1], str[2], str[3]);

  char *data = NULL;
  if(net_io_download(parent, url, &data)) {
    printf("ok: %s\n", data);

    /* feed this into the xml parser */
    xmlDoc *doc = NULL;

    LIBXML_TEST_VERSION;
  
    /* parse the file and get the DOM */
    if((doc = xmlReadMemory(data, strlen(data), NULL, NULL, 0)) == NULL) {
      xmlErrorPtr errP = xmlGetLastError();
      printf("While parsing:\n\n%s\n", errP->message);
    } else {
      GSList *list = geonames_parse_doc(doc); 
      
      printf("got %d results\n", g_slist_length(list));

      if(g_slist_length(list))
	;
      
      geonames_entry_list_free(list);
    }
  } else
    printf("download failed\n");

  g_free(url);
}
