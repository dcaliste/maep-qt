/*
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

#include <bluetooth/bluetooth.h>
#include <bluetooth/hci.h>
#include <bluetooth/hci_lib.h>
#include <bluetooth/rfcomm.h>

#include <errno.h>
#include <stdlib.h>
#include <unistd.h>

#include "hxm.h"
#include "misc.h"

#define HXM_CLASS  0x001f00
#define HXM_VENDOR 0x000780
#define HXM_NAME   "HXM"

#define HXM_POLY  0x8c
#define HXM_STX   0x02
#define HXM_ID    0x26
#define HXM_ETX   0x03


#define HXM_PACKET_SIZE			60

/* hexdump for debug */
void hexdump(void *buf, int size) {
  int i,n=0, b2c;
  unsigned char *ptr = buf;

  if(!size) return;

  while(size>0) {
    printf("HXM:   %04x: ", n);

    b2c = (size>16)?16:size;

    for(i=0;i<b2c;i++)
      printf("%02x ", 0xff & ptr[i]);

    printf("  ");

    for(i=0;i<(16-b2c);i++)
      printf("   ");

    for(i=0;i<b2c;i++)
      printf("%c",
	     (ptr[i]>=' ' &&
	      ptr[i]<127)?ptr[i]:'.');

    printf("\n");
    
    ptr  += b2c;
    size -= b2c;
    n    += b2c;
  }
}


static int bluez_connect(bdaddr_t *bdaddr, int channel) {
  struct sockaddr_rc rem_addr;                    
  int bt;                                         

  // bluez connects to BlueClient
  if( (bt = socket(PF_BLUETOOTH, SOCK_STREAM, BTPROTO_RFCOMM)) < 0 ) {
    fprintf(stderr, "HXM: Can't create socket. %s(%d)\n", strerror(errno), errno);
    return -1;                                                               
  }

  /* connect on rfcomm */
  memset(&rem_addr, 0, sizeof(rem_addr));
  rem_addr.rc_family = AF_BLUETOOTH;
  rem_addr.rc_bdaddr = *bdaddr;
  rem_addr.rc_channel = channel;
  if( connect(bt, (struct sockaddr *)&rem_addr, sizeof(rem_addr)) < 0 ){
    fprintf(stderr, "HXM: Can't connect. %s(%d)\n", strerror(errno), errno);
    close(bt);
    return -1;
  }

  return bt;
}

static ssize_t read_num(int fd, void *data, size_t count) {
  ssize_t total = 0;

  while(count) {
    ssize_t rd = read(fd, data, count);
    if(rd < 0) {
      perror("read");
      return rd;
    } else {
      count -= rd;
      data += rd;
      total += rd;
    }
  }
  return total;
}

static bdaddr_t *inquiry() {
  inquiry_info *info = NULL;
  uint8_t lap[3] = { 0x33, 0x8b, 0x9e };
  int num_rsp, i;
  bdaddr_t *result = NULL;
  int dev_id = hci_get_route(NULL);

  int sock = hci_open_dev( dev_id );
  if (dev_id < 0 || sock < 0) {
    perror("HXM: opening socket");
    return NULL;
  }

  num_rsp = hci_inquiry(-1, 8, 0, lap, &info, 0);
  if (num_rsp < 0) {
    perror("HXM: Inquiry failed");
    close( sock );
    return NULL;
  }

  for (i = 0; i < num_rsp; i++) {
    if((info+i)->dev_class[0] == ((HXM_CLASS >> 0) & 0xff) &&
       (info+i)->dev_class[1] == ((HXM_CLASS >> 8) & 0xff) &&
       (info+i)->dev_class[2] == ((HXM_CLASS >> 16) & 0xff) &&
       (info+i)->bdaddr.b[3]  == ((HXM_VENDOR >> 0) & 0xff) &&
       (info+i)->bdaddr.b[4]  == ((HXM_VENDOR >> 8) & 0xff) &&
       (info+i)->bdaddr.b[5]  == ((HXM_VENDOR >> 16) & 0xff) 
       ) {
      
      /* check if device name starts with "HXM" */
      char name[248];    
      memset(name, 0, sizeof(name));
      if((hci_read_remote_name(sock, &(info+i)->bdaddr, sizeof(name), 
			       name, 0) == 0) && 
	 (strncmp(HXM_NAME, name, sizeof(HXM_NAME)-1) == 0)) {

	result = malloc(sizeof(bdaddr_t));
	*result = (info+i)->bdaddr;
	close( sock );
	return result;
      }
    }
  }

  close( sock );
  return result;
}

