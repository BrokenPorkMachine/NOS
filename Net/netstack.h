#ifndef NET_STACK_H
#define NET_STACK_H
#include <stddef.h>
void net_init(void);
int net_send(const void *data, size_t len);
int net_receive(void *buf, size_t buflen);
#endif // NET_STACK_H
