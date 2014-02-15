
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>


char osc_ip[100] = "127.0.0.1";
int osc_port = 8003;
int osc_socket;
struct sockaddr_in si_me;

void osc_init(const char *hostname, int port) {
    strcpy(osc_ip, hostname);
    osc_port = port;
    printf("OSC: Using host \"%s\", port %d\n", osc_ip, osc_port);
    osc_socket = socket(AF_INET, SOCK_DGRAM, 0);
    memset((char *) &si_me, 0, sizeof(si_me));
    si_me.sin_family = AF_INET;
    si_me.sin_port = htons(osc_port);
    si_me.sin_addr.s_addr = htonl(INADDR_ANY);
    if (inet_aton(osc_ip, &si_me.sin_addr)==0) {
        printf("OSC: Can't resolve ip.\n");
    }
}


void osc_send(const char *msg, float amt) {
    // printf("OSC: Sending message \"%s\" with value %1.1f\n", msg, amt);

    unsigned char msgbuf[100];
    memset(msgbuf, 0, 100);

    // strcpy((char *)&msgbuf, "hej\n");

    int o = 0;

    memcpy(msgbuf+o, msg, strlen(msg));
    o += strlen(msg);

    o += 4 - (o % 4);

    char *typestring = (char *)",f";

    memcpy(msgbuf+o, typestring, strlen(typestring));
    o += strlen(typestring);

    o += 4 - (o % 4);

    char minibuf[4];
    memcpy((char *)&minibuf, (unsigned char *)&amt, 4);

    msgbuf[o] = minibuf[3];
    msgbuf[o+1] = minibuf[2];
    msgbuf[o+2] = minibuf[1];
    msgbuf[o+3] = minibuf[0];
    // memcpy(msgbuf+o, (unsigned char *)&amt, 4);
    o += 4;

    // o += 4 - (o % 4);

    /*
    for(int i=0; i<o; i++) {
        char c = msgbuf[i];
        printf("%02X ", c);
    }
    printf("\n");

    for(int i=0; i<o; i++) {
        char c = msgbuf[i];
        if (c > 32 && c < 128)
            printf("%C  ", c);
        else
            printf("?  ");
    }
    printf("\n");
    */

    if (sendto(osc_socket, (const char *)msgbuf, o, 0, (sockaddr*)&si_me, sizeof(si_me)) == -1) {
        printf("OSC: Failed to send.\n");
    }
}