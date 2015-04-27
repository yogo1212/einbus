#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
//#include <sys/inotify.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/un.h>
#include <unistd.h>

/* ugly. TODO find out if anything breaks if this is either GNU or POSIX */
#include <libgen.h>
#include <event.h>

#include "libeinbus.h"
#include "libeinbus-event.h"
#include "libeinbus-util.h"

/* TODO ???? */
void *my_alloc(size_t size)
{
    return calloc(1, size);
}


einbus_t *einbus_create(const char *uri)
{
    einbus_service *service = (einbus_service *)my_alloc(sizeof(einbus_service));

    service->chanlist = NULL;
    service->event = NULL;

    if (mksocketpath(uri)) {
        return NULL;
    }

    if ((service->fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        return NULL;
    }

    fcntl(service->fd, F_SETFD, fcntl(service->fd, F_GETFD) | FD_CLOEXEC);

    service->local.sun_family = AF_UNIX;
    strcpy(service->local.sun_path, uri);
    unlink(service->local.sun_path); //FIXME: find better way

    if (bind(service->fd, (struct sockaddr *) & (service->local),
                strlen(service->local.sun_path) + sizeof(service->local.sun_family)) < 0) {
        free(service);
        return NULL;
    }

    if (listen(service->fd, 5) < 0) {
        free(service);
        return NULL;
    }

    fcntl(service->fd, F_SETFL, fcntl(service->fd, F_GETFL) | O_NONBLOCK);

    return service;
}

int einbus_fd(einbus_t *s)
{
    return ((einbus_service *)(s))->fd;
}

void einbus_new_connection_event(evutil_socket_t fd, short events, void *userdata)
{
    einbus_service *service = (einbus_service *) userdata;

    einbus_channel *chan = einbus_accept(service);

    if (!chan) {
        fprintf(stderr, "libeinbus: error while accepting socket!");
        event_free(service->event);
        service->event = NULL;
        return;
    }

    service->new_connection_cb(chan, service->userdata);
}

void einbus_add_accept_event(einbus_t *s, struct event_base *base, einbus_chan_event_cb_t cb, void *userdata)
{
    einbus_service *service = (einbus_service *) s;

    if (service->event) {
        fprintf(stderr, "service already has event attached!");
        event_free(service->event);
    }

#ifdef EINBUS_DEBUG
    fprintf(stderr, "accepting on %p\n", (void *) service);
#endif

    service->userdata = userdata;
    service->new_connection_cb = cb;
    service->event = event_new(base, einbus_fd(service), EV_READ | EV_PERSIST, einbus_new_connection_event, service);
    event_add(service->event, NULL);
}

einbus_t *einbus_accept(einbus_t *s)
{
    einbus_service *server = (einbus_service *)(s);
    einbus_channel *chan = my_alloc(sizeof(einbus_channel));
    unsigned int t;

    chan->status = EINBUS_CONNECTED;
    //    chan->inotify = 0;
    chan->service = server;
    chan->next = NULL;
    chan->prev = NULL;
    chan->activated = 0;
    chan->isnew = 1;
    t = sizeof(chan->remote);
    chan->fd = accept(server->fd, (struct sockaddr *) & (chan->remote), &t);

    if (chan->fd == -1) {
        free(chan);
        chan = NULL;
    }
    else {
        fcntl(chan->fd, F_SETFD, fcntl(chan->fd, F_GETFD) | FD_CLOEXEC);
        fcntl(chan->fd, F_SETFL, fcntl(chan->fd, F_GETFL) | O_NONBLOCK);
        einbus_client_add(server, chan);
    }

    return chan;
}

void einbus_destroy(einbus_t *s)
{
    einbus_service *server = (einbus_service *)(s);
    einbus_channel *e;

    for (e = server->chanlist; e; e = einbus_client_del(server, e));

    if (server->event)
        event_free(server->event);

    close(server->fd);
    unlink(server->local.sun_path);
    free(server);
}

//high level bus api
int einbus_broadcast(einbus_t *s, const void *buff, int len)
{
    return einbus_broadcast_without(s, buff, len, 0);
}

int einbus_broadcast_without(einbus_t *s, const void *buff, int len, einbus_chan_t *exclude)
{
    einbus_channel *e;

    for (e = ((einbus_service *)s)->chanlist; e; e = e->next) {
        if (e != exclude) {
            einbus_write(e, buff, len);
        }
    }

    return len;
}

einbus_chan_t *einbus_ready_chan(einbus_t *s)
{
    einbus_channel *e;

    for (e = ((einbus_service *)s)->chanlist; e; e = e->next) {
        if (e->activated) {
            e->activated = 0;
            return e;
        }
    }

    return NULL;
}

einbus_chan_t *einbus_fresh_chan(einbus_t *s)
{
    einbus_channel *e = ((einbus_service *)s)->chanlist;

    while (e) {
        if (e->isnew) {
            e->isnew = 0;
            return e;
        }

        e = e->next;
    }

    return NULL;
}

int einbus_select_all(einbus_t *s, fd_set *fds)
{
    einbus_service *server = (einbus_service *)(s);
    einbus_channel *e;
    int fd = einbus_fd(server);
    int max = fd;

    FD_SET(fd, fds);

    for (e = server->chanlist; e; e = e->next) {
        fd = einbus_chan_fd(e);
        FD_SET(fd, fds);

        if (fd > max) {
            max = fd;
        }
    }

    return max;
}

void einbus_activate_all(einbus_t *s, fd_set *fds, int flags)
{
    einbus_service *server = (einbus_service *)(s);
    einbus_channel *e;

    for (e = server->chanlist; e;) {
        if (FD_ISSET(einbus_chan_fd(e), fds)) {
            einbus_activate(e);

            if (flags & EINBUS_IGNORE_INBOUND) {
                static char ignored_buff[1000];
                int ret = einbus_read(e, &ignored_buff, 1000);

                if (ret < 1) {
                    if (ret < 0 && errno == EAGAIN) {
                        // fd was set for no reason :(
                        e = e->next;
                        continue;
                    }

                    einbus_channel *e2 = e;
                    e = e->next;
                    einbus_disconnect(e2);
                    continue;
                }
            }
            else if (e->status == EINBUS_READY) {
                e->activated = 1;
            }
        }

        e = e->next;
    }

    if (FD_ISSET(einbus_fd(server), fds)) {
        einbus_accept(server);
    }
}

//chan api
einbus_chan_t *einbus_connect(const char *uri)
{
    einbus_channel *chan = my_alloc(sizeof(einbus_channel));

    if (chan == NULL) {
        return NULL;
    }

    chan->status = EINBUS_INIT;
    //chan->inotify = 0;
    chan->fd = 0;
    chan->service = 0;
    chan->remote.sun_family = AF_UNIX;
    strcpy(chan->remote.sun_path, uri);

    return (einbus_chan_t *)chan;
}

EINBUS_STATUS einbus_status(einbus_chan_t *s)
{
    return ((einbus_channel *)s)->status;
}

int einbus_chan_fd(einbus_chan_t *s)
{
    einbus_channel *chan = (einbus_channel *)(s);

    //    return (chan->inotify) ? chan->inotify : chan->fd;
    return chan->fd;
}

static int do_connect(einbus_channel *chan)
{
    int len;

    if ((chan->fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        chan->status = EINBUS_ERROR;
        return 1;
    }

    fcntl(chan->fd, F_SETFL, fcntl(chan->fd, F_GETFL) | O_NONBLOCK);
    len = strlen(chan->remote.sun_path) + sizeof(chan->remote.sun_family);

    if (connect(chan->fd, (struct sockaddr *) & (chan->remote), len) < 0) {
        chan->status = EINBUS_ERROR;
        return 1;
    }

    chan->status = EINBUS_CONNECTED;
    return 0;
}

EINBUS_STATUS einbus_activate(einbus_chan_t *s)
{
    einbus_channel *chan = (einbus_channel *)(s);
    struct stat st;

    if (chan->status == EINBUS_INIT) {
        //TODO: exists should stat the entire path.
        //      sometimes the stat failes because we cant read a dir
        int exists = (stat(chan->remote.sun_path, &st) > -1);

        if (exists && !do_connect(chan)) {
            /*
               if (chan->inotify) {
               close(chan->inotify);
               }

               chan->inotify = 0;
             */
            /*
               } else if (!exists) {
               chan->inotify = inotify_init();
               fcntl(chan->inotify, F_SETFL, fcntl(chan->inotify, F_GETFL) | O_NONBLOCK);
               tmp = my_alloc(strlen(chan->remote.sun_path));
               strcpy(tmp, chan->remote.sun_path);
               if (inotify_add_watch(chan->inotify, dirname(tmp), IN_CREATE) < 0)
               chan->status = EINBUS_ERROR;
               free(tmp);
               chan->status = EINBUS_LURKING;
             */
        }
        else {
            chan->status = EINBUS_ERROR;
        }
    }
    /*
       else if (chan->status == EINBUS_LURKING) {
       char buff[2000];
       int exists = (stat(chan->remote.sun_path, &st) > -1);
       read(chan->inotify, &buff, 2000);

       if (exists) {
       if (!do_connect(chan)) {
       close(chan->inotify);
       chan->inotify = 0;
       }
       else {
       / *
       FIXME
       (file create && listen) is not atomic
       avoid dead lock for now by waiting.
       this should be solved with flock
     * /
     usleep(100);
     close(chan->inotify);
     chan->inotify = 0;
     do_connect(chan);
     }
     }
     } */
    else if (chan->status == EINBUS_CONNECTED) {
        chan->status = EINBUS_READY;
    }

    if (getenv("EINBUS_DEBUG")) {
        fprintf(stderr, "libeinbus: s_%i\n", chan->status);
    }

    return chan->status;
}

int einbus_write(einbus_chan_t *s, const void *buff, int len)
{
    einbus_channel *chan = (einbus_channel *)(s);
    int r = send(chan->fd, buff, len, 0);

    fsync(chan->fd);

    return r;
}

int einbus_read(einbus_chan_t *s, void *buff, int len)
{
    einbus_channel *chan = (einbus_channel *)(s);
    int r = recv(chan->fd, buff, len, 0);

    if (r == 0) {
        chan->status = EINBUS_EOF;
    }
    else if (r < 0) {
        chan->status = EINBUS_ERROR;
    }
    else {
        chan->status = EINBUS_CONNECTED;
    }

    return r;
}

void einbus_close(einbus_chan_t *s)
{
    shutdown(((einbus_channel *)s)->fd, SHUT_WR);
}

void einbus_disconnect(einbus_chan_t *s)
{
    einbus_channel *chan = (einbus_channel *)(s);

    if (chan->service) {
        einbus_client_del(chan->service, chan);
    }
    else {
        /*
           if (chan->inotify != 0) {
           close(chan->inotify);
           }
         */

        close(chan->fd);
        free(chan);
    }
}
