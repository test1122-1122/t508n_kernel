#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/tty.h>

#ifndef N_HCI
#define N_HCI 15
#endif
#ifndef HCIUARTSETPROTO
#define HCIUARTSETPROTO _IOW('U', 200, int)
#endif
#define HCI_UART_H4 0

static void usage(const char *p){ fprintf(stderr,"usage: %s [tty]\n",p); }

int main(int argc,char **argv){
    const char *dev = (argc>1)?argv[1]:"/dev/ttyBT0";
    int fd = open(dev, O_RDWR|O_NOCTTY|O_NONBLOCK);
    if(fd<0){ fprintf(stderr,"open %s: %s\n",dev,strerror(errno)); return 1; }
    int ldisc = N_HCI;
    if(ioctl(fd,TIOCSETD,&ldisc)<0){ fprintf(stderr,"TIOCSETD: %s\n",strerror(errno)); return 2; }
    if(ioctl(fd,HCIUARTSETPROTO,(unsigned long)HCI_UART_H4)<0){ fprintf(stderr,"HCIUARTSETPROTO: %s\n",strerror(errno)); return 3; }
    printf("attached %s as H4\n",dev); fflush(stdout);
    for(;;) pause();
    return 0;
}
