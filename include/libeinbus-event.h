#ifndef __LIBEINBUS_EVENT_H__
#define __LIBEINBUS_EVENT_H__


#include "libeinbus.h"

#include <event.h>

/*
 * no callback for closed channels. generall rules for programming with events apply
 * (e.g. read-event with size 0 is a good time to close event)
 */
typedef void (*einbus_chan_event_cb_t)(einbus_chan_t *chan, void *userdata);
/* add event to base that handles new connections and calls cb when that happens */
void            einbus_add_accept_event(einbus_t *service, struct event_base *base, einbus_chan_event_cb_t cb, void *userdata);

#endif
