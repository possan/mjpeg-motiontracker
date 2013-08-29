#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <arpa/inet.h>

char osc_ip[100] = "127.0.0.1";
int osc_port = 8000;
int osc_socket;
struct sockaddr_in si_me;

void osc_init() {
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

void osc_generate(char *msg, float amt) {
    unsigned char msgbuf[100];
    memset(msgbuf, 0, 100);

    int o = 0;

    memcpy(msgbuf+o, msg, strlen(msg));
    o += strlen(msg);

    o += 4 - (o % 4);

    char *typestring = ",f";

    memcpy(msgbuf+o, typestring, strlen(typestring));
    o += strlen(typestring);

    o += 4 - (o % 4);

    char minibuf[4];
    memcpy((char *)&minibuf, (unsigned char *)&amt, 4);

    msgbuf[o] = minibuf[3];
    msgbuf[o+1] = minibuf[2];
    msgbuf[o+2] = minibuf[1];
    msgbuf[o+3] = minibuf[0];
    o += 4;

    if (sendto(osc_socket, (const char *)msgbuf, o, 0, (sockaddr*)&si_me, sizeof(si_me)) == -1) {
        printf("OSC: Failed to send.\n");
    }
    /*
    for(int i=0; i<o; i++) {
        char c = msgbuf[i];
        printf("%c", c);
    }
    */
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("Syntax: mm-msg [ip] [port] [string] [float-value]\n");
        printf("Example: mm-msg 127.0.0.1 8000 /motion/1 150.0\n");
        return 1;
    }

    strcpy(osc_ip, argv[1]);
    osc_port = atoi(argv[2]);

    osc_init();
    osc_generate(argv[3], atof(argv[4]));
    return 0;
}
