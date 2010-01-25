/*
 * Copyright (C) 2010 Till Harbaum <till@harbaum.org>.
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
#include <gtk/gtk.h>

#include "misc.h"
#include "net_io.h"

#include <curl/curl.h>
#include <curl/types.h> /* new for v7 */
#include <curl/easy.h>  /* new for v7 */
#include <unistd.h>
#include <string.h>

#include <gconf/gconf.h>
#include <gconf/gconf-client.h>

static struct http_message_s {
  int id;
  char *msg;
} http_messages [] = {
  {   0, "Curl internal failure" },
  { 200, "Ok" },
  { 301, "Moved permanently" },
  { 302, "Found" },
  { 303, "See Other" },
  { 400, "Bad Request (area too big?)" },
  { 401, "Unauthorized (wrong user/password?)" },
  { 403, "Forbidden" },
  { 404, "Not Found" },
  { 405, "Method Not Allowed" },
  { 410, "Gone" },
  { 412, "Precondition Failed" },
  { 417, "Expectation failed (expect rejected)" },
  { 500, "Internal Server Error" },
  { 503, "Service Unavailable" },
  { 0,   NULL }
};

/* structure shared between worker and master thread */
typedef struct {
  gint refcount;       /* reference counter for master and worker thread */ 

  char *url, *user;
  gboolean cancel;
  float progress;

  /* curl/http related stuff: */
  CURLcode res;
  long response;
  char buffer[CURL_ERROR_SIZE];

  net_io_cb cb;
  gpointer data;

  net_result_t result;

} net_io_request_t;

static char *http_message(int id) {
  struct http_message_s *msg = http_messages;

  while(msg->msg) {
    if(msg->id == id) return _(msg->msg);
    msg++;
  }

  return NULL;
}

static gint dialog_destroy_event(GtkWidget *widget, gpointer data) {
  /* set cancel flag */
  *(gboolean*)data = TRUE;
  return FALSE;
}

static void on_cancel(GtkWidget *widget, gpointer data) {
  /* set cancel flag */
  *(gboolean*)data = TRUE;
}

/* create the dialog box shown while worker is running */
static GtkWidget *busy_dialog(GtkWidget *parent, GtkWidget **pbar,
			      gboolean *cancel_ind) {
  GtkWidget *dialog = gtk_dialog_new();

  gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);
  gtk_window_set_title(GTK_WINDOW(dialog), _("Downloading"));
  gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 10);

  gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
  gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(parent));

  g_assert(pbar);
  *pbar = gtk_progress_bar_new();
  gtk_progress_bar_set_pulse_step(GTK_PROGRESS_BAR(*pbar), 0.1);

  gtk_box_pack_start_defaults(GTK_BOX(GTK_DIALOG(dialog)->vbox), *pbar);

  GtkWidget *button = button_new_with_label(_("Cancel"));
  gtk_signal_connect(GTK_OBJECT(button), "clicked",
		     GTK_SIGNAL_FUNC(on_cancel), (gpointer)cancel_ind);
  gtk_container_add(GTK_CONTAINER(GTK_DIALOG(dialog)->action_area), button);

  gtk_signal_connect(GTK_OBJECT(dialog), "destroy", 
	     G_CALLBACK(dialog_destroy_event), (gpointer)cancel_ind);

  gtk_widget_show_all(dialog);

  return dialog;
}

static void request_free(net_io_request_t *request) {
  /* decrease refcount and only free structure if no references are left */
  request->refcount--;
  if(request->refcount) {
    //    printf("still %d references, keeping request\n", request->refcount);
    return;
  }

  //  printf("no references left, freeing request\n");
  if(request->url)  g_free(request->url);
  if(request->user) g_free(request->user);

  g_free(request);
}

static int curl_progress_func(net_io_request_t *request,
			    double t, /* dltotal */ double d, /* dlnow */
			    double ultotal, double ulnow) {
  request->progress = t?d/t:0;
  return 0;
}