static int check_frame(unsigned char *pkt) {
  
  int crc = 0, i, bit;

  if((pkt[0] != HXM_STX) || (pkt[59] != HXM_ETX))
    return 1;

  if(pkt[1] != HXM_ID)
    return 2;

  if(pkt[2] != 55)
    return 3;

  for (i = 3; i < 58; i++) {
    crc = (crc ^ pkt[i]);
  
    for (bit = 0; bit < 8; bit++) {
      if (crc & 1) crc = ((crc >> 1) ^ HXM_POLY);
      else         crc =  (crc >> 1);
    }
  }

  return (crc == pkt[58])?0:4;
}

static void hxm_cb_func(gpointer data, gpointer user_data) {
  hxm_callback_t *callback = (hxm_callback_t*)data;
  hxm_t *hxm = (hxm_t*)user_data;

  callback->cb(hxm, callback->data);
}

static void hxm_destroy(hxm_t *hxm) {
  printf("HXM: request to destroy context\n");

  g_mutex_lock(hxm->mutex);
  hxm->clients--;
  g_mutex_unlock(hxm->mutex);

  if(hxm->clients > 0)
    printf("HXM: still %d clients, keeping context\n", hxm->clients);
  else {
    printf("HXM: last client gone, destroying context\n");
    g_free(hxm);
  }
}

static gboolean hxm_notify(gpointer data) {
  hxm_t *hxm = (hxm_t*)data;

  if(hxm->callbacks) {
    g_mutex_lock(hxm->mutex);

    /* tell all clients */
    g_slist_foreach(hxm->callbacks, hxm_cb_func, hxm);

    g_mutex_unlock(hxm->mutex);
  }

  return FALSE; 
}

