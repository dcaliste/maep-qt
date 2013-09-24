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

#include <glib.h>
#include <glib/gstdio.h>
#include <stdlib.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>
#include <string.h>
#include <ctype.h>

#include "config.h"
#include "misc.h"

#ifdef MAEMO5
#include <hildon/hildon-button.h>
#include <hildon/hildon-note.h>
#include <hildon/hildon-entry.h>
#include <hildon/hildon-pannable-area.h>
#include <mce/dbus-names.h>
#include <mce/mode-names.h>
#endif

#ifdef ENABLE_BROWSER_INTERFACE
#include <strings.h>
#ifndef USE_MAEMO
#include <libgnome/gnome-url.h>
#else
#include <tablet-browser-interface.h>
#endif

void browser_url(GtkWidget *root, char *url) {
#ifndef USE_MAEMO
  /* taken from gnome-open, part of libgnome */
  GError *err = NULL;
  gnome_url_show(url, &err);
#else
  osso_context_t *osso_context = 
    (osso_context_t*)g_object_get_data(G_OBJECT(root), "osso-context");

  osso_rpc_run_with_defaults(osso_context, "osso_browser",
			     OSSO_BROWSER_OPEN_NEW_WINDOW_REQ, NULL,
			     DBUS_TYPE_STRING, url,
			     DBUS_TYPE_BOOLEAN, FALSE, DBUS_TYPE_INVALID);
#endif
}
#endif

#define GCONF_PATH         "/apps/" APP "/%s"