static size_t mem_write(void *ptr, size_t size, size_t nmemb, 
			void *stream) {
  net_mem_t *p = (net_mem_t*)stream;
  
  p->ptr = g_realloc(p->ptr, p->len + size*nmemb + 1);
  if(p->ptr) {
    memcpy(p->ptr+p->len, ptr, size*nmemb);
    p->len += size*nmemb;
    p->ptr[p->len] = 0;
  }
  return nmemb;
}

#define PROXY_KEY  "/system/http_proxy/"

void net_io_set_proxy(CURL *curl) {

#if 1
#warning "http_proxy not evaluated yet!"
#else
  /* get proxy settings */
  static char proxy_buffer[64] = "";
  
  /* use environment settings if preset (e.g. for scratchbox) */
  const char *proxy = g_getenv("http_proxy");
  if(proxy) return proxy;
#endif

  GConfClient *gconf_client = gconf_client_get_default();

  /* ------------- get proxy settings -------------------- */
  if(gconf_client_get_bool(gconf_client, 
		   PROXY_KEY "use_http_proxy", NULL)) {

    /* get basic settings */
    char *host = 
      gconf_client_get_string(gconf_client, PROXY_KEY "host", NULL);
    if(host) {
      int port =
	gconf_client_get_int(gconf_client, PROXY_KEY "port", NULL);

      curl_easy_setopt(curl, CURLOPT_PROXY, host);
      curl_easy_setopt(curl, CURLOPT_PROXYPORT, port);
      g_free(host);

      if(gconf_client_get_bool(gconf_client, 
	       PROXY_KEY "use_authentication", NULL)) {
    
	char *user = gconf_client_get_string(gconf_client, 
		     PROXY_KEY "authentication_user", NULL);
	char *pass = gconf_client_get_string(gconf_client, 
		     PROXY_KEY "authentication_password", NULL);

	char *cred = g_strdup_printf("%s:%s", user, pass);
	curl_easy_setopt(curl, CURLOPT_PROXYUSERPWD, cred);
	g_free(cred);

	if(pass) g_free(pass);
	if(user) g_free(user);
      }
    }
  }
}

static gboolean net_io_idle_cb(gpointer data) {
  net_io_request_t *request = (net_io_request_t *)data;

  /* the http connection itself may have failed */
  if(request->res != 0) {
    request->result.code = 2;
    printf("Download failed with message: %s\n", request->buffer);
  } else  if(request->response != 200) {
    /* a valid http connection may have returned an error */
    request->result.code = 3;
    printf("Download failed with code %ld: %s\n", 
	   request->response, http_message(request->response));
  }

  /* call application callback */
  if(request->cb)
    request->cb(&request->result, request->data);

  request_free(request);

  return FALSE;
}

static void *worker_thread(void *ptr) {
  net_io_request_t *request = (net_io_request_t*)ptr;
  
  printf("thread: running\n");

  //  sleep(2);  // useful for debugging

  CURL *curl = curl_easy_init();
  if(curl) {
    /* prepare target memory */
    request->result.data.ptr = NULL;
    request->result.data.len = 0;

    curl_easy_setopt(curl, CURLOPT_URL, request->url);
      
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &request->result.data);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, mem_write);

    net_io_set_proxy(curl);

    /* set user name and password for the authentication */
    if(request->user)
      curl_easy_setopt(curl, CURLOPT_USERPWD, request->user);
    
    /* setup progress notification */
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION, curl_progress_func);
    curl_easy_setopt(curl, CURLOPT_PROGRESSDATA, request);
    
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, request->buffer);
    
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1l);
    
    /* play nice and report some user agent */
    curl_easy_setopt(curl, CURLOPT_USERAGENT, PACKAGE "-libcurl/" VERSION); 
    
    request->res = curl_easy_perform(curl);
    printf("thread: curl perform returned with %d\n", request->res);
    
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &request->response);

    /* always cleanup */
    curl_easy_cleanup(curl);
  } else
    printf("thread: unable to init curl\n");
  
  printf("thread: io done\n");

  if(request->cb)
    g_idle_add(net_io_idle_cb, request);

  request_free(request);
  
  printf("thread: terminating\n");
  return NULL;
}