/* start a background process to connect to and handle hxm */
static gpointer hxm_thread(gpointer data) {
  bdaddr_t *bdaddr = NULL;
  hxm_t *hxm = (hxm_t*)data;

  char addr[18];
  printf("HXM: starting hxm\n");
  
  hxm->clients++;;

  char *bdaddr_str = gconf_get_string("hxm_bdaddr");
  if(bdaddr_str) {
    printf("HXM: trying to connect to preset hxm %s\n", bdaddr_str);
    
    /* if an address was found in gconf, try to use it */
    bdaddr = g_new0(bdaddr_t, 1);
    baswap(bdaddr, strtoba(bdaddr_str));

    hxm->state = HXM_STATE_CONNECTING;
    g_idle_add(hxm_notify, hxm); 
    hxm->handle = bluez_connect(bdaddr, 1);
  }

  /* no initial address or initial connect failed: perform search */
  if(hxm->handle < 0) {
    printf("HXM: preset connection not present/failed\n");

    hxm->state = HXM_STATE_INQUIRY;
    g_idle_add(hxm_notify, hxm); 

    sleep(5);

    bdaddr = inquiry();
    if(!bdaddr) {
      printf("HXM: no hxm found\n");
      hxm->state = HXM_STATE_FAILED;
      g_idle_add(hxm_notify, hxm); 
      hxm_destroy(hxm);
      return NULL;
    }

    ba2str(bdaddr, addr);
    printf("HXM: device %s found ...\n", addr);

    hxm->state = HXM_STATE_CONNECTING;
    hxm->handle = bluez_connect(bdaddr, 1);
    if(hxm->handle < 0) {
      printf("HXM: connection failed\n");
      if(bdaddr) g_free(bdaddr);
      hxm->state = HXM_STATE_FAILED;
      g_idle_add(hxm_notify, hxm); 
      hxm_destroy(hxm);
      return NULL;
    }

    /* save device name in gconf */
    gconf_set_string("hxm_bdaddr", addr);
  }
  
  hxm->state = HXM_STATE_CONNECTED;
  ba2str(bdaddr, addr);
  printf("HXM: connected to %s\n", addr);

  /* do the work while link ok and master still there... */
  while(hxm->handle >= 0 && hxm->clients > 1) {
    unsigned char buf[HXM_PACKET_SIZE];
    int size = read_num(hxm->handle, buf, sizeof(buf));
    if(size < 0) {
      printf("HXM: read failed\n");
      hxm->state = HXM_STATE_FAILED;
      g_idle_add(hxm_notify, hxm); 
      hxm->handle = -1;
    } else {
      //      hexdump(buf, size);

      /* check frame format */
      int i;
      if((i = check_frame(buf)) != 0) {
	printf("HXM: invalid frame %d, re-sync\n", i);
	
	/* try to re-sync */
	/* the 60 bytes should somewhere contain the ETX followed by */
	/* the STX marker. Skip HXM_PACKET_SIZE bytes after STX */
	int i;
	for(i=0;i<(HXM_PACKET_SIZE-1) &&
	      (buf[i] != HXM_ETX || buf[i+1] != HXM_STX) ;i++);
	
	read_num(hxm->handle, buf, i+1);
      } else {
	g_mutex_lock(hxm->mutex);
	// fill hxm data structure
	hxm->time  = time(NULL);
	hxm->bat   = 0xff & buf[11];
	hxm->hr    = 0xff & buf[12];
	hxm->hrno  = 0xff & buf[13];
	hxm->dist  = *((short*)(buf+50)) / 16.0;
	hxm->speed = *((short*)(buf+52)) / 256.0;
	hxm->steps = *((short*)(buf+54));
	hxm->cad   = *((short*)(buf+56)) / 256.0;
	g_mutex_unlock(hxm->mutex);

	g_idle_add(hxm_notify, hxm); 
      }
    }
  }

  hxm->state = HXM_STATE_DISCONNECTED;
  g_idle_add(hxm_notify, hxm); 

  if(bdaddr) g_free(bdaddr);

  close(hxm->handle);
  hxm->handle = -1;

  hxm_destroy(hxm);

  return NULL;
}

hxm_t *hxm_init(void) {
  hxm_t *hxm = g_new0(hxm_t, 1);

  hxm->clients = 1;
  hxm->handle = -1;
  hxm->state = HXM_STATE_UNKNOWN;

  /* start a new thread to listen to gpsd */
  hxm->mutex = g_mutex_new();
  hxm->thread_p = 
    g_thread_create(hxm_thread, hxm, FALSE, NULL);

  return hxm;
}

void hxm_register_callback(hxm_t *hxm, hxm_cb cb, void *data) {
  hxm_callback_t *callback = g_new0(hxm_callback_t, 1);

  callback->cb = cb;
  callback->data = data;

  hxm->callbacks = g_slist_append(hxm->callbacks, callback);
}

static gint compare(gconstpointer a, gconstpointer b) {
  return ((hxm_callback_t*)a)->cb != b;
}

void hxm_unregister_callback(hxm_t *hxm, hxm_cb cb) {
  /* find callback in list */
  GSList *list = g_slist_find_custom(hxm->callbacks, cb, compare);
  g_assert(list);

  /* and de-chain and free it */
  g_free(list->data);
  hxm->callbacks = g_slist_remove(hxm->callbacks, list->data);
}

static void hxm_unregister_all(hxm_t *hxm) {
  g_slist_foreach(hxm->callbacks, (GFunc)g_free, NULL);
  g_slist_free(hxm->callbacks);
  hxm->callbacks = NULL;
}

void hxm_release(hxm_t *hxm) { 
  hxm_unregister_all(hxm);
  hxm_destroy(hxm);
}

