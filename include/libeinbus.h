#ifndef __LIBEINBUS_H__
#define __LIBEINBUS_H__

#define USE_SELECT

#ifdef USE_SELECT
#include <sys/select.h>
#endif

typedef void einbus_t;
typedef void einbus_chan_t;

typedef enum {
    EINBUS_INIT      = 1,
    EINBUS_ERROR     = 2,
    EINBUS_LURKING   = 3,
    EINBUS_CONNECTED = 4,
    EINBUS_READY     = 5,
    EINBUS_EOF       = 6,
    EINBUS_ACCEPTING = 7
}  EINBUS_STATUS;

typedef enum {
    EINBUS_NO_ACTIVATE_FLAGS = 0,
    EINBUS_IGNORE_INBOUND    = 1,
} EINBUS_ACTIVATE_FLAGS;

einbus_t *      einbus_create     (const char *path);
int             einbus_fd         (einbus_t *);
einbus_chan_t * einbus_accept     (einbus_t *);
void            einbus_destroy    (einbus_t *);

int             einbus_broadcast  (einbus_t *, const void *buff, int len);
int             einbus_broadcast_without  (einbus_t *, const void *buff, int len, einbus_chan_t *exclude);
einbus_chan_t * einbus_ready_chan (einbus_t *);
einbus_chan_t * einbus_fresh_chan (einbus_t *);

#ifdef USE_SELECT
/* adds all FDs attached to the einbus-instance to the fd_set */
int             einbus_select_all     (einbus_t *, fd_set *);
void            einbus_activate_all   (einbus_t *, fd_set *, int flags);
#endif

einbus_chan_t * einbus_connect    (const char * path);
EINBUS_STATUS   einbus_status     (einbus_chan_t *);
int             einbus_chan_fd    (einbus_chan_t *);
EINBUS_STATUS   einbus_activate   (einbus_chan_t *);
int             einbus_write      (einbus_chan_t *, const void *buff, int len);
int             einbus_read       (einbus_chan_t *, void *buff, int len);
void            einbus_close      (einbus_chan_t *);
void            einbus_disconnect (einbus_chan_t *);

#endif