static gboolean net_io_do(GtkWidget *parent, net_io_request_t *request) {
  /* the request structure is shared between master and worker thread. */
  /* typically the master thread will do some waiting until the worker */
  /* thread returns. But the master may very well stop waiting since e.g. */
  /* the user activated some cancel button. The client will learn this */
  /* from the fact that it's holding the only reference to the request */

  GtkWidget *pbar = NULL;
  GtkWidget *dialog = busy_dialog(parent, &pbar, &request->cancel);
  
  /* create worker thread */
  request->refcount = 2;   // master and worker hold a reference
  if(!g_thread_create(&worker_thread, request, FALSE, NULL) != 0) {
    g_warning("failed to create the worker thread");

    /* free request and return error */
    request->refcount--;    /* decrease by one for dead worker thread */
    gtk_widget_destroy(dialog);
    return FALSE;
  }

  /* wait for worker thread */
  float progress = 0;
  while(request->refcount > 1 && !request->cancel) {
    while(gtk_events_pending())
      gtk_main_iteration();

    /* worker has made progress changed the progress value */
    if(request->progress != progress) {
      gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(pbar), request->progress);
      progress = request->progress;
    } else 
      if(!progress) 
	gtk_progress_bar_pulse(GTK_PROGRESS_BAR(pbar));

    usleep(100000);
  }

  gtk_widget_destroy(dialog);

  /* user pressed cancel */
  if(request->refcount > 1) {
    printf("operation cancelled, leave worker alone\n");
    return FALSE;
  }

  printf("worker thread has ended\n");

  /* --------- evaluate result --------- */

  /* the http connection itself may have failed */
  if(request->res != 0) {
    errorf(parent, _("Download failed with message:\n\n%s"), request->buffer);
    return FALSE;
  }

  /* a valid http connection may have returned an error */
  if(request->response != 200) {
    errorf(parent, _("Download failed with code %ld:\n\n%s\n"), 
	   request->response, http_message(request->response));
    return FALSE;
  }

  return TRUE;
}

gboolean net_io_download(GtkWidget *parent, char *url, char **mem) {
  net_io_request_t *request = g_new0(net_io_request_t, 1);

  request->url = g_strdup(url);

  gboolean result = net_io_do(parent, request);
  if(result) 
    *mem = request->result.data.ptr;
  
  request_free(request);
  return result;
}

/* --------- start of async io ------------ */

static gboolean net_io_do_async(net_io_request_t *request) {
  /* the request structure is shared between master and worker thread. */
  /* typically the master thread will do some waiting until the worker */
  /* thread returns. But the master may very well stop waiting since e.g. */
  /* the user activated some cancel button. The client will learn this */
  /* from the fact that it's holding the only reference to the request */

  /* create worker thread */
  request->refcount = 2;   // master and worker hold a reference
  if(!g_thread_create(&worker_thread, request, FALSE, NULL) != 0) {
    g_warning("failed to create the worker thread");

    /* free request and return error */
    request->refcount--;    /* decrease by one for dead worker thread */
    return FALSE;
  }
  
  return TRUE;
}

net_io_t net_io_download_async(char *url, net_io_cb cb, gpointer data) {
  net_io_request_t *request = g_new0(net_io_request_t, 1);

  request->url = g_strdup(url);
  request->cb = cb;
  request->data = data;

  if(!net_io_do_async(request)) {
    request->result.code = 1;  // failure
    cb(&request->result, data); 
    return NULL;
  }

  return (net_io_t)request;
}

void net_io_cancel_async(net_io_t io) {
  net_io_request_t *request = (net_io_request_t*)io;
  g_assert(request);

  request->cb = NULL;   // just cancelling the callback is sufficient
}
