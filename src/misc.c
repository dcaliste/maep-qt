/*
 * Copyright (C) 2008 Till Harbaum <till@harbaum.org>.
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

#include <glib/gstdio.h>
#include <stdlib.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <string.h>

#include "config.h"
#include "misc.h"

#define GCONF_PATH         "/apps/" APP "/%s"

void gconf_set_string(char *m_key, char *str) {
  GConfClient *client = gconf_client_get_default();
  char *key = g_strdup_printf(GCONF_PATH, m_key);

  if(!str || !strlen(str)) {
    gconf_client_unset(client, key, NULL); 
    g_free(key);
    return;
  }

  gconf_client_set_string(client, key, str, NULL);
  g_free(key);
}

char *gconf_get_string(char *m_key) {
  GConfClient *client = gconf_client_get_default();

  char *key = g_strdup_printf(GCONF_PATH, m_key);
  GConfValue *value = gconf_client_get(client, key, NULL);
  if(!value) {
    g_free(key);
    return NULL;
  }

  char *ret = gconf_client_get_string(client, key, NULL);
  g_free(key);
  return ret;
}

void gconf_set_bool(char *m_key, gboolean value) {
  GConfClient *client = gconf_client_get_default();
  char *key = g_strdup_printf(GCONF_PATH, m_key);
  gconf_client_set_bool(client, key, value, NULL);
  g_free(key);
}

gboolean gconf_get_bool(char *m_key, gboolean default_value) {
  GConfClient *client = gconf_client_get_default();

  char *key = g_strdup_printf(GCONF_PATH, m_key);
  GConfValue *value = gconf_client_get(client, key, NULL);
  if(!value) {
    g_free(key);
    return default_value;
  }

  gboolean ret = gconf_client_get_bool(client, key, NULL);
  g_free(key);
  return ret;
}

static const char *data_paths[] = {
  "~/." APP,                 // in home directory
  DATADIR ,                  // final installation path (e.g. /usr/share/maep)
#ifdef USE_MAEMO
  "/media/mmc1/" APP,        // path to external memory card
  "/media/mmc2/" APP,        // path to internal memory card
#endif
  "./data", "../data",       // local paths for testing
  NULL
};

char *find_file(char *name) {
  const char **path = data_paths;
  char *p = getenv("HOME");

  while(*path) {
    char *full_path = NULL;

    if(*path[0] == '~')
      full_path = g_strdup_printf("%s/%s/%s", p, *path+2, name);
    else
      full_path = g_strdup_printf("%s/%s", *path, name);

    if(g_file_test(full_path, G_FILE_TEST_IS_REGULAR))
      return full_path;

    g_free(full_path);
    path++;
  }

  return NULL;
}

GtkWidget *notebook_new(void) {
#ifdef MAEMO5               
  GtkWidget *vbox = gtk_vbox_new(FALSE, 0);

  GtkWidget *notebook =  gtk_notebook_new();

  /* solution for fremantle: we use a row of ordinary buttons instead */
  /* of regular tabs */                                                 

  /* hide the regular tabs */
  gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook), FALSE);

  gtk_box_pack_start_defaults(GTK_BOX(vbox), notebook);

  /* store a reference to the notebook in the vbox */
  g_object_set_data(G_OBJECT(vbox), "notebook", (gpointer)notebook);

  /* create a hbox for the buttons */
  GtkWidget *hbox = gtk_hbox_new(TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox, FALSE, FALSE, 0);
  g_object_set_data(G_OBJECT(vbox), "hbox", (gpointer)hbox);

  return vbox;
#else         
  return gtk_notebook_new();
#endif                      
}                           

GtkWidget *notebook_get_gtk_notebook(GtkWidget *notebook) {
#ifdef MAEMO5                                           
  return GTK_WIDGET(g_object_get_data(G_OBJECT(notebook), "notebook"));
#else                                                                  
  return notebook;                                                     
#endif                                                                 
}                                                                      

#ifdef MAEMO5
static void on_notebook_button_clicked(GtkWidget *button, gpointer data) {
  GtkNotebook *nb =                                                       
    GTK_NOTEBOOK(g_object_get_data(G_OBJECT(data), "notebook"));          

  gint page = (gint)g_object_get_data(G_OBJECT(button), "page");
  gtk_notebook_set_current_page(nb, page);                      
}                                                               
#endif             

void notebook_append_page(GtkWidget *notebook, 
                          GtkWidget *page, char *label) {
#ifdef MAEMO5                                         
  GtkNotebook *nb =                                      
    GTK_NOTEBOOK(g_object_get_data(G_OBJECT(notebook), "notebook"));

  gint page_num = gtk_notebook_append_page(nb, page, gtk_label_new(label));
  GtkWidget *button = NULL;                                                

  /* select button for page 0 by default */
  if(!page_num) {                          
    button = gtk_radio_button_new_with_label(NULL, label);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(button), TRUE);
    g_object_set_data(G_OBJECT(notebook), "group_master", (gpointer)button);
  } else {                                                                  
    GtkWidget *master = g_object_get_data(G_OBJECT(notebook), "group_master");
    button = gtk_radio_button_new_with_label_from_widget(                     
                                 GTK_RADIO_BUTTON(master), label);            
  }                                                                           

  gtk_toggle_button_set_mode(GTK_TOGGLE_BUTTON(button), FALSE);
  g_object_set_data(G_OBJECT(button), "page", (gpointer)page_num);

  gtk_signal_connect(GTK_OBJECT(button), "clicked",
           GTK_SIGNAL_FUNC(on_notebook_button_clicked), notebook);

  hildon_gtk_widget_set_theme_size(button,           
           (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH));

  gtk_box_pack_start_defaults(
              GTK_BOX(g_object_get_data(G_OBJECT(notebook), "hbox")),
              button);                                               

#else
  gtk_notebook_append_page(GTK_NOTEBOOK(notebook), page, gtk_label_new(label));
#endif                                                                         
}                             

gboolean yes_no_f(GtkWidget *parent, char *title, const char *fmt, ...) {

  va_list args;
  va_start( args, fmt );
  char *buf = g_strdup_vprintf(fmt, args);
  va_end( args );

#ifndef MAEMO5
#define RESPONSE_YES  GTK_RESPONSE_YES

  GtkWidget *dialog = gtk_message_dialog_new(
		     GTK_WINDOW(parent),
                     GTK_DIALOG_DESTROY_WITH_PARENT,
		     GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
                     buf);

  gtk_window_set_title(GTK_WINDOW(dialog), title);
#else
#define RESPONSE_YES  GTK_RESPONSE_OK

  GtkWidget *dialog = 
    hildon_note_new_confirmation(GTK_WINDOW(parent), buf);
#endif

  gboolean yes = (gtk_dialog_run(GTK_DIALOG(dialog)) == RESPONSE_YES);

  gtk_widget_destroy(dialog);

  g_free(buf);
  return yes;
}