void gconf_set_string(const char *m_key, const char *str) {
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

char *gconf_get_string(const char *m_key) {
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

void gconf_set_int(char *m_key, gint value) {
  GConfClient *client = gconf_client_get_default();
  char *key = g_strdup_printf(GCONF_PATH, m_key);
  gconf_client_set_int(client, key, value, NULL);
  g_free(key);
}

gint gconf_get_int(char *m_key, gint def_value) {
  GConfClient *client = gconf_client_get_default();

  char *key = g_strdup_printf(GCONF_PATH, m_key);
  GConfValue *value = gconf_client_get(client, key, NULL);
  if(!value) {
    g_free(key);
    return def_value;
  }

  gint ret = gconf_client_get_int(client, key, NULL);
  g_free(key);
  return ret;
}

void gconf_set_float(char *m_key, gfloat value) {
  GConfClient *client = gconf_client_get_default();
  char *key = g_strdup_printf(GCONF_PATH, m_key);
  gconf_client_set_float(client, key, value, NULL);
  g_free(key);
}

gfloat gconf_get_float(char *m_key, gfloat def_value) {
  GConfClient *client = gconf_client_get_default();

  char *key = g_strdup_printf(GCONF_PATH, m_key);
  GConfValue *value = gconf_client_get(client, key, NULL);
  if(!value) {
    g_free(key);
    return def_value;
  }

  gfloat ret = gconf_client_get_float(client, key, NULL);
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

#ifdef MAEMO5               
gboolean is_portrait() {
  GdkScreen *screen = gdk_screen_get_default();
  return (gdk_screen_get_width(screen) < gdk_screen_get_height(screen));
}
 
static
void on_orientation_changed(GdkScreen *screen, gpointer userdata) {
  GtkWidget *vbox = GTK_WIDGET(userdata);
  GtkWidget *hbox0 = g_object_get_data(G_OBJECT(vbox), "hbox0");
  GtkWidget *hbox1 = g_object_get_data(G_OBJECT(vbox), "hbox1");

  GSList *xbut = g_object_get_data(G_OBJECT(vbox), "xbut");
  if(!xbut) return;

  int i;
  for(i=0;i<g_slist_length(xbut);i++) {
    GtkWidget *but = g_slist_nth_data(xbut, i);
    g_object_ref(G_OBJECT(but));

    if(is_portrait()) {
      /* move button from hbox0 to hbox1 */
      gtk_container_remove(GTK_CONTAINER(hbox0), but);
      gtk_box_pack_start_defaults(GTK_BOX(hbox1), but);
    } else {
      /* move button from hbox1 to hbox0 */
      gtk_container_remove(GTK_CONTAINER(hbox1), but);
      gtk_box_pack_start_defaults(GTK_BOX(hbox0), but);
    }
    g_object_unref(G_OBJECT(but));
  }
  
  gtk_window_resize(GTK_WINDOW(gtk_widget_get_toplevel(vbox)), 800, 800);
}
#endif

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

  /* create hboxes for the buttons (second hbox for portrait mode) */
  GtkWidget *hbox0 = gtk_hbox_new(TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox0, FALSE, FALSE, 0);
  g_object_set_data(G_OBJECT(vbox), "hbox0", (gpointer)hbox0);
  GtkWidget *hbox1 = gtk_hbox_new(TRUE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), hbox1, FALSE, FALSE, 0);
  g_object_set_data(G_OBJECT(vbox), "hbox1", (gpointer)hbox1);

  /* this box needs to be re-arranged for portrait mode */
  GdkScreen *screen = gdk_screen_get_default(); 
  g_signal_connect(screen, "size-changed", G_CALLBACK(on_orientation_changed), vbox);

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
	GTK_BOX(g_object_get_data(G_OBJECT(notebook),   
	  (!is_portrait() || page_num < 3)?"hbox0":"hbox1")), button);

  /* update list of "buttons that need to be moved when switching orientation" */
  if(page_num >= 3) {
    GSList *xbut = g_object_get_data(G_OBJECT(notebook), "xbut");
    xbut = g_slist_append(xbut, button);
    g_object_set_data(G_OBJECT(notebook), "xbut", xbut);
  }

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

static void vmessagef(GtkWidget *parent, int type, int buttons,
                      char *title, const char *fmt, 
                      va_list args) {

  char *buf = g_strdup_vprintf(fmt, args);

#ifndef MAEMO5
  GtkWidget *dialog = gtk_message_dialog_new(
                           GTK_WINDOW(parent),
                           GTK_DIALOG_DESTROY_WITH_PARENT,
                           type, buttons, buf);

  gtk_window_set_title(GTK_WINDOW(dialog), title);
#else
  GtkWidget *dialog = 
    hildon_note_new_information(GTK_WINDOW(parent), buf);
#endif

  gtk_dialog_run(GTK_DIALOG(dialog));
  gtk_widget_destroy(dialog);

  g_free(buf);
}

void errorf(GtkWidget *parent, const char *fmt, ...) {
  va_list args;
  va_start( args, fmt );

  vmessagef(parent, GTK_MESSAGE_ERROR, 
            GTK_BUTTONS_CLOSE, _("Error"), fmt, args);
  va_end( args );
}

/* Converts an integer value to its hex character*/
static char to_hex(char code) {
  static char hex[] = "0123456789abcdef";
  return hex[code & 15];
}

/* Returns a url-encoded version of str */
/* IMPORTANT: be sure to free() the returned string after use */
char *url_encode(char *str) {
  char *pstr = str, *buf = g_malloc(strlen(str) * 3 + 1), *pbuf = buf;
  while (*pstr) {
    if (isalnum(*pstr) || *pstr == '-' || *pstr == '_' || 
	*pstr == '.' || *pstr == '~') 
      *pbuf++ = *pstr;
    else if (*pstr == ' ') 
      *pbuf++ = '+';
    else 
      *pbuf++ = '%', *pbuf++ = to_hex(*pstr >> 4), 
	*pbuf++ = to_hex(*pstr & 15);
    pstr++;
  }
  *pbuf = '\0';
  return buf;
}

GtkWidget *button_new(void) {
  GtkWidget *button = gtk_button_new();
#ifdef MAEMO5
  hildon_gtk_widget_set_theme_size(button, 
           (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH));
#endif  
  return button;
}

GtkWidget *button_new_with_label(char *label) {
  GtkWidget *button = gtk_button_new_with_label(label);
#ifdef MAEMO5
  hildon_gtk_widget_set_theme_size(button, 
           (HILDON_SIZE_FINGER_HEIGHT | HILDON_SIZE_AUTO_WIDTH));
#endif  
  return button;
}

GtkWidget *entry_new(void) {
#ifndef MAEMO5
  return gtk_entry_new();
#else
  return hildon_entry_new(HILDON_SIZE_AUTO);
#endif
}

GtkWidget *label_big_new(char *str) {
  GtkWidget *label = gtk_label_new("");
  char *markup = 
    g_markup_printf_escaped("<span size='x-large'>%s</span>", str);
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);
  return label;
}

GtkWidget *label_left_new(char *str) {
  GtkWidget *widget = gtk_label_new(str);
  gtk_misc_set_alignment(GTK_MISC(widget), 0.0f, 0.5f);
  return widget;
}

GtkWidget *label_right_new(char *str) {
  GtkWidget *widget = gtk_label_new(str);
  gtk_misc_set_alignment(GTK_MISC(widget), 1.0f, 0.5f);
  return widget;
}

static void  
on_label_realize(GtkWidget *widget, gpointer user_data)  {
  /* get parent size (which is a container) */
  gtk_widget_set_size_request(widget, widget->parent->allocation.width, -1);
}

GtkWidget *label_wrap_new(char *str) {
  GtkWidget *label = gtk_label_new(str);

  gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD);
  gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);

  g_signal_connect(G_OBJECT(label), "realize",
		   G_CALLBACK(on_label_realize), NULL);

  return label;
}

