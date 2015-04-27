#ifndef __LIBEINBUS_UTIL_H__
#define __LIBEINBUS_UTIL_H__

#include <sys/un.h>

#include "libeinbus.h"
#include "libeinbus-event.h"

typedef struct einbus_channel einbus_channel;
typedef struct einbus_service einbus_service;
struct einbus_service {
    int fd;
    einbus_channel *chanlist;
    struct sockaddr_un local;

    struct event *event;
    einbus_chan_event_cb_t new_connection_cb;
    void *userdata;
};

struct einbus_channel {
    int fd;
    EINBUS_STATUS status;
    //int inotify;
    einbus_service *service;
    struct sockaddr_un remote;

    // todo: save bytes, make those flags
    int activated;
    int isnew;

    struct einbus_channel *next;
    struct einbus_channel *prev;
};

int mkpath(const char *s, mode_t mode);
int mksocketpath(const char *s);

/* add c to a service */
einbus_channel *einbus_client_add(einbus_service *service, einbus_channel *c);

/* remove c from service */
einbus_channel *einbus_client_del(einbus_service *service, einbus_channel *c);

/* this removes c from the list pointed to by head. Head changes, if the first element is deleted. */
einbus_channel *einbus_client_del_raw(einbus_channel **head, einbus_channel *c);

#endif
