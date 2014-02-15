#ifndef _OSC_H_
#define _OSC_H_

extern void osc_init(const char *hostname, int port);
extern void osc_send(const char *msg, float amt);

#endif