/* create a pannable area */
GtkWidget *scrolled_window_new(GtkPolicyType hp,  GtkPolicyType vp) {
#ifdef MAEMO5
  HildonMovementMode mode = 0;
  GtkWidget *pannable_area = hildon_pannable_area_new();
  if(hp != GTK_POLICY_NEVER) mode |= HILDON_MOVEMENT_MODE_HORIZ;
  if(vp != GTK_POLICY_NEVER) mode |= HILDON_MOVEMENT_MODE_VERT;

  g_object_set(G_OBJECT(pannable_area), "mov-mode", mode, NULL);

  return pannable_area;
#else
  GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), hp, vp);
  gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(scrolled_window), 
				      GTK_SHADOW_NONE);
  return scrolled_window;
#endif
}

void scrolled_window_add_with_viewport(GtkWidget *win, GtkWidget *child) {
#ifdef MAEMO5
  hildon_pannable_area_add_with_viewport(HILDON_PANNABLE_AREA(win), child);
#else
  gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(win), child);
  GtkWidget *viewport = gtk_bin_get_child(GTK_BIN(win));
  gtk_viewport_set_shadow_type(GTK_VIEWPORT(viewport), GTK_SHADOW_NONE);
#endif
}

void scrolled_window_add(GtkWidget *win, GtkWidget *child) {
  gtk_container_add(GTK_CONTAINER(win), child);
}

#ifdef MAEMO5
static void
set_orientation(GtkWidget *window, const gchar *mode) {
  HildonPortraitFlags flags = HILDON_PORTRAIT_MODE_SUPPORT;
  
  if (strcmp(mode, "portrait") == 0)
    flags += HILDON_PORTRAIT_MODE_REQUEST;
  hildon_gtk_window_set_portrait_flags(GTK_WINDOW(window), flags);
}

#define MCE_MATCH_RULE "type='signal',interface='" MCE_SIGNAL_IF "',member='" MCE_DEVICE_ORIENTATION_SIG "'"

static DBusHandlerResult
dbus_handle_mce_message(DBusConnection *conn, DBusMessage *msg, gpointer data) {
  GtkWidget *window = GTK_WIDGET(data);

  DBusMessageIter iter;
  const gchar *mode = NULL;
  
  if (dbus_message_is_signal(msg, MCE_SIGNAL_IF,
			     MCE_DEVICE_ORIENTATION_SIG)) {
    if (dbus_message_iter_init(msg, &iter)) {
      dbus_message_iter_get_basic(&iter, &mode);
      set_orientation(window, mode);
    }
  }
  return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

void rotation_enable(GtkWidget *window) {
  DBusError error;
  char *mode;
  DBusMessage *message, *reply;

  osso_context_t *osso_context = 
    g_object_get_data(G_OBJECT(window), "osso-context");

  g_assert(osso_context);
  
  /* Get the system DBus connection */
  DBusConnection *conn = osso_get_sys_dbus_connection(osso_context);
  
  /* Add the callback, which should be called, once the device is rotated */
  dbus_bus_add_match(conn, MCE_MATCH_RULE, NULL);
  dbus_connection_add_filter(conn, dbus_handle_mce_message, window, NULL);
  
  /* Get the current orientation */
  message = dbus_message_new_method_call(MCE_SERVICE,
					 MCE_REQUEST_PATH,
					 MCE_REQUEST_IF,
					 MCE_ACCELEROMETER_ENABLE_REQ);
  
  dbus_error_init(&error);
  reply = dbus_connection_send_with_reply_and_block(conn, message,
						    -1, &error);
  dbus_message_unref(message); 
  
  if (dbus_message_get_args(reply, NULL,
			    DBUS_TYPE_STRING, &mode,
			    DBUS_TYPE_INVALID)) {
    set_orientation(window, mode);
  }
  dbus_message_unref(reply); 
}

void rotation_disable(GtkWidget *window) {
  osso_context_t *osso_context = 
    g_object_get_data(G_OBJECT(window), "osso-context");

  g_assert(osso_context);

  DBusConnection *conn = osso_get_sys_dbus_connection(osso_context);
  DBusMessage *message;
  
  dbus_bus_remove_match(conn, MCE_MATCH_RULE, NULL);
  dbus_connection_remove_filter(conn, dbus_handle_mce_message, NULL);
  
  message = dbus_message_new_method_call(MCE_SERVICE,
					 MCE_REQUEST_PATH,
					 MCE_REQUEST_IF,
					 MCE_ACCELEROMETER_DISABLE_REQ);
  dbus_message_set_no_reply(message, TRUE);
  
  dbus_connection_send(conn, message, NULL);
  dbus_message_unref(message); 
}

#endif

