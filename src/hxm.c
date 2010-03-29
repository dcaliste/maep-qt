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

typedef struct {
  unsigned char bat;     // in percent
  unsigned char hr;      // heart rate (30..240)
  unsigned char hrno;    // heart rate number
  float dist;            // 0..256 m
  float speed;           // 0 - 15,996 m/s
  unsigned short steps;  // 0-127
  float cad;             // steps/min
} hxm_t;

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
    printf("  %04x: ", n);

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
    fprintf(stderr, "Can't create socket. %s(%d)\n", strerror(errno), errno);
    return -1;                                                               
  }

  /* connect on rfcomm */
  memset(&rem_addr, 0, sizeof(rem_addr));
  rem_addr.rc_family = AF_BLUETOOTH;
  rem_addr.rc_bdaddr = *bdaddr;
  rem_addr.rc_channel = channel;
  if( connect(bt, (struct sockaddr *)&rem_addr, sizeof(rem_addr)) < 0 ){
    fprintf(stderr, "Can't connect. %s(%d)\n", strerror(errno), errno);
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

bdaddr_t *inquiry() {
  inquiry_info *info = NULL;
  uint8_t lap[3] = { 0x33, 0x8b, 0x9e };
  int num_rsp, i;
  bdaddr_t *result = NULL;
  int dev_id = hci_get_route(NULL);

  int sock = hci_open_dev( dev_id );
  if (dev_id < 0 || sock < 0) {
    perror("opening socket");
    return NULL;
  }

  num_rsp = hci_inquiry(-1, 8, 0, lap, &info, 0);
  if (num_rsp < 0) {
    perror("Inquiry failed.");
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

  if(pkt[1] != 55)
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

int main(int argc, char **argv) {
  int bt;
  bdaddr_t *bdaddr = NULL;

  bdaddr = inquiry();
  if(!bdaddr) {
    printf("not found\n");
    return 0;
  }

  printf("HXM device found ...\n");

  bt = bluez_connect(bdaddr, 1);

  while(1) {
    hxm_t hxm;
    char buf[HXM_PACKET_SIZE];
    int size = read_num(bt, buf, sizeof(buf));

    hexdump(buf, size);

    /* check frame format */
    if(check_frame(buf) != 0) {
      printf("invalid frame, re-sync\n");
      
      /* try to re-sync */
      /* the 60 bytes should somewhere contain the ETX followed by */
      /* the STX marker. Skip HXM_PACKET_SIZE bytes after STX */
      int i;
      for(i=0;i<(HXM_PACKET_SIZE-1) &&
	    (buf[i] != HXM_ETX || buf[i+1] != HXM_STX) ;i++);
      
      read_num(bt, buf, i+1);
    } else {
      // fill hxm data structure
      hxm.bat   = 0xff & buf[11];
      hxm.hr    = 0xff & buf[12];
      hxm.hrno  = 0xff & buf[13];
      hxm.dist  = *((short*)(buf+50)) / 16.0;
      hxm.speed = *((short*)(buf+52)) / 256.0;
      hxm.steps = *((short*)(buf+54));
      hxm.cad   = *((short*)(buf+56)) / 256.0;

      printf("bat   = %d\n", hxm.bat);
      printf("hr    = %d\n", hxm.hr);
      printf("hrno  = %u\n", hxm.hrno);
      printf("dist  = %f\n", hxm.dist);
      printf("speed = %f\n", hxm.speed);
      printf("steps = %u\n", hxm.steps);
      printf("cad   = %f\n", hxm.cad);
	     
    }
  }
  

  free(bdaddr);
  close(bt);
}
