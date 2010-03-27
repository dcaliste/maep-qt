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

#define HXM_CLASS  0x001f00
#define HXM_VENDOR 0x000780

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

  num_rsp = hci_inquiry(-1, 8, 0, lap, &info, 0);
  if (num_rsp < 0) {
    perror("Inquiry failed.");
    return NULL;
  }

  for (i = 0; i < num_rsp; i++) {
    if((info+i)->dev_class[0] == ((HXM_CLASS >> 0) & 0xff) &&
       (info+i)->dev_class[1] == ((HXM_CLASS >> 8) & 0xff) &&
       (info+i)->dev_class[2] == ((HXM_CLASS >> 16) & 0xff) &&
       (info+i)->bdaddr.b[3] == ((HXM_VENDOR >> 0) & 0xff) &&
       (info+i)->bdaddr.b[4] == ((HXM_VENDOR >> 8) & 0xff) &&
       (info+i)->bdaddr.b[5] == ((HXM_VENDOR >> 16) & 0xff) 
       ) {
      result = malloc(sizeof(bdaddr_t));
      *result = (info+i)->bdaddr;
      return result;
    }
  }

  return result;
}

int main(int argc, char **argv) {
  int bt;
  bdaddr_t *bdaddr = NULL;

  bdaddr = inquiry();
  if(!bdaddr) {
    printf("not found\n");
    return 0;
  }

  char addr[18];
  ba2str(bdaddr, addr);
  printf("hxm device address %s\n", addr);

  bt = bluez_connect(bdaddr, 1);
  while(1) {
    char buf[HXM_PACKET_SIZE];
    int size = read_num(bt, buf, sizeof(buf));

    printf("got %d\n", size);
    hexdump(buf, size);

    printf("hr = %d\n", buf[12]);
  }
  

  free(bdaddr);
  close(bt);
}
