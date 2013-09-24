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

#ifndef HXM_H
#define HXM_H

#include <glib.h>

#define HXM_ENABLED "hxm_enabled"

typedef enum {
  HXM_STATE_UNKNOWN = 0,
  HXM_STATE_CONNECTING,
  HXM_STATE_INQUIRY,
  HXM_STATE_CONNECTED,
  HXM_STATE_DISCONNECTED,
  HXM_STATE_FAILED,
} hxm_state_t;

typedef struct hxm {
  GThread* thread_p;
  GMutex *mutex;

  hxm_state_t state;
  int handle;
  int clients;

  time_t time;           // time of last update
  unsigned char bat;     // in percent
  unsigned char hr;      // heart rate (30..240)
  unsigned char hrno;    // heart rate number
  float dist;            // 0..256 m
  float speed;           // 0 - 15,996 m/s
  unsigned short steps;  // 0-127
  float cad;             // steps/min

  GSList *callbacks;
} hxm_t;

typedef void (*hxm_cb)(hxm_t *hxm, void *data);
#define HXM_CB(f) ((hxm_cb)(f))

typedef struct {
  hxm_cb cb;
  void *data;
} hxm_callback_t;

hxm_t *hxm_init(void);
void hxm_register_callback(hxm_t *hxm, hxm_cb cb, void *data);
void hxm_unregister_callback(hxm_t *hxm,hxm_cb cb);
void hxm_release(hxm_t *hxm);

#endif // HXM_H
